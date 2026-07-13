# MMOServer Custom FlatBuffers

This directory contains the FlatBuffers compiler/runtime source maintained as
part of the MMOServer repository. It is ordinary vendored source, not a Git
submodule or subtree.

- Upstream: `https://github.com/google/flatbuffers.git`
- Imported source commit: `eb08b2ef2f1354b418fc7e2e7828573669ddff82`
- Upstream version reported by `flatc`: `25.12.19`
- Original MMOServer customization commit: `538f8db62381eebf129c0f15b3f1bd89a0fdd320`
- Imported on: `2026-07-13`
- Reference Windows Release artifact SHA-256:
  `D62C3695662B727C2104EB9A681C459AD0A9CB1C0A73DE381CC73C42C696FF9C`
- Reference toolchain: MSVC `19.44.35222`, CMake `3.31.6-msvc6`, Windows x64

## MMOServer customization

`src/idl_parser.cpp` permits C# code generation for unions with an explicit
underlying type. `src/idl_gen_csharp.cpp` emits the union discriminator using
that underlying scalar type. MMOServer needs this because `MessageID` is
declared as `union MessageID : int` and contains values larger than `ubyte`.

## Source of truth

The files in this directory are the source of truth for the MMOServer build of
`flatc`. Do not replace `flatbuffers/flatc.exe` with an unrelated downloaded
binary. Build and promote the compiler, runtime headers, and generated C++ code
together with:

```powershell
.\scripts\build_custom_flatbuffers.ps1
```

The script also verifies that the custom compiler can generate both C++ and C#
from `flatbuffers/ProtocoID.fbs`.
