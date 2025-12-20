# Shared-memory IPC & Heterogeneous Compute

This document explains how the shared-memory SPSC ring buffer in this
repository can be used as the backbone of a **C++ → Python/GPU** data
pipeline, and sketches extensions towards multi-subscriber IPC.

## 1. Layout guarantees

`hpc::ipc::shm_spsc_ring_buffer<T>` places a small control header followed by
an array of `T` elements into a POSIX shared memory object created by
`shm_open` + `mmap`.

The in-memory layout is intentionally simple:

```text
+----------------------+  offset 0
| control header       |  (size rounded to cache line)
+----------------------+  offset HEADER_SIZE
| slot[0] (T)          |
+----------------------+
| slot[1] (T)          |
+----------------------+
| ...                  |
+----------------------+
| slot[N-1] (T)        |
+----------------------+  offset HEADER_SIZE + N * sizeof(T)
```

Key properties:

- The header contains the producer and consumer indices (SPSC) and capacity.
- Slots are trivially indexable as `base + HEADER_SIZE + i * sizeof(T)`.
- The ring buffer forces its capacity to a power-of-two, so index wrap-around
  is implemented with `index & (capacity - 1)` rather than modulo.

For heterogeneous consumers (Python, CUDA, etc.) the important guarantees are:

- `T` has a fixed, known layout (POD / standard-layout struct).
- No internal pointers or ownership; all fields are plain integers/floats.
- Endianness is little-endian (matching x86_64).

## 2. Example: C++ publisher → Python subscriber

Under `examples/`:

- `shm_publisher.cpp`: C++ publisher that owns the shared memory region and
  pushes fixed-size `Message` structs into the ring.
- `shm_subscriber.py`: Python process that mmaps the same shared memory object
  and decodes messages with `struct.Struct`.

### 2.1 C++ message definition

```c++
struct Message {
    std::uint64_t seq;
    std::uint64_t timestamp_ns;
    std::uint8_t  payload[48];
};
```

This is mirrored in Python as:

```python
MESSAGE_STRUCT = struct.Struct("<QQ48s")  # seq, ts_ns, payload[48]
```

The `"<"` forces little-endian interpretation to match the C++ side.

### 2.2 Backpressure & dropping policy

The publisher example uses a simple backpressure strategy:

- Attempt to `try_push(msg)` into the ring.
- If the ring is full, `try_pop(dropped)` one element (drop oldest) and then
  push again.

This is appropriate for telemetry-style streams where **freshness** matters
more than completeness (e.g., best bid/offer snapshots, model features that
can be recomputed from the latest state).

In a production engine you would likely parameterize this policy:

- **Drop oldest** (as above) for lossy feeds.
- **Drop newest** (reject producer) if you cannot afford to lose history.
- **Block producer** with a timeout if end-to-end latency is still acceptable.

## 3. Multi-subscriber shared-memory design (sketch)

The current `shm_spsc_ring_buffer<T>` is a single-producer / single-consumer
primitive. Extending it to multi-subscriber IPC can be done with minimal
structural changes:

### 3.1 Per-subscriber head indices

Instead of a single consumer head index, maintain:

- One producer tail index `tail` (monotonic, wraps via mask).
- An array of consumer heads `head[i]` for `i in [0, M)` subscribers.

Each consumer operates as an independent SPSC pair with the producer:

- Producer writes into `slot[tail & mask]` and advances `tail`.
- Consumer `i` reads from `slot[head[i] & mask]` and advances `head[i]`.

To ensure bounded memory and well-defined backpressure, the effective
"drain point" is:

```text
min_head = min(head[0..M-1])
```

The producer must not advance more than `capacity` elements beyond `min_head`.

### 3.2 Handling lagging readers

A slow consumer can hold back `min_head` and therefore stall the producer.
There are three main strategies:

1. **Hard backpressure:**
   - If `tail - min_head == capacity`, block the producer or fail `try_push`.
   - Guarantees no message loss but can stall the entire pipeline.

2. **Per-subscriber dropping:**
   - Maintain a per-subscriber `max_lag` and, if `head[i]` falls more than
     `max_lag` behind `tail`, advance `head[i]` forward (drop for that
     subscriber) and record statistics.

3. **Global lossy policy:**
   - If the buffer is full from the producer perspective, advance
     `min_head` by one (drop oldest) and move any `head[i]` that is still
     behind up to `min_head`.

For a HPC system, strategy (2) is typically the most attractive: critical
subscribers (risk, persistence) have large or infinite `max_lag`, whereas
non-critical analytics consumers have tight `max_lag` and accept drops.

### 3.3 Metadata layout

One possible on-wire layout for a multi-subscriber shared ring:

```text
struct ShmHeader {
    std::uint64_t capacity;   // power-of-two
    std::uint64_t tail;       // producer index
    std::uint64_t num_subs;   // number of subscribers (M)
    std::uint64_t head[MaxM]; // per-subscriber heads
    // padding to cache line
};
```

This header is followed by `capacity` slots of `T`, as in the current SPSC
implementation. Subscribers identify themselves either by a fixed index or via
an out-of-band registration handshake.

## 4. PyTorch / GPU integration

Once Python can mmap and decode `Message` objects directly, integrating with
PyTorch is straightforward:

- Expose the payload area as a `numpy.ndarray` or `torch.Tensor` with
  `frombuffer`, using the same dtype and shape as on the C++ side.
- Ensure the lifetime of the underlying shared memory mapping outlives the
  tensor view.

For GPU acceleration, the typical pattern is:

1. C++ publishes feature vectors into the shared-memory ring.
2. Python process reads batches of `Message` structs, builds a batched
   `torch.Tensor` on CPU via zero-copy views when possible.
3. Transfer the batch to GPU (`to(device="cuda")`) and run inference.

The key advantage is that **no JSON/protobuf or heap allocations** occur on
this hot path; both sides agree on a compact, fixed-size struct layout.

