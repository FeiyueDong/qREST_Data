# qrest_data_tools_ui Agent Notes

Scope UI changes for this subproject to this directory whenever possible.
Prefer updating `main.qml`, `qrest_view_model.h/.cpp`, `qml.qrc`, this
directory's `xmake.lua`, or local documentation before touching shared library
code.

When shared qREST behavior is genuinely required, keep the UI as a consumer of
`qrest_data_lib` and describe the reason clearly. Do not duplicate qREST binary
serialization, packet checksum, metadata parsing, or file-header rules in QML.

Important local contracts:

- `QrestViewModel` is the C++ facade exposed to QML as
  `DataTools.Backend 1.0`.
- `DataTableModel` exposes packet-body samples to `TableView`.
- Packet data is channel-major: `channel * data_point_count + row`.
- Text imports are interpreted as row-major matrices and converted before
  constructing `qrest_data::DataPacket`.
- Metadata, packet header, and file-header sizes must stay synchronized before
  save.

For future metadata UI work, prefer structured C++ properties or focused QML
models over ad hoc string editing. Keep validation explicit for channel counts,
sample counts, timestamp/sample-rate derived fields, and data matrix shape.

Build the target with:

```sh
env XMAKE_GLOBALDIR=/tmp/msl_xmake_global xmake build qrest_data_tools_ui
```
