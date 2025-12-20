#!/usr/bin/env python3
import mmap
import signal
import struct
import sys
import time

import posix_ipc  # pip install posix_ipc

SHM_NAME = "/hpc_shm_spsc_ring"

# Keep in sync with the C++ Message struct
MESSAGE_STRUCT = struct.Struct("<QQ48s")  # seq (u64), ts (u64), payload[48]
MESSAGE_SIZE = MESSAGE_STRUCT.size

# Header layout: capacity (u64), head (u64), tail (u64)
HEADER_STRUCT = struct.Struct("<QQQ")
HEADER_SIZE = HEADER_STRUCT.size

stop = False


def handle_signal(signum, frame):
    global stop
    stop = True


for sig in (signal.SIGINT, signal.SIGTERM):
    signal.signal(sig, handle_signal)


def main() -> int:
    try:
        shm = posix_ipc.SharedMemory(SHM_NAME, flags=0)  # open existing
    except posix_ipc.ExistentialError:
        print(f"Shared memory object {SHM_NAME} not found (is the publisher running?).")
        return 1

    # First, map just the header to read capacity.
    mm_header = mmap.mmap(shm.fd, HEADER_SIZE, access=mmap.ACCESS_READ)
    raw_header = mm_header[:HEADER_SIZE]
    capacity, head_idx, tail_idx = HEADER_STRUCT.unpack(raw_header)
    mm_header.close()

    total_size = HEADER_SIZE + capacity * MESSAGE_SIZE

    # Remap the full region now that we know the capacity/size.
    mm = mmap.mmap(shm.fd, total_size, access=mmap.ACCESS_READ)
    shm.close_fd()

    max_messages = 20
    count = 0

    try:
        while not stop and count < max_messages:
            # Reload header each iteration to see updated head/tail.
            raw_header = mm[0:HEADER_SIZE]
            capacity, head_idx, tail_idx = HEADER_STRUCT.unpack(raw_header)

            if head_idx == tail_idx:
                # Queue is empty; sleep briefly and retry.
                time.sleep(0.001)
                continue

            idx = head_idx % capacity
            offset = HEADER_SIZE + idx * MESSAGE_SIZE
            raw = mm[offset : offset + MESSAGE_SIZE]
            if len(raw) != MESSAGE_SIZE:
                time.sleep(0.001)
                continue

            seq, ts_ns, payload = MESSAGE_STRUCT.unpack(raw)

            print(f"seq={seq} ts={ts_ns} payload[0:4]={payload[:4].hex()} (head={head_idx} tail={tail_idx})")
            count += 1
            time.sleep(0.01)
    finally:
        mm.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())

