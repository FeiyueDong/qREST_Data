# qrest_data_tools_ui Agent Notes

Scope UI changes for this subproject to this directory whenever possible.
Prefer updating `main.qml`, `qrest_view_model.h/.cpp`, `qml.qrc`, this
directory's `xmake.lua`, or local documentation before touching shared library
code.

When shared qREST behavior is genuinely required, keep the UI as a consumer of
`qrest_data_lib` and describe the reason clearly. Do not duplicate qREST binary
serialization, packet checksum, metadata parsing, or file-header rules in QML.

Important local contracts:

- `QrestDocument` owns document mode, draft state, dirty tracking, source path,
  validation, and Save As behavior.
- `QrestViewModel` is the C++ facade exposed to QML as
  `DataTools.Backend 1.0`.
- `ChannelTableModel` exposes schema-backed channel metadata. Do not add
  GUI-only persisted fields; display-only derived values such as Direction
  should be computed from existing schema fields.
- `ValidationTableModel` exposes validation results for the QML Validation
  page. Keep issue severity explicit: save should block on Error, not on
  Warning.
- `BinaryTableModel` exposes complete file bytes as offset/hex/ASCII rows for
  the Advanced Binary Viewer. Keep binary viewing read-only and avoid
  pre-formatting the whole file into one giant string.
- `DataTableModel` exposes packet-body samples to `TableView`.
- Packet data is channel-major: `channel * data_point_count + row`.
- Text imports are interpreted as row-major matrices and converted before
  constructing `qrest_data::DataPacket`.
- Metadata, packet header, and file-header sizes must stay synchronized before
  save.
- Existing qREST files must open read-only; modifications require an editable
  draft and must be saved through Save As.

For future metadata UI work, prefer structured C++ properties or focused QML
models over ad hoc string editing. Keep validation explicit for channel counts,
sample counts, timestamp/sample-rate derived fields, and data matrix shape.
The current structured metadata page covers top-level units, BuildingInfo basics,
geolocation, rectangular/circular/polygon footprint parameters, derived
bounding-box refresh, elevation parsing, provider, DataInfo basics, and
StartTime editing; Raw JSON remains an advanced fallback.
The Channels page covers `ChannelID`, `Measurand`, `Scale`, `Azimuth`, and
`LocationXYZ`. It derives `ChannelNo`, `ChannelNum`, and display-only Direction.
Geometry preview data is derived in C++ from the current metadata and rendered in
QML. The first interactive behavior is click-to-select for channel sensors; keep
future geometry calculations out of ad hoc QML logic when they affect
interpretation or validation.
Validation currently checks serialization, metadata shape, derived counts,
channel uniqueness, packet dimensions, sampling consistency, and geometry
warnings. Add field navigation only after the page/navigation structure is
stable.
Text data import should reject channel-count mismatches and ask before replacing
an existing `DataInfo.NPTS` value with an imported row count. Binary viewing
supports offset jump plus ASCII/hex search and should remain read-only.

Build the target with:

```sh
env XMAKE_GLOBALDIR=/tmp/msl_xmake_global xmake build qrest_data_tools_ui
```
