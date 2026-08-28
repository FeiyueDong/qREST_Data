# Repository Guidelines

## Project Structure & Module Organization

This repository contains the qREST data management library and tools. Core C++ sources live in `src/`: `qrest_data_lib` provides the shared C API library, `qrest_data_hdf5` adds HDF5 support, `data_generator` and `data_loader` are CLI utilities, and `data_tools` is a Qt Quick app. Test targets are kept beside their modules as `src/test_qrest_data_lib` and `src/qrest_data_hdf5/test_*.cpp`. Sample metadata, text data, and `.qrest` files are under `resource/`. Protocol and interface documentation is under `doc/`. The `project/` directory contains generated Visual Studio project files; prefer updating Xmake targets first.

## Build, Test, and Development Commands

- `xmake config -p linux -m debug`: configure a Linux debug build. Use `mingw`, `windows`, or `macosx` when appropriate.
- `xmake build`: build all libraries, tools, and test binaries into `build/<platform>/`.
- `xmake build qrest_data_lib`: build one target while iterating.
- `xmake run test_qrest_data_lib`: run the qREST data library regression test.
- `xmake run test_qrest_data_hdf5`: run HDF5 read/write tests; requires system HDF5.
- `xmake run data_generator resource/metadata.json resource/wuhan/data.txt /tmp/sample.qrest`: generate a sample data file.

## Coding Style & Naming Conventions

Use C++20 and follow the root `.clang-format`, `.clang-tidy`, and `.editorconfig` files. C++ files use 4-space indentation, UTF-8, final newlines, trimmed trailing whitespace, and attached braces. Keep target and directory names lowercase with underscores, matching existing names such as `qrest_data_lib` and `data_generator`. Public headers should be clear and stable; implementation helpers belong in the relevant module directory.

## Testing Guidelines

Add or update tests when changing serialization, metadata handling, byte layout, or HDF5 behavior. Name C++ test files `test_*.cpp` and wire new test binaries through the nearest `xmake.lua`. Run the focused test target first, then `xmake build` before handing off broader changes.

## Commit & Pull Request Guidelines

Recent history uses short, imperative summaries such as `update proj dir` and `add new data`. Keep commits focused and mention the touched area when helpful, for example `update hdf5 reader`. Pull requests should describe the data-format or API impact, list tests run, link related issues, and include screenshots only for `data_tools` UI changes.

## Security & Configuration Tips

Do not commit generated build outputs from `build/`, `out/`, or `.xmake/`. Keep sample data small and non-sensitive. When changing dependencies, update both `xmake.lua` and `vcpkg.json` or `vcpkg-configuration.json` as needed.
