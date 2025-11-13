import subprocess
import os
import sys

# ================================================
# PostgreSQL -> Flyway용 깨끗한 스키마 덤프 생성기
# ================================================

DB_HOST = "localhost"
DB_PASSWORD = "dlwnguq1!"
DB_USER = "postgres"
OUT_RAW = "Temp_Schema.sql"
OUT_CLEAN = "GameDB_Schema.sql"


def get_db_name():
    # 파라미터 부족 → 종료
    if len(sys.argv) < 2:
        print("Usage: python convert_dump.py <DB_NAME>")
        sys.exit(1)

    return sys.argv[1]


def run_pg_dump(db_name):
    print("\n=== PostgreSQL schema-only dump Generating... ===")
    env = os.environ.copy()
    env["PGPASSWORD"] = DB_PASSWORD

    cmd = [
        "pg_dump",
        "-U", DB_USER,
        "-w",
        "-h", DB_HOST,
        "-d", db_name,
        "--schema-only",
        "--no-owner",
        "--no-privileges",
        "-F", "p",
        "-f", OUT_RAW
    ]

    result = subprocess.run(cmd, env=env)
    return result.returncode


def clean_dump_file():
    print("\n=== Clean Dump Schema... ===")
    if not os.path.exists(OUT_RAW):
        print("ERROR: Raw dump file not found:", OUT_RAW)
        return False

    with open(OUT_RAW, "r", encoding="utf-8") as f:
        lines = f.readlines()

    cleaned = [line for line in lines if not line.startswith("\\")]

    with open(OUT_CLEAN, "w", encoding="utf-8") as f:
        f.writelines(cleaned)

    return os.path.exists(OUT_CLEAN)


def main():
    # ▶ DB 이름 받기 (명령줄 파라미터)
    db_name = get_db_name()

    # ▶ pg_dump 실행
    code = run_pg_dump(db_name)
    if code != 0:
        print("*** pg_dump Fail Please Check Configure.")
        sys.exit(code)

    # ▶ clean 작업
    if clean_dump_file():
        print(f"Schema SQL is Completed : {OUT_CLEAN}")
    else:
        print("Fail!")

    # ▶ raw 파일 제거
    if os.path.exists(OUT_RAW):
        os.remove(OUT_RAW)

    print("\nDone.")


if __name__ == "__main__":
    main()