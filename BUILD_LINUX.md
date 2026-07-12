# Linux build and soak test

Linux/CMake가 서버 배포와 장시간 검증의 기준입니다. Windows의 기존 `.sln`, `.lib`,
`DummyClient` 산출물을 Linux 서버 검증 결과로 대체하지 않습니다.

## Ubuntu 24.04 / WSL 준비

```bash
sudo apt update
sudo apt install -y build-essential g++-14 cmake ninja-build libboost-dev \
  python3 gdb binutils procps iproute2
```

저장소에 Git 제외 상태의 `include/boost`가 있으면 이를 우선 사용하고, 없으면 시스템
Boost를 사용합니다. Docker build는 `.dockerignore`로 로컬 Boost를 제외해 Ubuntu의
시스템 Boost를 사용합니다.

## 빌드와 짧은 회귀 테스트

```bash
cmake -S . -B build/linux -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_COMPILER=g++-14 \
  -DBUILD_TESTING=ON

cmake --build build/linux --parallel
ctest --test-dir build/linux --output-on-failure
```

생성되는 주요 실행 파일은 다음과 같습니다.

```text
build/linux/PlayServer/PlayServer
build/linux/SoakClient/SoakClient
build/linux/SoakClient/ProtocolProbe
```

## 자동 Linux soak

`run_soak.sh`는 다음 순서를 자동화합니다.

1. `RelWithDebInfo` 빌드와 CTest 4종
2. disposable 서버에 실제 TCP fragmentation/coalescing/under/over/malformed/
   slowloris preflight
3. 새 서버 프로세스 + 장시간 봇 실행
4. server/client CSV, RSS/PSS/private/FD/socket 표본, 로그와 재현 환경 보존
5. ACK/TPS/RTT/session/memory 판정과 JSON report 생성

기본값은 1,000명, 봇당 5 request/s, 24시간입니다.

```bash
bash scripts/run_soak.sh
```

`DURATION_SEC=0`을 지정하면 봇은 시간 제한 없이 실행되며 `SIGINT` 또는 `SIGTERM`을
받았을 때 진행 중인 요청의 ACK를 timeout까지 기다리고 최종 CSV 표본을 기록한 뒤
종료합니다. 직접 실행한 `SoakClient`는 `Ctrl+C`로 중지할 수 있지만, orchestration
스크립트는 loadbot만 먼저 정상 종료해야 후속 서버 종료와 분석 단계가 계속됩니다.
강제 종료하면 최종 표본이 남지 않을 수 있습니다. 무제한 설정 자체가 24시간 합격을
뜻하지는 않으며, analyzer는 실제 기록된 실행 시간으로 자격을 판정합니다. CSV와
console/Compose 로그는 계속 증가하므로 장기 무제한 실행에서는 artifact 디스크
사용량과 로그 보존 정책도 함께 관리합니다.

```bash
RUN_ID=unlimited-$(date -u +%Y%m%dT%H%M%SZ) DURATION_SEC=0 \
bash scripts/run_soak.sh
```

`run_soak.sh`를 다른 터미널에서 정상 종료할 때는 `pgrep -a -x SoakClient`로 이 run의
PID를 확인한 뒤 `kill -TERM <pid>`로 loadbot만 먼저 중지합니다.

짧은 smoke 예시는 다음과 같습니다. 이는 동작 확인일 뿐 24시간 자격을 만족하지
않습니다.

```bash
RUN_ID=smoke-$(date -u +%Y%m%dT%H%M%SZ) \
BOT_COUNT=100 DURATION_SEC=30 RAMP_UP_SEC=2 WARMUP_SEC=5 \
RECONNECT_PERCENT=10 RECONNECT_INTERVAL_SEC=10 \
bash scripts/run_soak.sh
```

결과는 기본적으로 `artifacts/<run_id>/`에 저장됩니다. analyzer의 일반 `PASS`와
`report.json`의 `qualification_24h_1000_clients.qualified`를 구분해서 확인합니다.

## Docker soak

Docker daemon이 실행 중인 Linux/Docker Desktop 환경에서:

```bash
bash scripts/run_docker_soak.sh
```

