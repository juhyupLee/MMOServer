#!/usr/bin/env bash
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-${root}/build/linux-soak}"
run_id="${RUN_ID:-$(date -u +%Y%m%dT%H%M%SZ)}"
artifact_dir="${ARTIFACT_DIR:-${root}/artifacts/${run_id}}"
port="${PORT:-17777}"
bots="${BOT_COUNT:-1000}"
pps="${PPS_PER_BOT:-5}"
duration="${DURATION_SEC:-86400}"
ramp="${RAMP_UP_SEC:-60}"
metrics_interval="${METRICS_INTERVAL_SEC:-5}"
seed="${SOAK_SEED:-20260712}"

if [[ -e "${artifact_dir}" ]]; then
    echo "artifact directory already exists: ${artifact_dir}" >&2
    exit 2
fi
mkdir -p "${artifact_dir}/preflight"

server_pid=""
collector_pid=""
cleanup() {
    if [[ -n "${collector_pid}" ]] && kill -0 "${collector_pid}" 2>/dev/null; then
        kill -TERM "${collector_pid}" 2>/dev/null || true
        wait "${collector_pid}" 2>/dev/null || true
    fi
    if [[ -n "${server_pid}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
        kill -TERM "${server_pid}" 2>/dev/null || true
        wait "${server_pid}" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

ulimit -n 65536 || true
ulimit -c unlimited || true

cmake -S "${root}" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_COMPILER="${CXX:-g++-14}" \
    -DBUILD_TESTING=ON
cmake --build "${build_dir}" --parallel
ctest --test-dir "${build_dir}" --output-on-failure

{
    echo "run_id=${run_id}"
    echo "started_utc=$(date -u --iso-8601=seconds)"
    echo "command=$0"
    echo "bots=${bots}"
    echo "pps_per_bot=${pps}"
    echo "duration_sec=${duration}"
    echo "ramp_up_sec=${ramp}"
    echo "seed=${seed}"
    echo "git_sha=$(git -C "${root}" rev-parse HEAD 2>/dev/null || echo unavailable)"
    echo "git_status_begin"
    git -C "${root}" status --short 2>/dev/null || true
    echo "git_status_end"
    uname -a
    "${CXX:-g++-14}" --version | head -n 1
    cmake --version | head -n 1
    echo "core_ulimit=$(ulimit -c)"
    echo "core_pattern=$(cat /proc/sys/kernel/core_pattern 2>/dev/null || echo unavailable)"
    file "${build_dir}/PlayServer/PlayServer"
    sha256sum "${build_dir}/PlayServer/PlayServer" \
        "${build_dir}/SoakClient/SoakClient" \
        "${build_dir}/SoakClient/ProtocolProbe"
    readelf -n "${build_dir}/PlayServer/PlayServer" | sed -n '/Build ID/ p'
} > "${artifact_dir}/environment.txt"

# Malformed/under/over/slowloris checks run against a disposable server so the
# expected protocol-error counters do not contaminate the healthy soak gate.
"${build_dir}/PlayServer/PlayServer" \
    --port "${port}" \
    --metrics-interval-sec 1 \
    --metrics-file "${artifact_dir}/preflight/server.csv" \
    --run-id "${run_id}-preflight" \
    > "${artifact_dir}/preflight/server.log" 2>&1 &
server_pid=$!
bash "${root}/scripts/wait_for_port.sh" 127.0.0.1 "${port}" 30
"${build_dir}/SoakClient/ProtocolProbe" \
    --host 127.0.0.1 --port "${port}" \
    --timeout-ms 3000 --frame-timeout-wait-ms 12000 \
    | tee "${artifact_dir}/preflight/protocol-probe.log"
kill -TERM "${server_pid}"
set +e
wait "${server_pid}"
preflight_server_status=$?
set -e
server_pid=""
if (( preflight_server_status != 0 )); then
    echo "preflight server exited with status ${preflight_server_status}" >&2
    exit 1
fi

"${build_dir}/PlayServer/PlayServer" \
    --port "${port}" \
    --metrics-interval-sec "${metrics_interval}" \
    --metrics-file "${artifact_dir}/server.csv" \
    --run-id "${run_id}" \
    > "${artifact_dir}/server.log" 2>&1 &
server_pid=$!
bash "${root}/scripts/collect_process_metrics.sh" \
    "${server_pid}" "${artifact_dir}/process.csv" 60 "${port}" &
collector_pid=$!
bash "${root}/scripts/wait_for_port.sh" 127.0.0.1 "${port}" 30

set +e
"${build_dir}/SoakClient/SoakClient" \
    --host 127.0.0.1 --port "${port}" \
    --bots "${bots}" --duration-sec "${duration}" --ramp-up-sec "${ramp}" \
    --pps "${pps}" --reconnect-percent "${RECONNECT_PERCENT:-5}" \
    --reconnect-interval-sec "${RECONNECT_INTERVAL_SEC:-60}" \
    --reconnect-delay-ms "${RECONNECT_DELAY_MS:-1000}" \
    --payload-sizes "${PAYLOAD_SIZES:-1,32,128,512,1024,4096,6400}" \
    --metrics-interval-sec "${metrics_interval}" \
    --metrics-file "${artifact_dir}/client.csv" \
    --run-id "${run_id}" --seed "${seed}" --probe-pass-count 11 \
    > >(tee "${artifact_dir}/client.log") 2>&1
client_status=$?
set -e

kill -TERM "${server_pid}"
set +e
wait "${server_pid}"
server_status=$?
set -e
server_pid=""
kill -TERM "${collector_pid}" 2>/dev/null || true
wait "${collector_pid}" 2>/dev/null || true
collector_pid=""

min_tps="$(awk -v bots="${bots}" -v pps="${pps}" 'BEGIN { printf "%.3f", bots * pps * 0.95 }')"
set +e
python3 "${root}/scripts/analyze_soak.py" \
    --server-csv "${artifact_dir}/server.csv" \
    --client-csv "${artifact_dir}/client.csv" \
    --output "${artifact_dir}/report.json" \
    --min-recv-tps "${min_tps}" --min-send-tps "${min_tps}" \
    --warmup-sec "${WARMUP_SEC:-300}" \
    | tee "${artifact_dir}/analysis.log"
analysis_status=$?
set -e

if (( client_status != 0 || server_status != 0 || analysis_status != 0 )); then
    echo "soak failed: client=${client_status} server=${server_status} analysis=${analysis_status}" >&2
    exit 1
fi

echo "soak completed: ${artifact_dir}"
