import argparse
import os
import subprocess

def runCommand(commandArgs):
    process = subprocess.Popen(
        commandArgs,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    stdout, stderr = process.communicate()
    return process.returncode, stdout, stderr

def main():
    parser = argparse.ArgumentParser(description="Create PostgreSQL database and apply schema")
    parser.add_argument("dbName", help="Database name to create")
    parser.add_argument("schemaFile", help="SQL schema file path")
    args = parser.parse_args()

    host = "localhost"
    user = "postgres"
    password = "dlwnguq1!"

    os.environ["PGPASSWORD"] = password

    print("========== Create DB Start ==========")

    createDbCmd = [
        "psql",
        "-h", host,
        "-U", user,
        "-c", f"CREATE DATABASE {args.dbName};"
    ]

    code, out, err = runCommand(createDbCmd)
    if code != 0:
        print("Failed to create database")
        print(err)
        return

    applySchemaCmd = [
        "psql",
        "-h", host,
        "-U", user,
        "-d", args.dbName,
        "-f", args.schemaFile
    ]

    code, out, err = runCommand(applySchemaCmd)
    if code != 0:
        print("Failed to apply schema")
        print(err)
        return

    print("========== Create DB Finished ==========")


if __name__ == "__main__":
    main()