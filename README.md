# neo-recomp

Early scaffold for exploring static recompilation of Neo Geo 68000 program ROMs.

The first goal is deliberately small: load Neo Geo P-ROM bytes, discover a few candidate 68000 entry points, emit or call placeholder generated functions, and keep all platform behavior behind a runtime API.

This is not an emulator yet, and it is not a working recompiler yet.

## Shape

- `recompiler/` owns ROM loading, 68000 decoding, function discovery, and C emission.
- `runtime/` owns the Neo Geo bus/runtime boundary used by generated C.
- `include/ngrecomp/` contains shared public interfaces.
- `games/` contains per-game metadata.

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

## Current CLI

```powershell
.\build\Debug\neo-recomp.exe --game .\games\nam1975.toml --p1 path\to\p1.rom
```

For now this only loads the P-ROM and prints scaffold diagnostics.

