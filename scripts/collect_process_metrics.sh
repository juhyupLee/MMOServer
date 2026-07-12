#!/usr/bin/env bash
set -euo pipefail

pid="${1:?usage: collect_process_metrics.sh PID OUTPUT_CSV [INTERVAL_SEC] [PORT]}"
output="${2:?usage: collect_process_metrics.sh PID OUTPUT_CSV [INTERVAL_SEC] [PORT]}"
interval="${3:-60}"
port="${4:-7777}"

echo "timestamp_ms,rss_kb,pss_kb,private_clean_kb,private_dirty_kb,swap_kb,vmsize_kb,fd_count,thread_count,tcp_established" > "${output}"

read_kb() {
    local file="$1"
    local key="$2"
    awk -v wanted="${key}:" '$1 == wanted { print $2; found=1; exit } END { if (!found) print 0 }' "${file}" 2>/dev/null
}

while kill -0 "${pid}" 2>/dev/null; do
    now_ms="$(date +%s%3N)"
    status="/proc/${pid}/status"
    rollup="/proc/${pid}/smaps_rollup"
    rss="$(read_kb "${status}" VmRSS)"
    vmsize="$(read_kb "${status}" VmSize)"
    threads="$(awk '$1 == "Threads:" { print $2 }' "${status}" 2>/dev/null || echo 0)"
    pss="$(read_kb "${rollup}" Pss)"
    private_clean="$(read_kb "${rollup}" Private_Clean)"
    private_dirty="$(read_kb "${rollup}" Private_Dirty)"
    swap="$(read_kb "${rollup}" Swap)"
    fd_count="$(find "/proc/${pid}/fd" -mindepth 1 -maxdepth 1 2>/dev/null | wc -l)"
    established="$(ss -Htan state established "sport = :${port}" 2>/dev/null | wc -l)"
    echo "${now_ms},${rss},${pss},${private_clean},${private_dirty},${swap},${vmsize},${fd_count},${threads:-0},${established}" >> "${output}"
    sleep "${interval}"
done