Docker에서도 `DURATION_SEC=0`의 의미는 같습니다. `run_docker_soak.sh`를 무제한으로
실행했다면 다른 터미널에서 다음 명령으로 loadbot만 먼저 중지합니다. loadbot이
종료되면 원래 스크립트가 서버를 내리고 report를 생성합니다.

```bash
docker compose -f compose.soak.yml stop -t 30 loadbot
```

설정은 환경변수로 바꿀 수 있습니다.

```bash
RUN_ID=soak-$(date -u +%Y%m%dT%H%M%SZ) \
BOT_COUNT=1000 PPS_PER_BOT=5 DURATION_SEC=86400 RAMP_UP_SEC=60 \
SOAK_SEED=20260712 \
bash scripts/run_docker_soak.sh
```

Compose는 `nofile=65536`, `core=unlimited`, `MALLOC_ARENA_MAX=2`를 설정합니다.
다만 container의 core ulimit만으로 host의 `kernel.core_pattern`이 바뀌지는 않습니다.
실행 전 host의 core/coredump 정책을 확인해야 합니다.

## 수동 실행

터미널 1:

```bash
./build/linux/PlayServer/PlayServer \
  --port 7777 \
  --metrics-file artifacts/server.csv \
  --run-id manual
```

터미널 2에서 경계/비정상 패킷 preflight:

```bash
./build/linux/SoakClient/ProtocolProbe --host 127.0.0.1 --port 7777
```

Malformed preflight는 예상된 `protocol_errors`를 만들므로, healthy soak는 서버를 새로
시작한 뒤 실행합니다.

```bash
./build/linux/SoakClient/SoakClient \
  --host 127.0.0.1 --port 7777 \
  --bots 1000 --duration-sec 86400 --ramp-up-sec 60 --pps 5 \
  --reconnect-percent 5 --reconnect-interval-sec 60 \
  --payload-sizes 1,32,128,512,1024,4096,6400 \
  --metrics-file artifacts/client.csv --run-id manual --seed 20260712
```

## Sanitizer

ASan/LSan과 TSan은 서로 다른 build directory에서 실행하며 동시에 켜지 않습니다.

```bash
cmake -S . -B build/linux-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-14 \
  -DBUILD_TESTING=ON -DMMO_ENABLE_ASAN=ON
cmake --build build/linux-asan --parallel
ASAN_OPTIONS=halt_on_error=1:detect_leaks=1 \
  ctest --test-dir build/linux-asan --output-on-failure

cmake -S . -B build/linux-tsan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-14 \
  -DBUILD_TESTING=ON -DMMO_ENABLE_TSAN=ON
cmake --build build/linux-tsan --parallel
TSAN_OPTIONS=halt_on_error=1:exitcode=66 \
  ctest --test-dir build/linux-tsan --output-on-failure
```

WSL2에서 TSan이 코드 report 없이 `unexpected memory mapping`으로 시작 실패할 때만
ASLR을 끈 wrapper를 사용합니다.

```bash
setarch x86_64 -R env \
  TSAN_OPTIONS=halt_on_error=1:exitcode=66:history_size=7 \
  ctest --test-dir build/linux-tsan --output-on-failure
```

Boost.Asio의 `atomic_thread_fence`에 대해 GCC TSan 경고가 날 수 있어 fence 기반
동기화에는 false negative 가능성이 있습니다. TSan 통과 하나만으로 race 부재를
확정하지 않습니다. ASan의 거대한 shadow address space 때문에 sanitizer run의
`VmSize`도 누수 지표로 사용하지 않습니다.

Windows 전용 custom allocator는 128-bit CAS/주소 배치 가정의 Linux 이식성 테스트가
생길 때까지 Linux 서버 target에서 제외합니다.

## Crash/core 분석

```bash
ulimit -c unlimited
cat /proc/sys/kernel/core_pattern
bash scripts/analyze_core.sh \
  /absolute/path/to/exact/PlayServer \
  /absolute/path/to/core.PlayServer \
  artifacts/<run_id>/gdb-report.txt
```

동일 binary/build-id가 아닌 파일로 core를 열지 않습니다. GDB 보고서를 검토한 뒤
확인된 사실과 가설을 분리해 `troubleshoot.md` 끝에 incident를 append합니다.
