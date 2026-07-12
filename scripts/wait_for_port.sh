#!/usr/bin/env bash
set -euo pipefail

host="${1:?usage: wait_for_port.sh HOST PORT [TIMEOUT_SEC]}"
port="${2:?usage: wait_for_port.sh HOST PORT [TIMEOUT_SEC]}"
timeout_sec="${3:-60}"
deadline=$((SECONDS + timeout_sec))

while (( SECONDS < deadline )); do
    if (exec 3<>"/dev/tcp/${host}/${port}") 2>/dev/null; then
        exec 3>&-
        exec 3<&-
        exit 0
    fi
    sleep 0.1
done

echo "timed out waiting for ${host}:${port}" >&2
exit 1
