# MMOServer 장애 분석 기록

이 문서는 장시간 Linux/Docker 검증 중 발생한 crash, hang, OOM, data race 및 심각한
세션/메모리 이상을 증거와 함께 누적하는 append-only 기록이다.

## 현재 확인 상태

- 2026-07-12 KST 기준, 이 문서에 core/dump와 재현 증거를 갖춰 등록된 **확인된
  크래시는 없다**.
- 이는 서버에 결함이 없다는 뜻이 아니다. 1000 동접 24시간 검증의 통과 결과도 아직
  이 문서에 등록되어 있지 않다.
- 분석하지 않은 증상이나 추측 원인을 완료 결과처럼 추가하지 않는다.

## 기록 원칙

- 사고 하나당 아래 템플릿을 문서 끝에 새로 추가한다. 기존 사고 내용을 삭제하거나
  과거 결론을 몰래 고치지 않는다.
- 이후 결론이 바뀌면 같은 Incident ID의 `정정/추가 분석` 항목을 새 날짜로 append한다.
- `확인된 사실`, `가설`, `검증`을 분리한다. 원인은 재현 또는 core/source 증거로
  확인되기 전까지 `미확정`으로 둔다.
- binary/core/log/metrics의 원본 경로와 SHA-256을 기록한다. core 생성 후 다시 빌드한
  실행 파일을 같은 binary라고 취급하지 않는다.
- secret/token/개인정보가 artifact에 있으면 원본 접근을 제한하고, 문서에는 필요한
  최소 정보만 마스킹해 기록한다.

## Linux core 보존과 gdb 기본 절차

프로세스를 시작하는 동일 shell/container에서 미리 core 생성을 허용한다.

```bash
ulimit -c unlimited
file ./PlayServer
sha256sum ./PlayServer
readelf -n ./PlayServer | grep -A1 'Build ID'
ldd ./PlayServer
```

systemd-coredump 환경이면 host에서 해당 사고를 식별한 뒤 보존한다.

```bash
coredumpctl list PlayServer
coredumpctl info <PID-or-incident>
coredumpctl dump <PID-or-incident> --output=core.<run_id>.<pid>
sha256sum core.<run_id>.<pid>
```

container 안의 PID와 host PID, image digest, executable/library mount를 함께 기록한다.
OOM kill은 core가 없을 수 있으므로 `dmesg`, `journalctl`, cgroup의 `memory.events`와
limit/peak도 보존한다.

반드시 core를 만든 동일 실행 파일과 가능한 동일 shared libraries/debug symbols로
연다.

```bash
gdb -q /absolute/path/to/exact/PlayServer /absolute/path/to/core
```

gdb에서 최소 다음을 수집한다.

```gdb
set pagination off
set print thread-events off
info files
info sharedlibrary
info threads
thread apply all bt full
thread apply all info registers
thread <crashing-thread-number>
frame 0
info args
info locals
disassemble /m
```

최적화로 locals가 사라졌다면 임의 값을 만들어 쓰지 않는다. container와 host의
library 경로가 다르면 보존한 rootfs를 기준으로 `set sysroot` 또는
`set solib-search-path`를 설정하고 그 경로도 기록한다. 필요할 때 특정 frame으로
이동해 `list`, `up`, `down`, `p <expression>`을 추가한다.

## 사고별 append-only 템플릿

아래 블록을 복사해 문서 **끝**에 추가한다. 대괄호 필드는 실제 증거로 채우며, 모르면
`미확인`이라고 쓴다.

```markdown
---

## Incident <YYYYMMDD-HHMM-run_id-sequence>

- 상태: 조사 중 | 원인 확인 | 수정 검증 중 | 해결 | 재현 불가
- 최초 발생 시각: <KST와 UTC>
- 작성/갱신 시각: <KST와 UTC>
- run_id: <값>
- 영향: <crash/hang/OOM/race/session leak/memory growth 및 범위>
- signal/exit code: <값 또는 미확인>
- process/container/host PID: <값>

### 실행 환경

- git commit: <SHA>
- dirty diff/patch artifact: <경로와 SHA-256>
- exact binary: <경로, SHA-256, ELF build-id>
- core/dump: <경로와 SHA-256, 없으면 이유>
- compiler/CMake/build type/sanitizer: <값>
- OS/kernel/container image digest: <값>
- CPU/memory/cgroup/ulimit: <값>
- shared libraries/rootfs/debug symbols: <경로 또는 artifact>

### 당시 workload

- server/client 명령과 config: <값 또는 artifact>
- bots/connected sessions: <값>
- 방향별 packet budget와 관측 TPS: <값>
- packet size mix, fragmentation/coalescing: <값>
- reconnect/churn/slowloris/malformed 단계: <값>
- random seed와 마지막 정상 seq/checksum: <값>
- 시작 후 경과 시간: <값>

### 증상과 타임라인

- 마지막 정상 server/client metric: <timestamp와 값>
- 최초 이상 징후: <timestamp와 값>
- 종료/발견 시점: <timestamp와 값>
- kernel/cgroup/OOM 기록: <요약과 artifact>

### 보존 artifact

- server log/metrics: <경로와 SHA-256>
- client log/metrics: <경로와 SHA-256>
- system metrics, fd/ss snapshot: <경로와 SHA-256>
- core, binary, config, source patch: <경로와 SHA-256>

### gdb 증거

- crashing thread/frame: <thread, function, file:line 또는 미확인>
- `info threads` 요약: <값>
- `thread apply all bt full` artifact: <경로>
- 관련 args/locals/registers: <값; optimized out이면 그대로 기록>
- source와 stack이 직접 보여 주는 사실: <값>

### 확인된 사실

- <증거로 확인된 사실만>

### 가설과 검증

- 가설: <미확정 원인>
  - 근거: <값>
  - 반증/부족한 점: <값>
  - 검증 방법과 결과: <값>

### 근본 원인

- 상태: 미확정 | 확인
- 원인: <확인 전에는 미확정>
- 발생 조건과 코드 경로: <값>

### 수정과 검증

- 임시 완화: <값 또는 없음>
- 수정 commit/diff: <값 또는 아직 없음>
- 회귀 테스트: <명령과 결과 artifact>
- ASan/LSan 결과: <별도 run_id와 artifact>
- TSan 결과: <별도 run_id와 artifact>
- 1000명 24h 재검증: <별도 run_id와 artifact; 미실행이면 미실행>
- 잔여 위험/후속 작업: <값>

### 정정/추가 분석 <timestamp, 필요할 때만 append>

- 이전 기록 중 바뀐 내용: <값>
- 새 증거와 결론: <값>
```

<!-- 새 Incident는 이 줄 아래에만 append한다. -->
