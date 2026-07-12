#!/usr/bin/env python3
"""Analyze MMO server/client soak-test CSV metrics and emit a JSON verdict.

The analyzer intentionally separates cumulative/final correctness checks from
steady-state performance checks:

* final rows prove shutdown cleanup and end-to-end acknowledgement accounting;
* non-final rows after warm-up provide TPS, latency, session drift, and memory
  growth measurements;
* short smoke runs fall back to the available samples and carry explicit
  confidence warnings instead of crashing or silently analyzing an empty set.

Only the Python standard library is required.
"""

from __future__ import annotations

import argparse
import bisect
import csv
import datetime as dt
import json
import math
import os
import statistics
import sys
from pathlib import Path
from typing import Any, Iterable, Sequence


MIB = 1024.0 * 1024.0
SOAK_24H_SECONDS = 24.0 * 60.0 * 60.0
MIN_CONFIDENT_SAMPLES = 30
DEFAULT_TARGET_BOTS = 1000
DEFAULT_OFFERED_PPS_PER_BOT = 5.0
DEFAULT_THROUGHPUT_RETENTION = 0.95
DEFAULT_MIN_TPS = (
    DEFAULT_TARGET_BOTS * DEFAULT_OFFERED_PPS_PER_BOT * DEFAULT_THROUGHPUT_RETENTION
)
DEFAULT_SAME_HOST_MAX_P95_MS = 50.0

SERVER_COLUMNS = {
    "run_id",
    "timestamp_ms",
    "elapsed_sec",
    "final",
    "sessions",
    "session_objects",
    "created_sessions",
    "removed_sessions",
    "recv_tps",
    "send_tps",
    "recv_total",
    "send_total",
    "recv_bytes",
    "send_bytes",
    "auth_total",
    "protocol_errors",
    "network_errors",
    "backpressure_disconnects",
    "rss_bytes",
    "virtual_bytes",
}

SERVER_INTEGER_COLUMNS = {
    "timestamp_ms",
    "sessions",
    "session_objects",
    "created_sessions",
    "removed_sessions",
    "recv_total",
    "send_total",
    "recv_bytes",
    "send_bytes",
    "auth_total",
    "protocol_errors",
    "network_errors",
    "backpressure_disconnects",
    "rss_bytes",
    "virtual_bytes",
}

SERVER_FLOAT_COLUMNS = {"elapsed_sec", "recv_tps", "send_tps"}

SERVER_CUMULATIVE_COLUMNS = {
    "created_sessions",
    "removed_sessions",
    "recv_total",
    "send_total",
    "recv_bytes",
    "send_bytes",
    "auth_total",
    "protocol_errors",
    "network_errors",
    "backpressure_disconnects",
}

CLIENT_COLUMNS = {
    "run_id",
    "timestamp_ms",
    "elapsed_sec",
    "final",
    "configured_bots",
    "connected",
    "connecting",
    "reconnecting",
    "offered",
    "admitted",
    "acked",
    "timeouts",
    "unexpected_acks",
    "bad_acks",
    "reconnects",
    "connect_failures",
    "probe_pass",
    "probe_fail",
    "rtt_p50_ms",
    "rtt_p95_ms",
    "rtt_p99_ms",
    "send_tps",
    "ack_tps",
}

CLIENT_INTEGER_COLUMNS = {
    "timestamp_ms",
    "configured_bots",
    "connected",
    "connecting",
    "reconnecting",
    "offered",
    "admitted",
    "acked",
    "timeouts",
    "unexpected_acks",
    "bad_acks",
    "reconnects",
    "connect_failures",
    "probe_pass",
    "probe_fail",
}

CLIENT_FLOAT_COLUMNS = {
    "elapsed_sec",
    "rtt_p50_ms",
    "rtt_p95_ms",
    "rtt_p99_ms",
    "send_tps",
    "ack_tps",
}

CLIENT_CUMULATIVE_COLUMNS = {
    "offered",
    "admitted",
    "acked",
    "timeouts",
    "unexpected_acks",
    "bad_acks",
    "reconnects",
    "connect_failures",
    "probe_pass",
    "probe_fail",
}

CLIENT_SUM_COLUMNS = {
    "configured_bots",
    "connected",
    "connecting",
    "reconnecting",
    "offered",
    "admitted",
    "acked",
    "timeouts",
    "unexpected_acks",
    "bad_acks",
    "reconnects",
    "connect_failures",
    "probe_pass",
    "probe_fail",
    "send_tps",
    "ack_tps",
}

CLIENT_LATENCY_COLUMNS = {"rtt_p50_ms", "rtt_p95_ms", "rtt_p99_ms"}


class AnalysisInputError(Exception):
    """Raised when CSV data or command-line combinations are unusable."""


