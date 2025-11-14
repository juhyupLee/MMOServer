import subprocess
import os
import sys
import datetime

DB_HOST = "localhost"
DB_USER = "postgres"
DB_PASSWORD = "dlwnguq1!"

TEMP_BACKUP = "tempBackup.sql"
TEMP_SCHEMA = "tempSchema.sql"
DIFF_FILE = "Diff.sql"

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


def backupDatabase(dbName):
    printMsg("=== Step 0: Backup Database ===")

    timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    
    scriptDir = os.path.dirname(os.path.abspath(__file__))

    # 2) Backup 폴더 경로 만들기
    backupDir = os.path.join(scriptDir, "Backup")
    os.makedirs(backupDir, exist_ok=True)  # 폴더 없으면 자동 생성

    # 3) 백업 파일 전체 경로
    backupFile = os.path.join(
        backupDir, f"{dbName}_{timestamp}_backup.sql"
    )
    
    env = os.environ.copy()
    env["PGPASSWORD"] = DB_PASSWORD

    cmd = [
        "pg_dump",
         "--no-blobs",
        "-U", DB_USER,
        "-w",
        "-h", DB_HOST,
        "-d", dbName,
        "-F", "p",
        "-f", TEMP_BACKUP
    ]

    printMsg("Running: " + " ".join(cmd))

    result = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    if result.returncode != 0:
        printMsg("ERROR: DB backup failed!")
        printMsg(result.stderr.decode())
        return False


    if cleanDumpFile(TEMP_BACKUP,backupFile):
        printMsg(f"Backup created: {backupFile}")
        return True

    return False
    
def printMsg(msg):
    print(msg, flush=True)


def runCreateSchema(dbName):
    printMsg("=== Step 1: Run SchemaGenerator.py ===")

    cmd = [
        "python",
        "SchemaGenerator.py",
        dbName,
        TEMP_SCHEMA
    ]

    printMsg("Running: " + " ".join(cmd))

    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    if result.returncode != 0:
        printMsg("ERROR: CreateSchema.py failed")
        printMsg(result.stderr.decode())
        return False

    if not os.path.exists(TEMP_SCHEMA):
        printMsg("ERROR: temp schema file not generated")
        return False

    return True


def runApgDiff(targetSchemaFile):
    printMsg("=== Step 2: Run apgdiff ===")

    cmd = [
        "java",
        "-jar",
        "apgdiff-2.4.jar",
        TEMP_SCHEMA,
        targetSchemaFile
    ]

    printMsg("Running: " + " ".join(cmd))

    with open(DIFF_FILE, "w", encoding="utf-8") as f:
        result = subprocess.run(cmd, stdout=f, stderr=subprocess.PIPE)

    if result.returncode != 0:
        printMsg("ERROR: apgdiff failed")
        printMsg(result.stderr.decode())
        return False

    if not os.path.exists(DIFF_FILE):
        printMsg("ERROR: Diff.sql not created")
        return False

    return True


def applyDiff(dbName):
    printMsg("=== Step 3: Apply Diff.sql ===")

    if not os.path.exists(DIFF_FILE):
        printMsg("ERROR: Diff.sql not found")
        return False

    env = os.environ.copy()
    env["PGPASSWORD"] = DB_PASSWORD

    cmd = [
        "psql",
        "-U", DB_USER,
        "-w",
        "-h", DB_HOST,
        "-d", dbName,
        "-f", DIFF_FILE
    ]

    printMsg("Running: " + " ".join(cmd))

    result = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    if result.returncode != 0:
        printMsg("ERROR: Failed to apply diff")
        printMsg(result.stderr.decode())
        return False

    return True


def cleanup():
    printMsg("=== Cleanup ===")
    for file in [TEMP_SCHEMA, DIFF_FILE, TEMP_BACKUP]:
        if os.path.exists(file):
            os.remove(file)
            printMsg(f"Deleted: {file}")


def main():
    if len(sys.argv) < 3:
        printMsg("Usage: python UpdateDB.py <TargetDB> <TargetSchemaFile>")
        return

    targetDB = sys.argv[1]
    targetSchemaFile = sys.argv[2]


    if not os.path.exists(targetSchemaFile):
        printMsg("ERROR: Target schema file not found: " + targetSchemaFile)
        return


    # Step 0: Backup Current DB
    if not backupDatabase(targetDB):
        return

    # Step 1: create temp schema via CreateSchema.py
    if not runCreateSchema(targetDB):
        return

    # Step 2: apgdiff
    if not runApgDiff(targetSchemaFile):
        return

    # check diff content
    with open(DIFF_FILE, "r", encoding="utf-8") as f:
        diffContent = f.read().strip()

    if diffContent == "":
        printMsg("No differences. DB already up-to-date.")
        cleanup()
        return

    printMsg("Schema differences detected. Applying updates...")

    # Step 3: Apply diff
    if not applyDiff(targetDB):
        return

    printMsg("DB schema updated successfully.")

    cleanup()


if __name__ == "__main__":
    main()