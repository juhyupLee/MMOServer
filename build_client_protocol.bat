
@echo off
setlocal
set OUTPUT_DIR=..\DemoClient\DemoClient\Assets\Scripts\Protocol\Generated

if not exist ".\flatbuffers\flatc.exe" (
    echo ERROR: .\flatbuffers\flatc.exe was not found.
    echo Run powershell -File .\scripts\build_custom_flatbuffers.ps1 first.
    exit /b 1
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

".\flatbuffers\flatc.exe" --csharp --gen-object-api --gen-onefile --gen-all --no-prefix --scoped-enums --no-warnings --filename-suffix "" -o "%OUTPUT_DIR%" .\flatbuffers\ProtocoID.fbs
if errorlevel 1 exit /b %errorlevel%

echo Done. Generated C# files to %OUTPUT_DIR%
