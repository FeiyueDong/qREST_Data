# Installed SDK smoke test

This project consumes only an installed qrest_data prefix. It must not add the
repository `include/`, `src/`, or `build/generated/` directories.

Run it from this directory after installing qrest_data:

```sh
xmake f -P . -p linux -m release \
    -o build \
    --qrest_data_sdk=/tmp/qrest_data_sdk
xmake -P .
xmake run -P . installed_sdk_smoke
```

The C++ test includes every public header, links `qrest_data_lib` and
`qrest_data_readers`, and checks version and reader API behavior.
`installed_reader_c_smoke` compiles a pure C consumer of the installed reader
header and shared library. Pass the absolute path of
`resource/test/format_readers/sit_1s.dat` to either binary to verify a real
3-channel, 100-sample read and native calibration. With no argument they verify
the C error/NULL-output contract. Neither project includes repository sources.
