#!/usr/bin/env python3
"""
NN 앱이 공유 메모리(/nn_frame_export)로 내보내는 raw BGR 프레임을 읽어서
화면에 보여주거나, VideoWriter로 영상 파일로 저장하는 예제.
(NN 앱 내부는 RGB; SHM 쓰기 직전에 RGB→BGR 변환하여 OpenCV가 변환 없이 사용.)

사용법:
  # 실시간 표시만
  python3 read_nn_shm_and_record.py

  # 영상 파일로 저장 (outputs/from_shm.avi)
  python3 read_nn_shm_and_record.py -o outputs/from_shm.avi

  # NN 앱과 다른 머신이면 shm 대신 나중에 소켓/파이프로 대체 가능.
"""
import argparse
import struct
import time
import os

try:
    import numpy as np
    import cv2
except ImportError as e:
    print("Need numpy and opencv-python: pip install numpy opencv-python")
    raise SystemExit(1)

# NN 앱과 동일한 레이아웃 (NnAppMain.c nn_export_shm_header_t)
SHM_NAME = "/nn_frame_export"
HEADER_SIZE = 32
MAGIC = 0x4E4E4652  # "NNFR"


def parse_header(data):
    if len(data) < HEADER_SIZE:
        return None
    magic, version, width, height, stride, write_slot, frame_index = struct.unpack(
        "<IIIIIIQ", data[:HEADER_SIZE]
    )
    if magic != MAGIC:
        return None
    return {
        "width": width,
        "height": height,
        "stride": stride,
        "write_slot": write_slot,
        "frame_index": frame_index,
    }


def read_frame_from_shm(mmap_obj, header_info):
    """공유 메모리에서 최신 프레임 한 장 읽기. 읽을 버퍼는 write_slot의 반대."""
    w = header_info["width"]
    h = header_info["height"]
    write_slot = header_info["write_slot"]
    frame_size = w * h * 3
    # NN이 방금 쓴 버퍼 = 1 - write_slot (NN이 쓰고 나서 write_slot을 플립하므로)
    read_slot = 1 - write_slot
    offset = HEADER_SIZE + read_slot * frame_size
    buf = mmap_obj[offset : offset + frame_size]
    arr = np.frombuffer(buf, dtype=np.uint8).reshape((h, w, 3))
    return arr.copy()  # RGB, copy so writer doesn't see changing data


def main():
    ap = argparse.ArgumentParser(description="Read NN export shm and display or record")
    ap.add_argument("-o", "--output", default="", help="Output video path (e.g. outputs/from_shm.avi)")
    ap.add_argument("--fps", type=float, default=30.0, help="FPS for output video")
    args = ap.parse_args()

    import mmap

    # Linux: shm_open("/nn_frame_export") creates /dev/shm/nn_frame_export
    MAX_SHM_SIZE = 1920 * 1080 * 3 * 2 + HEADER_SIZE
    shm_path = "/dev/shm" + SHM_NAME  # /dev/shm/nn_frame_export
    shm_fd = None
    try:
        fd = os.open(shm_path, os.O_RDWR)
        shm_fd = fd
        mm = mmap.mmap(fd, MAX_SHM_SIZE, mmap.MAP_SHARED, mmap.PROT_READ)
    except (OSError, TypeError) as e:
        print("Open shm failed (run NN app first):", e)
        if shm_fd is not None:
            os.close(shm_fd)
        raise SystemExit(1)

    header_bytes = mm.read(HEADER_SIZE)
    info = parse_header(header_bytes)
    if not info:
        print("Invalid header (wrong magic or size)")
        mm.close()
        if shm_fd is not None:
            os.close(shm_fd)
        raise SystemExit(1)

    w, h = info["width"], info["height"]
    frame_size = w * h * 3
    total_size = HEADER_SIZE + 2 * frame_size
    if total_size > MAX_SHM_SIZE:
        print("Shm too small for", w, "x", h)
        mm.close()
        if shm_fd is not None:
            os.close(shm_fd)
        raise SystemExit(1)

    writer = None
    if args.output:
        os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
        fourcc = cv2.VideoWriter_fourcc(*"MJPG")
        writer = cv2.VideoWriter(args.output, fourcc, args.fps, (w, h))
        print("Recording to", args.output)

    last_index = -1
    last_wall_time = time.perf_counter()
    try:
        while True:
            # 헤더 갱신 (매번 최신 write_slot, frame_index 읽기)
            mm.seek(0)
            header_bytes = mm.read(HEADER_SIZE)
            info = parse_header(header_bytes)
            if not info:
                time.sleep(0.001)
                continue
            idx = info["frame_index"]
            if idx == last_index:
                time.sleep(0.001)
                continue
            last_index = idx
            now = time.perf_counter()
            frame = read_frame_from_shm(mm, info)  # BGR from NN app (for display)
            if writer:
                # BGR 프레임을 VideoWriter에 전달
                frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                elapsed = now - last_wall_time
                n = max(1, int(round(elapsed * args.fps)))
                if n > 300:
                    n = 300
                for _ in range(n):
                    writer.write(frame_rgb)
            last_wall_time = now
            cv2.imshow("NN export", frame)
            if cv2.waitKey(1) & 0xFF == ord("q"):
                break
    except KeyboardInterrupt:
        pass
    finally:
        if writer:
            writer.release()
        cv2.destroyAllWindows()
        mm.close()
        try:
            shm.close_fd()
        except Exception:
            pass


if __name__ == "__main__":
    main()