def _non_negative_float(text: str) -> float:
    try:
        value = float(text)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"expected a number, got {text!r}") from exc
    if not math.isfinite(value) or value < 0.0:
        raise argparse.ArgumentTypeError("value must be a finite number >= 0")
    return value


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Analyze server and one or more bot-client soak CSV files, write a "
            "machine-readable JSON report, and return PASS/FAIL as the process "
            "exit status."
        ),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        epilog=(
            "Exit codes: 0=PASS, 1=acceptance failure, 2=input/report error.\n"
            "Example:\n"
            "  python scripts/analyze_soak.py --server-csv out/server.csv "
            "--client-csv out/client-0.csv --client-csv out/client-1.csv "
            "--output out/report.json"
        ),
    )
    parser.add_argument("--server-csv", required=True, type=Path, help="server metrics CSV")
    parser.add_argument(
        "--client-csv",
        required=True,
        action="append",
        type=Path,
        help="bot-client metrics CSV; repeat once per client shard",
    )
    parser.add_argument("--output", required=True, type=Path, help="JSON report path")
    parser.add_argument(
        "--min-recv-tps",
        type=_non_negative_float,
        default=DEFAULT_MIN_TPS,
        help=(
            "minimum warm median server receive TPS; provisional default is "
            "1000 bots x 5 offered packets/s x 95%% retention"
        ),
    )
    parser.add_argument(
        "--min-send-tps",
        type=_non_negative_float,
        default=DEFAULT_MIN_TPS,
        help=(
            "minimum warm median server send TPS; provisional default is "
            "1000 bots x 5 offered packets/s x 95%% retention"
        ),
    )
    parser.add_argument(
        "--max-p95-ms",
        type=_non_negative_float,
        default=DEFAULT_SAME_HOST_MAX_P95_MS,
        help=(
            "maximum observed client-shard aggregate RTT p95 in milliseconds; "
            "the default is a provisional same-host target"
        ),
    )
    parser.add_argument(
        "--max-session-drift",
        type=_non_negative_float,
        default=5.0,
        help="maximum absolute server sessions minus aggregate connected clients",
    )
    parser.add_argument(
        "--warmup-sec",
        type=_non_negative_float,
        default=300.0,
        help="elapsed time excluded before steady-state analysis",
    )
    parser.add_argument(
        "--max-rss-growth-mb",
        type=_non_negative_float,
        default=128.0,
        help="maximum warm baseline-to-tail median RSS growth",
    )
    parser.add_argument(
        "--max-vm-growth-mb",
        type=_non_negative_float,
        default=None,
        help=(
            "optional maximum warm baseline-to-tail virtual-memory growth; omit "
            "to report VM growth without making it a FAIL gate"
        ),
    )
    return parser


def _parse_integer(value: str | None, path: Path, line: int, column: str) -> int:
    if value is None or value.strip() == "":
        raise AnalysisInputError(f"{path}:{line}: empty integer column {column!r}")
    try:
        parsed = int(value.strip(), 10)
    except ValueError as exc:
        raise AnalysisInputError(
            f"{path}:{line}: invalid integer {column}={value!r}"
        ) from exc
    if parsed < 0:
        raise AnalysisInputError(f"{path}:{line}: {column} must be >= 0")
    return parsed


def _parse_float(value: str | None, path: Path, line: int, column: str) -> float:
    if value is None or value.strip() == "":
        raise AnalysisInputError(f"{path}:{line}: empty numeric column {column!r}")
    try:
        parsed = float(value.strip())
    except ValueError as exc:
        raise AnalysisInputError(
            f"{path}:{line}: invalid number {column}={value!r}"
        ) from exc
    if not math.isfinite(parsed) or parsed < 0.0:
        raise AnalysisInputError(
            f"{path}:{line}: {column} must be a finite number >= 0"
        )
    return parsed


def _parse_final(value: str | None, path: Path, line: int) -> bool:
    if value is None:
        raise AnalysisInputError(f"{path}:{line}: empty 'final' column")
    normalized = value.strip().lower()
    if normalized in {"1", "true", "yes"}:
        return True
    if normalized in {"0", "false", "no"}:
        return False
    raise AnalysisInputError(
        f"{path}:{line}: final must be 0/1 or false/true, got {value!r}"
    )


