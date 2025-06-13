
del /f .\flatbuffers\ProtocolID.h

".\flatbuffers\flatc.exe" --cpp --cpp-std c++17 --cpp-ptr-type std::shared_ptr --cpp-str-type std::string --reflect-names --gen-object-api --gen-onefile --gen-all --no-prefix --scoped-enums --no-emit-min-max-enum-values --no-warnings --filename-suffix "" -o .\flatbuffers .\flatbuffers\ProtocoID.fbs

pause