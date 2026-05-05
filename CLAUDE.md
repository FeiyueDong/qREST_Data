# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

qREST_Data is a sub-project of the qREST system (Quick Response Evaluation for Safety Tagging), responsible for managing seismic/structural monitoring data. It defines two protocols: a **data storage format** (binary file format) and a **data transfer protocol**. The project provides tools for generating, reading, and converting data in the qREST binary format.

## Build System

Uses **xmake** (v3.0.5+) with **vcpkg** for dependency management.

```bash
# Build everything (debug)
xmake

# Build everything (release)
xmake config --mode=release && xmake

# Build a specific target
xmake build data_generator
xmake build data_loader
xmake build qrest_data
xmake build test_qrest_data

# Run tests
xmake run test_qrest_data -- <meta.json> <data.txt> <channel_num> <npts>
```

- **Compiler**: C++20 (clangd configured for C++23 with `-Wall -Wextra`)
- **Dependencies**: `nlohmann-json` (vcpkg on Windows, system on Linux/macOS)
- **Platforms**: Windows, Linux, macOS, MinGW, MSYS
- **Output**: `out/<platform>/bin/` (binaries), `out/<platform>/lib/` (libraries)
- MSVC output goes to `x64/Release/` and `x64/Debug/`

## Architecture

```
src/
├── data_struct/           # Header-only C++ data model classes (namespace: qrest_data)
│   ├── file_header.hpp    # FileHeader: magic "qREST", metadata_size, data_size (16-byte POD)
│   ├── metadata.hpp       # Metadata: JSON-based building/instrument/data info + to/from bytes
│   └── data_packet.hpp    # DataPacket: 32-byte header + body, CRC32 checksum, type encoding
├── qrest_data/            # Shared library exposing a C ABI (extern "C")
│   ├── qrest_data.h       # Public C API header — structures, qrest_from_bytes, qrest_to_bytes
│   └── qrest_data.cpp     # Bridges C structs <-> C++ classes, memory management
├── data_generator/        # CLI binary: metadata.json + data.txt -> output.qrest
├── data_loader/           # CLI binary: input.qrest -> data.txt + metadata.json
├── test_qrest_data/       # Integration test binary exercising the C API round-trip
└── data_tools/            # Qt/QML GUI application (in development, no xmake target)
```

### Binary File Layout

```
FileHeader (16B) | Metadata (JSON, variable) | DataPacket (32B header + variable body)
```

- **FileHeader**: magic `"qREST\0\0\0"`, metadata_size (uint32), data_size (uint32)
- **Metadata**: JSON containing building info, instrument/channel info, data info (NPTS, DT, start time)
- **DataPacket** (32B header POD):
  - magic: `0x7144` ("qD"), source_id, version, packet_type, channel_count
  - data_encodings: 0=Float32, 1=Float64, 10=Int16, 11=Int32
  - sampling_rate, data_point_count, timestamp (ms epoch), body_size, CRC32 checksum
  - Body: channel-sequential doubles (all samples of ch1, then all samples of ch2, ...)

### Key Design Decisions

- Data is always held in memory as `double`; serialization converts to the target encoding (Float32, Float64, Int16, Int32)
- CRC32 is non-reflected (Poly=0x04C11DB7, Init=0xFFFFFFFF, XorOut=0xFFFFFFFF), calculated over packet body only
- `data_struct/` classes are header-only and reusable; `qrest_data` is the only compiled C++ lib
- The C API (`qrest_data.h`) is the **public interface** for external language bindings — all memory is caller-managed via explicit free functions
- Text data files use time-sequential rows (one row per time step, columns = channels); internals and DataPacket use channel-sequential order

## Release Packaging

```bash
python release.py
```

Assembles `release/win/` and `release/linux/` with DLLs/.so, headers, docs, examples, and test source.

## Linting / Formatting

- `.clang-format` and `.clang-tidy` provided at repo root
- `.clangd` configured with C++23, `-Wall -Wextra`, background indexing
