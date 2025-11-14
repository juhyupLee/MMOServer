import subprocess
import os
import sys

# ================================================
# PostgreSQL -> Flyway용 깨끗한 스키마 덤프 생성기
# ================================================

DB_HOST = "localhost"
DB_PASSWORD = "dlwnguq1!"
DB_USER = "postgres"


def getArgs():
    # DB 이름 + 출력 파일명 두 개 필요
    if len(sys.argv) < 3:
        print("Usage: python convert_dump.py <DB_NAME> <OUTPUT_FILE>")
        sys.exit(1)

    dbName = sys.argv[1]
    outFile = sys.argv[2]
    return dbName, outFile


def runPgDump(dbName, rawFile):
    print("\n=== PostgreSQL schema-only dump Generating... ===")
    env = os.environ.copy()
    env["PGPASSWORD"] = DB_PASSWORD

    cmd = [
        "pg_dump",
        "-U", DB_USER,
        "-w",
        "-h", DB_HOST,
        "-d", dbName,
        "--schema-only",
        "--no-owner",
        "--no-privileges",
        "-F", "p",
        "-f", rawFile
    ]

    result = subprocess.run(cmd, env=env)
    return result.returncode


def cleanDumpFile(rawFile, outFile):
    print("\n=== Clean Dump Schema... ===")
    if not os.path.exists(rawFile):
        print("ERROR: Raw dump file not found:", rawFile)
        return False

    with open(rawFile, "r", encoding="utf-8") as f:
        lines = f.readlines()

    cleaned = [line for line in lines if not line.startswith("\\")]

    with open(outFile, "w", encoding="utf-8") as f:
        f.writelines(cleaned)

    return os.path.exists(outFile)


def main():
    dbName, outFile = getArgs()

    rawFile = "Temp_Schema.sql"

    code = runPgDump(dbName, rawFile)
    if code != 0:
        print("*** pg_dump Fail Please Check Configure.")
        sys.exit(code)

    if cleanDumpFile(rawFile, outFile):
        print(f"Schema SQL is Completed : {outFile}")
    else:
        print("Fail!")

    if os.path.exists(rawFile):
        os.remove(rawFile)

    print("\nDone.")


if __name__ == "__main__":
    main()