def _load_csv(path: Path, kind: str) -> list[dict[str, Any]]:
    required = SERVER_COLUMNS if kind == "server" else CLIENT_COLUMNS
    integer_columns = SERVER_INTEGER_COLUMNS if kind == "server" else CLIENT_INTEGER_COLUMNS
    float_columns = SERVER_FLOAT_COLUMNS if kind == "server" else CLIENT_FLOAT_COLUMNS

    if not path.exists():
        raise AnalysisInputError(f"{kind} CSV does not exist: {path}")
    if not path.is_file():
        raise AnalysisInputError(f"{kind} CSV is not a regular file: {path}")

    rows: list[dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            if reader.fieldnames is None:
                raise AnalysisInputError(f"{path}: CSV header is missing")
            fieldnames = [name.strip() if name is not None else "" for name in reader.fieldnames]
            if len(fieldnames) != len(set(fieldnames)):
                raise AnalysisInputError(f"{path}: CSV header contains duplicate columns")
            # DictReader retains the original header strings as mapping keys.  Use
            # the normalized names validated above so harmless surrounding spaces
            # do not turn into misleading "empty required column" errors.
            reader.fieldnames = fieldnames
            missing = sorted(required.difference(fieldnames))
            if missing:
                raise AnalysisInputError(
                    f"{path}: missing required columns: {', '.join(missing)}"
                )

            for line, raw in enumerate(reader, start=2):
                if raw.get(None):
                    raise AnalysisInputError(f"{path}:{line}: too many CSV fields")
                if all(value is None or value.strip() == "" for value in raw.values()):
                    continue
                run_id_value = raw.get("run_id")
                run_id = "" if run_id_value is None else run_id_value.strip()
                if not run_id:
                    raise AnalysisInputError(f"{path}:{line}: run_id is empty")
                row: dict[str, Any] = {"run_id": run_id, "_line": line}
                for column in integer_columns:
                    row[column] = _parse_integer(raw.get(column), path, line, column)
                for column in float_columns:
                    row[column] = _parse_float(raw.get(column), path, line, column)
                row["final"] = _parse_final(raw.get("final"), path, line)
                rows.append(row)
    except (OSError, UnicodeError, csv.Error) as exc:
        raise AnalysisInputError(f"cannot read {path}: {exc}") from exc

    if not rows:
        raise AnalysisInputError(f"{path}: CSV contains no data rows")
    return rows


def _select_server_run(
    rows: Sequence[dict[str, Any]], warnings: list[str]
) -> tuple[str, list[dict[str, Any]], list[str]]:
    run_ids = sorted({str(row["run_id"]) for row in rows})
    latest_by_run = {
        run_id: max(int(row["timestamp_ms"]) for row in rows if row["run_id"] == run_id)
        for run_id in run_ids
    }
    selected_run = max(run_ids, key=lambda item: (latest_by_run[item], item))
    ignored = [run_id for run_id in run_ids if run_id != selected_run]
    if ignored:
        warnings.append(
            "Server CSV contains multiple run_id values; selected the most recent "
            f"run {selected_run!r} and ignored: {', '.join(ignored)}."
        )
    selected = sorted(
        (dict(row) for row in rows if row["run_id"] == selected_run),
        key=lambda row: (row["timestamp_ms"], row["elapsed_sec"], row["_line"]),
    )
    return selected_run, selected, ignored


def _select_client_run(
    rows: Sequence[dict[str, Any]], run_id: str, path: Path, warnings: list[str]
) -> tuple[list[dict[str, Any]], list[str]]:
    available = sorted({str(row["run_id"]) for row in rows})
    selected = sorted(
        (dict(row) for row in rows if row["run_id"] == run_id),
        key=lambda row: (row["timestamp_ms"], row["elapsed_sec"], row["_line"]),
    )
    if not selected:
        raise AnalysisInputError(
            f"{path}: no rows for selected server run_id {run_id!r}; "
            f"available run_id values: {', '.join(available)}"
        )
    ignored = [candidate for candidate in available if candidate != run_id]
    if ignored:
        warnings.append(
            f"{path} contains other runs that were ignored: {', '.join(ignored)}."
        )
    return selected, ignored


def _validate_series(
    rows: Sequence[dict[str, Any]],
    path: Path,
    cumulative_columns: Iterable[str],
) -> None:
    previous: dict[str, Any] | None = None
    for row in rows:
        if previous is not None:
            if row["elapsed_sec"] + 1e-6 < previous["elapsed_sec"]:
                raise AnalysisInputError(
                    f"{path}:{row['_line']}: elapsed_sec moved backwards within run_id "
                    f"{row['run_id']!r}"
                )
            for column in cumulative_columns:
                if row[column] < previous[column]:
                    raise AnalysisInputError(
                        f"{path}:{row['_line']}: cumulative counter {column!r} "
                        f"decreased from {previous[column]} to {row[column]}"
                    )
        previous = row


def _percentile(values: Sequence[float], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(float(value) for value in values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def _distribution(values: Sequence[float]) -> dict[str, float | int | None]:
    if not values:
        return {"samples": 0, "min": None, "p10": None, "median": None, "p95": None, "max": None}
    numeric = [float(value) for value in values]
    return {
        "samples": len(numeric),
        "min": min(numeric),
        "p10": _percentile(numeric, 0.10),
        "median": statistics.median(numeric),
        "p95": _percentile(numeric, 0.95),
        "max": max(numeric),
    }


def _median_positive_interval_ms(rows: Sequence[dict[str, Any]]) -> float | None:
    timestamps = sorted({int(row["timestamp_ms"]) for row in rows})
    differences = [
        float(current - previous)
        for previous, current in zip(timestamps, timestamps[1:])
        if current > previous
    ]
    return statistics.median(differences) if differences else None


def _steady_rows(
    rows: Sequence[dict[str, Any]],
    warmup_sec: float,
    label: str,
    warnings: list[str],
) -> list[dict[str, Any]]:
    operational = [row for row in rows if not row.get("final", False)]
    if not operational:
        operational = list(rows)
        warnings.append(
            f"{label} has no non-final samples; final/fallback samples are being used "
            "for steady-state statistics."
        )
    warm = [row for row in operational if float(row["elapsed_sec"]) >= warmup_sec]
    if not warm:
        warm = operational
        duration = max(float(row["elapsed_sec"]) for row in operational)
        warnings.append(
            f"{label} has no samples at or after warmup {warmup_sec:.3f}s "
            f"(last elapsed={duration:.3f}s); all available operational samples are used."
        )
    if len(warm) < MIN_CONFIDENT_SAMPLES:
        warnings.append(
            f"{label} has only {len(warm)} steady-state sample(s); percentile and "
            "memory-growth confidence is low "
            f"(at least {MIN_CONFIDENT_SAMPLES} are recommended)."
        )
    return warm


def _aggregate_rows(rows: Sequence[dict[str, Any]], source_count: int) -> dict[str, Any]:
    aggregate: dict[str, Any] = {
        "run_id": rows[0]["run_id"],
        "timestamp_ms": int(round(statistics.median(row["timestamp_ms"] for row in rows))),
        "elapsed_sec": statistics.median(float(row["elapsed_sec"]) for row in rows),
        "final": all(bool(row["final"]) for row in rows),
        "source_count": len(rows),
        "expected_source_count": source_count,
        "complete": len(rows) == source_count,
    }
    for column in CLIENT_SUM_COLUMNS:
        aggregate[column] = sum(row[column] for row in rows)
    for column in CLIENT_LATENCY_COLUMNS:
        # A conservative shard aggregate: the slowest shard's percentile wins.
        aggregate[column] = max(float(row[column]) for row in rows)
    return aggregate


def _aggregate_client_samples(
    client_sources: Sequence[dict[str, Any]], warnings: list[str]
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    operational_sources = [
        [row for row in source["rows"] if not row["final"]] for source in client_sources
    ]
    intervals = [
        interval
        for rows in operational_sources
        if (interval := _median_positive_interval_ms(rows)) is not None
    ]
    nominal_interval = statistics.median(intervals) if intervals else 2000.0
    tolerance_ms = max(250.0, min(5000.0, nominal_interval * 0.45))

    anchor_index = max(
        range(len(operational_sources)), key=lambda index: len(operational_sources[index])
    )
    anchor_rows = operational_sources[anchor_index]
    groups: list[list[dict[str, Any]]] = [[row] for row in anchor_rows]
    anchor_timestamps = [int(row["timestamp_ms"]) for row in anchor_rows]

    for source_index, rows in enumerate(operational_sources):
        if source_index == anchor_index:
            continue
        used_anchor_indices: set[int] = set()
        for row in rows:
            timestamp = int(row["timestamp_ms"])
            insertion = bisect.bisect_left(anchor_timestamps, timestamp)
            candidates = [index for index in (insertion - 1, insertion) if 0 <= index < len(groups)]
            candidates = [index for index in candidates if index not in used_anchor_indices]
            if not candidates:
                continue
            nearest = min(candidates, key=lambda index: abs(anchor_timestamps[index] - timestamp))
            if abs(anchor_timestamps[nearest] - timestamp) <= tolerance_ms:
                groups[nearest].append(row)
                used_anchor_indices.add(nearest)

    aggregates = [_aggregate_rows(group, len(client_sources)) for group in groups]
    complete_count = sum(1 for row in aggregates if row["complete"])
    if len(client_sources) > 1 and complete_count < len(aggregates):
        warnings.append(
            f"Only {complete_count}/{len(aggregates)} client timestamp groups contain "
            f"all {len(client_sources)} shards (matching tolerance {tolerance_ms:.0f}ms); "
            "incomplete groups are excluded from aggregate acceptance metrics."
        )
    return aggregates, {
        "anchor_client_csv": str(client_sources[anchor_index]["path"].resolve()),
        "nominal_interval_ms": nominal_interval,
        "matching_tolerance_ms": tolerance_ms,
        "groups": len(aggregates),
        "complete_groups": complete_count,
    }


def _aggregate_client_final(
    client_sources: Sequence[dict[str, Any]], warnings: list[str]
) -> tuple[dict[str, Any], bool, list[str]]:
    selected_rows: list[dict[str, Any]] = []
    missing: list[str] = []
    for source in client_sources:
        final_rows = [row for row in source["rows"] if row["final"]]
        if final_rows:
            selected_rows.append(final_rows[-1])
        else:
            selected_rows.append(source["rows"][-1])
            missing.append(str(source["path"].resolve()))
    if missing:
        warnings.append(
            "Client final row is missing in the following file(s); their last row is "
            f"reported only as a diagnostic fallback: {', '.join(missing)}."
        )
    return _aggregate_rows(selected_rows, len(client_sources)), not missing, missing


def _linear_slope_per_hour(rows: Sequence[dict[str, Any]], column: str) -> float | None:
    if len(rows) < 2:
        return None
    x_values = [float(row["elapsed_sec"]) for row in rows]
    y_values = [float(row[column]) / MIB for row in rows]
    x_mean = statistics.mean(x_values)
    y_mean = statistics.mean(y_values)
    denominator = sum((value - x_mean) ** 2 for value in x_values)
    if denominator <= 0.0:
        return None
    slope_per_second = sum(
        (x_value - x_mean) * (y_value - y_mean)
        for x_value, y_value in zip(x_values, y_values)
    ) / denominator
    return slope_per_second * 3600.0


def _memory_summary(rows: Sequence[dict[str, Any]], column: str) -> dict[str, Any]:
    if not rows:
        return {
            "available": False,
            "samples": 0,
            "window_samples": 0,
            "baseline_median_mb": None,
            "tail_median_mb": None,
            "growth_mb": None,
            "peak_growth_mb": None,
            "peak_mb": None,
            "linear_slope_mb_per_hour": None,
        }
    values_mb = [float(row[column]) / MIB for row in rows]
    window = min(60, max(1, int(round(len(rows) * 0.05))))
    baseline = statistics.median(values_mb[:window])
    tail = statistics.median(values_mb[-window:])
    peak = max(values_mb)
    return {
        "available": all(float(row[column]) > 0.0 for row in rows),
        "samples": len(rows),
        "window_samples": window,
        "baseline_median_mb": baseline,
        "tail_median_mb": tail,
        "growth_mb": tail - baseline,
        "peak_growth_mb": peak - baseline,
        "peak_mb": peak,
        "linear_slope_mb_per_hour": _linear_slope_per_hour(rows, column),
    }


def _nearest_row(
    rows: Sequence[dict[str, Any]], timestamps: Sequence[int], timestamp_ms: int
) -> tuple[dict[str, Any] | None, float | None]:
    if not rows:
        return None, None
    insertion = bisect.bisect_left(timestamps, timestamp_ms)
    candidates = [index for index in (insertion - 1, insertion) if 0 <= index < len(rows)]
    nearest = min(candidates, key=lambda index: abs(timestamps[index] - timestamp_ms))
    gap = abs(timestamps[nearest] - timestamp_ms)
    return rows[nearest], float(gap)


def _session_drift_summary(
    server_rows: Sequence[dict[str, Any]],
    client_rows: Sequence[dict[str, Any]],
    pairing_tolerance_ms: float,
) -> dict[str, Any]:
    client_sorted = sorted(client_rows, key=lambda row: row["timestamp_ms"])
    client_timestamps = [int(row["timestamp_ms"]) for row in client_sorted]
    samples: list[dict[str, Any]] = []
    for server in server_rows:
        client, gap = _nearest_row(client_sorted, client_timestamps, int(server["timestamp_ms"]))
        if client is None or gap is None or gap > pairing_tolerance_ms:
            continue
        # Server session-map entries include outbound connections that are still
        # completing their handshake. Compare against connected+connecting and
        # reserve `reconnecting` for clients intentionally in backoff.
        client_active = int(client["connected"]) + int(client["connecting"])
        signed = int(server["sessions"]) - client_active
        samples.append(
            {
                "timestamp_ms": int(server["timestamp_ms"]),
                "server_sessions": int(server["sessions"]),
                "client_connected": int(client["connected"]),
                "client_connecting": int(client["connecting"]),
                "client_active": client_active,
                "signed_drift": signed,
                "absolute_drift": abs(signed),
                "timestamp_gap_ms": gap,
            }
        )
    if not samples:
        return {
            "available": False,
            "samples": 0,
            "pairing_tolerance_ms": pairing_tolerance_ms,
            "max_absolute": None,
            "median_absolute": None,
            "p95_absolute": None,
            "max_timestamp_gap_ms": None,
            "worst_sample": None,
        }
    absolute = [float(sample["absolute_drift"]) for sample in samples]
    worst = max(samples, key=lambda sample: sample["absolute_drift"])
    return {
        "available": True,
        "samples": len(samples),
        "pairing_tolerance_ms": pairing_tolerance_ms,
        "max_absolute": max(absolute),
        "median_absolute": statistics.median(absolute),
        "p95_absolute": _percentile(absolute, 0.95),
        "max_timestamp_gap_ms": max(sample["timestamp_gap_ms"] for sample in samples),
        "worst_sample": worst,
    }


def _check(
    checks: list[dict[str, Any]],
    name: str,
    passed: bool,
    actual: Any,
    operator: str,
    threshold: Any,
    pass_message: str,
    fail_message: str,
) -> None:
    checks.append(
        {
            "name": name,
            "passed": bool(passed),
            "actual": actual,
            "operator": operator,
            "threshold": threshold,
            "message": pass_message if passed else fail_message,
        }
    )


def _analyze(args: argparse.Namespace) -> dict[str, Any]:
    warnings: list[str] = []
    server_path = args.server_csv.resolve()
    client_paths = [path.resolve() for path in args.client_csv]
    output_path = args.output.resolve()

    if len(client_paths) != len(set(client_paths)):
        raise AnalysisInputError("the same --client-csv path was supplied more than once")
    input_paths = {server_path, *client_paths}
    if output_path in input_paths:
        raise AnalysisInputError("--output must not overwrite an input CSV")

    all_server_rows = _load_csv(server_path, "server")
    run_id, server_rows, ignored_server_runs = _select_server_run(all_server_rows, warnings)
    _validate_series(server_rows, server_path, SERVER_CUMULATIVE_COLUMNS)

    client_sources: list[dict[str, Any]] = []
    ignored_client_runs: dict[str, list[str]] = {}
    for path in client_paths:
        all_rows = _load_csv(path, "client")
        rows, ignored = _select_client_run(all_rows, run_id, path, warnings)
        _validate_series(rows, path, CLIENT_CUMULATIVE_COLUMNS)
        client_sources.append({"path": path, "rows": rows})
        ignored_client_runs[str(path)] = ignored

    client_groups, aggregation = _aggregate_client_samples(client_sources, warnings)
    complete_client_groups = [row for row in client_groups if row["complete"]]
    client_steady = _steady_rows(
        complete_client_groups, args.warmup_sec, "aggregate client", warnings
    ) if complete_client_groups else []
    if not complete_client_groups:
        warnings.append("No complete non-final aggregate client timestamp groups are available.")

    # Align server performance/memory samples to the period in which clients
    # were actually running. Otherwise startup readiness delays or time spent
    # waiting to stop the server can dominate a short smoke run with zero TPS.
    if complete_client_groups:
        first_client_ms = min(int(row["timestamp_ms"]) for row in complete_client_groups)
        last_client_ms = max(int(row["timestamp_ms"]) for row in complete_client_groups)
        server_interval_hint = _median_positive_interval_ms(server_rows) or 1000.0
        pad_ms = max(250.0, min(5000.0, server_interval_hint * 0.55))
        server_during_clients = [
            row
            for row in server_rows
            if not row["final"]
            and first_client_ms - pad_ms <= int(row["timestamp_ms"]) <= last_client_ms + pad_ms
        ]
        if server_during_clients:
            first_server_elapsed = min(
                float(row["elapsed_sec"]) for row in server_during_clients
            )
            server_steady = _steady_rows(
                server_during_clients,
                first_server_elapsed + args.warmup_sec,
                "server during client activity",
                warnings,
            )
        else:
            warnings.append(
                "No server samples overlap the aggregate client activity window; "
                "falling back to the complete server run."
            )
            server_steady = _steady_rows(
                server_rows, args.warmup_sec, "server", warnings
            )
    else:
        server_steady = _steady_rows(server_rows, args.warmup_sec, "server", warnings)

    server_final_rows = [row for row in server_rows if row["final"]]
    server_final_present = bool(server_final_rows)
    server_final = server_final_rows[-1] if server_final_rows else server_rows[-1]
    if not server_final_present:
        warnings.append(
            "Server final row is missing; the last row is shown diagnostically but cannot "
            "prove error-free shutdown or session cleanup."
        )
    client_final, client_final_complete, missing_client_finals = _aggregate_client_final(
        client_sources, warnings
    )

    duration_sec = max(float(row["elapsed_sec"]) for row in server_rows)
    client_duration_sec = min(
        max(float(row["elapsed_sec"]) for row in source["rows"])
        for source in client_sources
    )
    if duration_sec < SOAK_24H_SECONDS:
        warnings.append(
            f"Run duration is {duration_sec:.3f}s, below 24h ({SOAK_24H_SECONDS:.0f}s); "
            "this verdict is useful for smoke/regression testing but is not 24h soak evidence."
        )
    if client_duration_sec < SOAK_24H_SECONDS:
        warnings.append(
            f"Shortest client-shard duration is {client_duration_sec:.3f}s, below 24h "
            f"({SOAK_24H_SECONDS:.0f}s); server idle time cannot qualify a short bot run."
        )

    recv_tps = _distribution([float(row["recv_tps"]) for row in server_steady])
    send_tps = _distribution([float(row["send_tps"]) for row in server_steady])
    client_send_tps = _distribution([float(row["send_tps"]) for row in client_steady])
    client_ack_tps = _distribution([float(row["ack_tps"]) for row in client_steady])
    client_rtt_p50 = _distribution([float(row["rtt_p50_ms"]) for row in client_steady])
    client_rtt_p95 = _distribution([float(row["rtt_p95_ms"]) for row in client_steady])
    client_rtt_p99 = _distribution([float(row["rtt_p99_ms"]) for row in client_steady])

    rss = _memory_summary(server_steady, "rss_bytes")
    virtual = _memory_summary(server_steady, "virtual_bytes")
    if not rss["available"]:
        warnings.append("RSS contains a zero/unavailable measurement; RSS growth is not trustworthy.")
    if not virtual["available"]:
        warnings.append(
            "Virtual memory contains a zero/unavailable measurement; VM growth is not trustworthy."
        )
    if args.max_vm_growth_mb is None:
        warnings.append(
            "Virtual-memory growth is report-only. Pass --max-vm-growth-mb to enable "
            "a VM FAIL gate; allocator reservations, sanitizers, and short shutdown "
            "samples can otherwise cause false positives."
        )

    server_interval = _median_positive_interval_ms(server_steady)
    client_interval = _median_positive_interval_ms(client_steady)
    relevant_intervals = [value for value in (server_interval, client_interval) if value is not None]
    nominal_pair_interval = max(relevant_intervals) if relevant_intervals else 2000.0
    pairing_tolerance_ms = max(500.0, min(30000.0, nominal_pair_interval * 0.75))
    stable_client_steady = [
        row
        for row in client_steady
        if int(row["connecting"]) == 0 and int(row["reconnecting"]) == 0
    ]
    if not stable_client_steady:
        stable_client_steady = client_steady
        warnings.append(
            "No fully stable client samples exist for ghost-session drift; churn samples "
            "are used as a lower-confidence fallback."
        )
    drift = _session_drift_summary(
        server_steady, stable_client_steady, pairing_tolerance_ms
    )
    if drift["samples"] < MIN_CONFIDENT_SAMPLES:
        warnings.append(
            f"Only {drift['samples']} server/client session-drift pair(s) are available; "
            "timing-drift confidence is low "
            f"(at least {MIN_CONFIDENT_SAMPLES} are recommended)."
        )

    peak_configured_bots = max(
        (int(row["configured_bots"]) for row in complete_client_groups), default=0
    )
    peak_connected = max((int(row["connected"]) for row in complete_client_groups), default=0)
    steady_connected_values = [int(row["connected"]) for row in client_steady]
    steady_connected_target_ratio = (
        sum(value >= DEFAULT_TARGET_BOTS for value in steady_connected_values)
        / len(steady_connected_values)
        if steady_connected_values
        else 0.0
    )
    if peak_configured_bots < DEFAULT_TARGET_BOTS:
        warnings.append(
            f"Peak configured bots is {peak_configured_bots}; this run does not demonstrate "
            f"the requested {DEFAULT_TARGET_BOTS}-client concurrency target."
        )
    if peak_connected < DEFAULT_TARGET_BOTS:
        warnings.append(
            f"Peak connected clients is {peak_connected}; this run does not demonstrate "
            f"{DEFAULT_TARGET_BOTS} simultaneously connected clients."
        )
    if int(client_final["reconnects"]) == 0:
        warnings.append("Client reconnects is zero; disconnect/reconnect coverage was not observed.")
    if int(client_final["probe_pass"]) == 0:
        warnings.append("Client probe_pass is zero; packet boundary probe coverage was not observed.")
    if server_final_present and int(server_final["network_errors"]) > 0:
        warnings.append(
            f"Server network_errors is {server_final['network_errors']}. This counter is "
            "report-only because normal EOF and injected reconnects can increment it; "
            "review its rate and correlate unexpected spikes with logs."
        )
    dropped_before_admission = int(client_final["offered"]) - int(client_final["admitted"])
    if dropped_before_admission > 0:
        warnings.append(
            f"Clients offered {dropped_before_admission} packet(s) that were not admitted "
            "to the socket send path."
        )
    if int(client_final["admitted"]) == 0:
        warnings.append("Client admitted-packet count is zero; ACK integrity has no traffic coverage.")

    checks: list[dict[str, Any]] = []
    _check(
        checks,
        "server.final_present",
        server_final_present,
        server_final_present,
        "==",
        True,
        "Server emitted a final shutdown sample.",
        "Server did not emit a final shutdown sample.",
    )

    recv_median = recv_tps["median"]
    _check(
        checks,
        "server.recv_tps_median",
        recv_median is not None and recv_median >= args.min_recv_tps,
        recv_median,
        ">=",
        args.min_recv_tps,
        f"Median receive TPS {recv_median:.3f} meets the target.",
        "Median receive TPS is unavailable or below the target."
        if recv_median is None
        else f"Median receive TPS {recv_median:.3f} is below {args.min_recv_tps:.3f}.",
    )
    send_median = send_tps["median"]
    _check(
        checks,
        "server.send_tps_median",
        send_median is not None and send_median >= args.min_send_tps,
        send_median,
        ">=",
        args.min_send_tps,
        f"Median send TPS {send_median:.3f} meets the target.",
        "Median send TPS is unavailable or below the target."
        if send_median is None
        else f"Median send TPS {send_median:.3f} is below {args.min_send_tps:.3f}.",
    )

    for column in ("protocol_errors", "backpressure_disconnects"):
        actual = int(server_final[column])
        passed = server_final_present and actual == 0
        _check(
            checks,
            f"server.{column}",
            passed,
            actual,
            "==",
            0,
            f"Final {column} is zero.",
            f"Final {column} is {actual}." if server_final_present else f"Cannot verify {column} without final row.",
        )

    for column in ("sessions", "session_objects"):
        actual = int(server_final[column])
        passed = server_final_present and actual == 0
        _check(
            checks,
            f"server.final_{column}",
            passed,
            actual,
            "==",
            0,
            f"Final server {column} is zero.",
            f"Final server {column} is {actual}."
            if server_final_present
            else f"Cannot verify final {column} without a server final row.",
        )

    created_sessions = int(server_final["created_sessions"])
    removed_sessions = int(server_final["removed_sessions"])
    _check(
        checks,
        "server.created_equals_removed_sessions",
        server_final_present and created_sessions == removed_sessions,
        {"created_sessions": created_sessions, "removed_sessions": removed_sessions},
        "created_sessions == removed_sessions",
        True,
        "Every created server session was removed.",
        "Created and removed session totals differ, or the server final row is missing.",
    )

    _check(
        checks,
        "client.final_present_all_shards",
        client_final_complete,
        len(client_sources) - len(missing_client_finals),
        "==",
        len(client_sources),
        "Every client shard emitted a final sample.",
        "One or more client shards did not emit a final sample.",
    )
    for column in (
        "timeouts",
        "unexpected_acks",
        "bad_acks",
        "connect_failures",
        "probe_fail",
    ):
        actual = int(client_final[column])
        passed = client_final_complete and actual == 0
        _check(
            checks,
            f"client.{column}",
            passed,
            actual,
            "==",
            0,
            f"Final aggregate client {column} is zero.",
            f"Final aggregate client {column} is {actual}."
            if client_final_complete
            else f"Cannot verify {column}: a client final row is missing.",
        )

    offered = int(client_final["offered"])
    admitted = int(client_final["admitted"])
    acked = int(client_final["acked"])
    ordering_ok = client_final_complete and offered >= admitted >= acked
    _check(
        checks,
        "client.packet_counter_order",
        ordering_ok,
        {"offered": offered, "admitted": admitted, "acked": acked},
        "offered >= admitted >= acked",
        True,
        "Client packet counters have a valid offered/admitted/acked order.",
        "Client packet counters are incomplete or violate offered >= admitted >= acked.",
    )
    admission_ok = client_final_complete and offered == admitted
    _check(
        checks,
        "client.offered_equals_admitted",
        admission_ok,
        {"offered": offered, "admitted": admitted, "not_admitted": offered - admitted},
        "offered == admitted",
        True,
        "Every offered packet was admitted to the socket send path.",
        "Offered and admitted packet totals differ, or final client data is incomplete.",
    )
    ack_ok = client_final_complete and admitted == acked
    _check(
        checks,
        "client.admitted_equals_acked",
        ack_ok,
        {"admitted": admitted, "acked": acked, "unacked": admitted - acked},
        "admitted == acked",
        True,
        "Every admitted packet received its expected ACK.",
        "Admitted and ACKed packet totals differ, or final client data is incomplete.",
    )

    observed_p95 = client_rtt_p95["max"]
    observed_p95_pass_message = (
        "Aggregate RTT p95 is available and meets the target."
        if observed_p95 is None
        else f"Maximum observed aggregate RTT p95 {observed_p95:.3f}ms meets the target."
    )
    _check(
        checks,
        "client.rtt_p95_max",
        observed_p95 is not None and observed_p95 <= args.max_p95_ms,
        observed_p95,
        "<=",
        args.max_p95_ms,
        observed_p95_pass_message,
        "Aggregate RTT p95 is unavailable or exceeds the target."
        if observed_p95 is None
        else f"Maximum observed aggregate RTT p95 {observed_p95:.3f}ms exceeds {args.max_p95_ms:.3f}ms.",
    )

    max_drift = drift["max_absolute"]
    drift_pass_message = (
        "Session drift is available and meets the target."
        if max_drift is None
        else f"Maximum server/client session drift {max_drift:.0f} meets the target."
    )
    _check(
        checks,
        "sessions.max_absolute_drift",
        bool(drift["available"]) and max_drift <= args.max_session_drift,
        max_drift,
        "<=",
        args.max_session_drift,
        drift_pass_message,
        "Session drift is unavailable or exceeds the target."
        if max_drift is None
        else f"Maximum server/client session drift {max_drift:.0f} exceeds {args.max_session_drift:.0f}.",
    )

    memory_gates = [("rss", rss, args.max_rss_growth_mb)]
    if args.max_vm_growth_mb is not None:
        memory_gates.append(("virtual_memory", virtual, args.max_vm_growth_mb))
    for name, summary, threshold in memory_gates:
        growth = summary["growth_mb"]
        passed = bool(summary["available"]) and growth is not None and growth <= threshold
        _check(
            checks,
            f"memory.{name}_growth_mb",
            passed,
            growth,
            "<=",
            threshold,
            f"Warm baseline-to-tail {name} growth {growth:.3f}MiB meets the target.",
            f"{name} growth is unavailable or exceeds the target."
            if growth is None
            else f"Warm baseline-to-tail {name} growth {growth:.3f}MiB exceeds {threshold:.3f}MiB."
            if summary["available"]
            else f"Cannot verify {name} growth because a memory sample is unavailable.",
        )

    passed = all(check["passed"] for check in checks)
    qualification = {
        "server_duration_at_least_24h": duration_sec >= SOAK_24H_SECONDS,
        "every_client_shard_duration_at_least_24h": (
            client_duration_sec >= SOAK_24H_SECONDS
        ),
        "peak_configured_bots_at_least_1000": peak_configured_bots >= DEFAULT_TARGET_BOTS,
        "peak_connected_at_least_1000": peak_connected >= DEFAULT_TARGET_BOTS,
        "at_least_90_percent_steady_samples_at_1000_connected": (
            steady_connected_target_ratio >= 0.90
        ),
        "reconnect_coverage_observed": int(client_final["reconnects"]) > 0,
        "packet_probe_coverage_observed": int(client_final["probe_pass"]) > 0,
        "acceptance_checks_passed": passed,
        "qualified": (
            passed
            and duration_sec >= SOAK_24H_SECONDS
            and client_duration_sec >= SOAK_24H_SECONDS
            and peak_configured_bots >= DEFAULT_TARGET_BOTS
            and peak_connected >= DEFAULT_TARGET_BOTS
            and steady_connected_target_ratio >= 0.90
            and int(client_final["reconnects"]) > 0
            and int(client_final["probe_pass"]) > 0
        ),
    }

    return {
        "schema_version": 1,
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "status": "PASS" if passed else "FAIL",
        "run_id": run_id,
        "inputs": {
            "server_csv": str(server_path),
            "client_csv": [str(path) for path in client_paths],
            "output": str(output_path),
            "ignored_server_run_ids": ignored_server_runs,
            "ignored_client_run_ids": ignored_client_runs,
        },
        "acceptance": {
            "provisional_profile": {
                "target_bots": DEFAULT_TARGET_BOTS,
                "offered_packets_per_second_per_bot": DEFAULT_OFFERED_PPS_PER_BOT,
                "throughput_retention": DEFAULT_THROUGHPUT_RETENTION,
                "derived_default_min_tps": DEFAULT_MIN_TPS,
                "latency_environment": "same-host",
                "derived_default_max_p95_ms": DEFAULT_SAME_HOST_MAX_P95_MS,
                "note": (
                    "Defaults are provisional; override them for the production network, "
                    "packet mix, and gameplay budget."
                ),
            },
            "min_recv_tps": args.min_recv_tps,
            "min_send_tps": args.min_send_tps,
            "max_p95_ms": args.max_p95_ms,
            "max_session_drift": args.max_session_drift,
            "warmup_sec": args.warmup_sec,
            "max_rss_growth_mb": args.max_rss_growth_mb,
            "max_vm_growth_mb": args.max_vm_growth_mb,
            "vm_growth_gate_enabled": args.max_vm_growth_mb is not None,
            "network_errors_gate_enabled": False,
            "network_errors_policy": (
                "report-only because normal EOF and reconnect scenarios may increment it"
            ),
        },
        "coverage": {
            "duration_sec": duration_sec,
            "client_min_duration_sec": client_duration_sec,
            "server_samples": len(server_rows),
            "server_steady_samples": len(server_steady),
            "client_shards": len(client_sources),
            "client_aggregate_groups": len(client_groups),
            "client_complete_groups": len(complete_client_groups),
            "client_steady_samples": len(client_steady),
            "peak_configured_bots": peak_configured_bots,
            "peak_connected": peak_connected,
            "steady_connected_target_ratio": steady_connected_target_ratio,
            "qualification_24h_1000_clients": qualification,
        },
        "summary": {
            "server": {
                "recv_tps": recv_tps,
                "send_tps": send_tps,
                "final_present": server_final_present,
                "final": {
                    column: server_final[column]
                    for column in (
                        "timestamp_ms",
                        "elapsed_sec",
                        "sessions",
                        "session_objects",
                        "created_sessions",
                        "removed_sessions",
                        "recv_total",
                        "send_total",
                        "recv_bytes",
                        "send_bytes",
                        "auth_total",
                        "protocol_errors",
                        "network_errors",
                        "backpressure_disconnects",
                        "rss_bytes",
                        "virtual_bytes",
                    )
                },
            },
            "client": {
                "aggregation": aggregation,
                "send_tps": client_send_tps,
                "ack_tps": client_ack_tps,
                "rtt_p50_ms": client_rtt_p50,
                "rtt_p95_ms": client_rtt_p95,
                "rtt_p99_ms": client_rtt_p99,
                "final_complete": client_final_complete,
                "missing_final_csv": missing_client_finals,
                "final": {
                    column: client_final[column]
                    for column in sorted(CLIENT_SUM_COLUMNS.difference({"send_tps", "ack_tps"}))
                },
            },
            "sessions": drift,
            "memory": {"rss": rss, "virtual_memory": virtual},
        },
        "checks": checks,
        "warnings": warnings,
    }


def _write_report(path: Path, report: dict[str, Any]) -> None:
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_name(f".{path.name}.tmp-{os.getpid()}")
        try:
            with temporary.open("w", encoding="utf-8", newline="\n") as stream:
                json.dump(report, stream, ensure_ascii=False, indent=2, allow_nan=False)
                stream.write("\n")
            os.replace(temporary, path)
        finally:
            if temporary.exists():
                temporary.unlink()
    except (OSError, TypeError, ValueError) as exc:
        raise AnalysisInputError(f"cannot write JSON report {path}: {exc}") from exc


def _print_report(report: dict[str, Any], output_path: Path) -> None:
    status = report["status"]
    print(f"SOAK ANALYSIS: {status} (run_id={report.get('run_id', 'unknown')})")
    coverage = report.get("coverage", {})
    if coverage:
        print(
            "Coverage: "
            f"duration={coverage['duration_sec']:.3f}s, "
            f"server steady samples={coverage['server_steady_samples']}, "
            f"client steady samples={coverage['client_steady_samples']}, "
            f"peak connected={coverage['peak_connected']}"
        )
        qualification = coverage.get("qualification_24h_1000_clients", {})
        if qualification:
            label = "QUALIFIED" if qualification.get("qualified") else "NOT QUALIFIED"
            print(f"24h / 1000-client soak qualification: {label}")
    for check in report.get("checks", []):
        marker = "PASS" if check["passed"] else "FAIL"
        print(f"[{marker}] {check['name']}: {check['message']}")
    warnings = report.get("warnings", [])
    if warnings:
        print("Warnings:")
        for warning in warnings:
            print(f"  - {warning}")
    print(f"JSON report: {output_path.resolve()}")


def _error_report(message: str, args: argparse.Namespace) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "status": "ERROR",
        "run_id": None,
        "inputs": {
            "server_csv": str(args.server_csv.resolve()),
            "client_csv": [str(path.resolve()) for path in args.client_csv],
            "output": str(args.output.resolve()),
        },
        "errors": [message],
        "checks": [],
        "warnings": [],
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    try:
        report = _analyze(args)
        _write_report(args.output.resolve(), report)
    except AnalysisInputError as exc:
        message = str(exc)
        report = _error_report(message, args)
        try:
            # Do not overwrite an input even while attempting to describe that error.
            output = args.output.resolve()
            inputs = {args.server_csv.resolve(), *(path.resolve() for path in args.client_csv)}
            if output not in inputs:
                _write_report(output, report)
                report_note = f"JSON report: {output}"
            else:
                report_note = "JSON report not written because --output names an input CSV."
        except AnalysisInputError as write_exc:
            report_note = f"JSON report could not be written: {write_exc}"
        print("SOAK ANALYSIS: ERROR", file=sys.stderr)
        print(f"[ERROR] {message}", file=sys.stderr)
        print(report_note, file=sys.stderr)
        return 2

    _print_report(report, args.output)
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
