
@echo off
set OUTPUT_DIR=..\DemoClient\DemoClient\Assets\Scripts\Protocol\Generated

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

".\flatbuffers\flatc.exe" --csharp --gen-object-api --gen-onefile --gen-all --no-prefix --scoped-enums --no-warnings --filename-suffix "" -o "%OUTPUT_DIR%" .\flatbuffers\ProtocoID.fbs

echo Done. Generated C# files to %OUTPUT_DIR%
pause
