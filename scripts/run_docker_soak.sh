#!/usr/bin/env bash
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export RUN_ID="${RUN_ID:-$(date -u +%Y%m%dT%H%M%SZ)}"
export ARTIFACT_DIR="${ARTIFACT_DIR:-${root}/artifacts/${RUN_ID}}"
compose=(docker compose -f "${root}/compose.soak.yml")

if [[ -e "${ARTIFACT_DIR}" ]]; then
    echo "artifact directory already exists: ${ARTIFACT_DIR}" >&2
    exit 2
fi
mkdir -p "${ARTIFACT_DIR}/preflight"

cleanup() {
    "${compose[@]}" down --remove-orphans >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

docker info >/dev/null
"${compose[@]}" build
"${compose[@]}" config > "${ARTIFACT_DIR}/compose.resolved.yml"
{
    echo "run_id=${RUN_ID}"
    echo "started_utc=$(date -u --iso-8601=seconds)"
    echo "git_sha=$(git -C "${root}" rev-parse HEAD 2>/dev/null || echo unavailable)"
    echo "git_status_begin"
    git -C "${root}" status --short 2>/dev/null || true
    echo "git_status_end"
    uname -a
    docker version
    docker compose version
    docker image inspect "mmoserver-soak:${RUN_ID}" \
        --format 'image_id={{.Id}} repo_digests={{json .RepoDigests}}'
    echo "core_ulimit=$(ulimit -c)"
    echo "core_pattern=$(cat /proc/sys/kernel/core_pattern 2>/dev/null || echo unavailable)"
} > "${ARTIFACT_DIR}/environment.txt"

# Preflight gets a disposable server/counter set.
export SERVER_METRICS_FILE=/artifacts/preflight/server.csv
"${compose[@]}" up -d server
"${compose[@]}" --profile preflight run --rm protocol-probe \
    | tee "${ARTIFACT_DIR}/preflight/protocol-probe.log"
"${compose[@]}" logs --no-color server > "${ARTIFACT_DIR}/preflight/server.log"
"${compose[@]}" down

export SERVER_METRICS_FILE=/artifacts/server.csv
export PROBE_PASS_COUNT=11
set +e
"${compose[@]}" up --no-color --exit-code-from loadbot server loadbot \
    | tee "${ARTIFACT_DIR}/compose.log"
client_status=${PIPESTATUS[0]}
set -e
"${compose[@]}" logs --no-color server > "${ARTIFACT_DIR}/server.log"
"${compose[@]}" logs --no-color loadbot > "${ARTIFACT_DIR}/client.log"
"${compose[@]}" down

mapfile -t client_csvs < <(find "${ARTIFACT_DIR}" -maxdepth 1 -type f -name 'client-*.csv' -print | sort)
if (( ${#client_csvs[@]} == 0 )); then
    echo "no client CSV found" >&2
    exit 1
fi
analyzer_args=(
    python3 "${root}/scripts/analyze_soak.py"
    --server-csv "${ARTIFACT_DIR}/server.csv"
    --output "${ARTIFACT_DIR}/report.json"
    --min-recv-tps "$(awk -v bots="${BOT_COUNT:-1000}" -v pps="${PPS_PER_BOT:-5}" 'BEGIN { printf "%.3f", bots * pps * 0.95 }')"
    --min-send-tps "$(awk -v bots="${BOT_COUNT:-1000}" -v pps="${PPS_PER_BOT:-5}" 'BEGIN { printf "%.3f", bots * pps * 0.95 }')"
    --warmup-sec "${WARMUP_SEC:-300}"
)
for csv in "${client_csvs[@]}"; do
    analyzer_args+=(--client-csv "${csv}")
done
"${analyzer_args[@]}" | tee "${ARTIFACT_DIR}/analysis.log"

if (( client_status != 0 )); then
    echo "loadbot container failed with status ${client_status}" >&2
    exit 1
fi

echo "Docker soak completed: ${ARTIFACT_DIR}"
