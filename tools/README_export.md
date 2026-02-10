# NN 앱 프레임 export (공유 메모리 + 영상 녹화)

## 공유 메모리 (`/nn_frame_export`)

NN 앱은 그리기된 출력 프레임을 **공유 메모리**에 매 프레임 복사합니다.  
OpenCV 등 다른 프로세스는 이 공유 메모리를 읽어 표시하거나 `VideoWriter`로 저장할 수 있습니다.

### 레이아웃 (NN 앱과 동일)

- **헤더 32바이트**: magic(4), version(4), width(4), height(4), stride(4), write_slot(4), frame_index(8)
- **버퍼 0**: `width * height * 3` (RGB)
- **버퍼 1**: 동일
- NN이 쓰는 쪽: `write_slot`이 가리키는 버퍼에 쓰고, 쓴 뒤 `write_slot = 1 - write_slot`, `frame_index++`
- **읽는 쪽**: `1 - write_slot` 버퍼가 “방금 쓴 완성 프레임”이므로 그 버퍼를 읽으면 됨.

### OpenCV 쪽 예제

- **실시간 표시만**: `python3 read_nn_shm_and_record.py`
- **영상 파일로 저장**: `python3 read_nn_shm_and_record.py -o outputs/from_shm.avi`
- NN 앱을 먼저 실행한 뒤, 같은 머신에서 위 스크립트 실행 (Linux: `/dev/shm/nn_frame_export` 사용).

## NN 앱 내 비동기 영상 녹화 (`-R`)

- 실행 시 **`-R`** 옵션을 주면, NN 앱이 **비동기 스레드**에서 `outputs/record.avi` (MJPEG)로 녹화합니다.
- 메인 루프는 블로킹되지 않고, 프레임만 큐에 넣고 다음 프레임으로 진행합니다.

예: `./tcnnapp -o n -R`
