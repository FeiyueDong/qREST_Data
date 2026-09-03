# qrest_data_tools_ui Agent Notes

Scope UI changes for this subproject to this directory whenever possible.
Prefer updating `main.qml`, `qrest_view_model.h/.cpp`, `qml.qrc`, this
directory's `xmake.lua`, or local documentation before touching shared library
code.

When shared qREST behavior is genuinely required, keep the UI as a consumer of
`qrest_data_lib` and describe the reason clearly. Do not duplicate qREST binary
serialization, packet checksum, metadata parsing, or file-header rules in QML.
Second-round exceptions that intentionally live outside this directory are the
core `Metadata` schema in `src/qrest_data/metadata.hpp` and shared validation in
`src/qrest_data/qrest_data_tools/validation.*`; keep those changes schema- and
policy-focused.

Important local contracts:

- `QrestDocument` owns document mode, draft state, dirty tracking, source path,
  validation, and Save As behavior.
- `QrestViewModel` is the C++ facade exposed to QML as
  `DataTools.Backend 1.0`.
- `ChannelTableModel` exposes schema-backed channel metadata. Do not add
  GUI-only persisted fields; display-only derived values such as Direction
  should be computed from existing schema fields.
- Channel metadata includes `DeviceType`. `Direction` stays derived from
  `Azimuth` and must not be serialized as a metadata field.
- `UNKNOWN` is a valid ChannelID value; multiple UNKNOWN IDs are allowed and
  should produce at most one summary warning.
- `ValidationTableModel` exposes validation results for the QML Validation
  page. Core/format validation comes from `qrest_data_tools_core`; GUI-only
  engineering warnings can be appended in `QrestViewModel`. Keep issue severity
  explicit: save should block on Error, not on Warning.
- `BinaryTableModel` exposes complete file bytes as offset/hex/ASCII rows for
  the Advanced Binary Viewer. Keep binary viewing read-only and avoid
  pre-formatting the whole file into one giant string.
- `DataTableModel` exposes packet-body samples to `TableView`.
- `QrestTableView.qml` owns the shared corner/header/body/scrollbar shell for
  binary, channel, data, and validation tables. Prefer extending this component
  over copying another header/table block into a page.
- Packet data is channel-major: `channel * data_point_count + row`.
- Text imports are interpreted as row-major matrices and converted before
  constructing `qrest_data::DataPacket`.
- Metadata, packet header, and file-header sizes must stay synchronized before
  save.
- Existing qREST files must open read-only; modifications require an editable
  draft and must be saved through Save As. After Save As succeeds, the saved
  file becomes the current read-only View document.

For future metadata UI work, prefer structured C++ properties or focused QML
models over ad hoc string editing. Keep validation explicit for channel counts,
sample counts, timestamp/sample-rate derived fields, and data matrix shape.
The current structured metadata pages split responsibilities as Building
(format/units/project/geolocation/footprint/elevation), Channels
(provider/channel table/editor/sensor layout), and Data (DataInfo/import/export
and packet table). Raw JSON remains an advanced fallback and should normalize
fixed and derived fields before applying.
Geometry data is derived in C++ as 2.5D projected structure edges, sensors,
directions, axes, and North; `SensorLayoutView.qml` renders and hit-tests that
projected model. Keep future geometry calculations out of ad hoc QML logic when
they affect interpretation or validation.
Text data import should reject channel-count mismatches and ask before replacing
an existing `DataInfo.NPTS` value with an imported row count. Binary viewing
supports offset jump plus ASCII/hex search and should remain read-only.

Build the target with:

```sh
env XMAKE_GLOBALDIR=/tmp/msl_xmake_global xmake build qrest_data_tools_ui
```
