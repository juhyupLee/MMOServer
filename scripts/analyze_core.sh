#!/usr/bin/env bash
set -euo pipefail

binary="${1:?usage: analyze_core.sh EXACT_BINARY CORE_FILE [OUTPUT_REPORT]}"
core="${2:?usage: analyze_core.sh EXACT_BINARY CORE_FILE [OUTPUT_REPORT]}"
output="${3:-gdb-report-$(date -u +%Y%m%dT%H%M%SZ).txt}"

[[ -f "${binary}" ]] || { echo "binary not found: ${binary}" >&2; exit 2; }
[[ -f "${core}" ]] || { echo "core not found: ${core}" >&2; exit 2; }
command -v gdb >/dev/null || { echo "gdb is required" >&2; exit 2; }

{
    echo "generated_utc=$(date -u --iso-8601=seconds)"
    echo "binary=${binary}"
    echo "core=${core}"
    sha256sum "${binary}" "${core}"
    file "${binary}" "${core}"
    readelf -n "${binary}" | sed -n '/Build ID/ p'
    echo
    gdb -q -batch "${binary}" "${core}" \
        -ex 'set pagination off' \
        -ex 'set print thread-events off' \
        -ex 'info files' \
        -ex 'info sharedlibrary' \
        -ex 'info threads' \
        -ex 'thread apply all bt full' \
        -ex 'thread apply all info registers'
} | tee "${output}"

echo "GDB evidence written to ${output}. Review it, then append a fact/evidence-based incident to troubleshoot.md."
