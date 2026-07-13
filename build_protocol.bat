@echo off
setlocal

if not exist ".\flatbuffers\flatc.exe" (
    echo ERROR: .\flatbuffers\flatc.exe was not found.
    echo Run powershell -File .\scripts\build_custom_flatbuffers.ps1 first.
    exit /b 1
)

".\flatbuffers\flatc.exe" --cpp --cpp-std c++17 --cpp-ptr-type std::shared_ptr --cpp-str-type std::string --reflect-names --gen-object-api --gen-onefile --gen-all --no-prefix --scoped-enums --no-emit-min-max-enum-values --no-warnings --filename-suffix "" -o .\flatbuffers .\flatbuffers\ProtocoID.fbs
if errorlevel 1 exit /b %errorlevel%

echo Done. Generated C++ protocol files in .\flatbuffers
