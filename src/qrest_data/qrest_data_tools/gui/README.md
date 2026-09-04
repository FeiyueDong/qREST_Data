# qREST Data Tools UI

This directory contains the Qt Quick UI target for inspecting and creating
qREST data files. The target is intentionally kept as a small application layer
over `qrest_data_lib`; qREST binary layout, JSON metadata parsing, packet
serialization, and checksum behavior should continue to come from the library
types instead of being reimplemented in QML.

## Target

- Xmake target: `qrest_data_tools_ui`
- Build file: `xmake.lua`
- Main dependencies: `qrest_data_lib`, `qrest_data_tools_core`
- QML resource bundle: `qml.qrc`
- QML backend type: `QrestViewModel`, registered as
  `DataTools.Backend 1.0`

## Current Structure

- `main.cpp` starts `QGuiApplication`, registers `QrestViewModel`, and loads
  `main.qml` from the Qt resource path.
- `main.qml` defines the single-window shell:
  - file menu actions for new/open/save qREST files,
  - data menu actions for importing/exporting metadata JSON and packet body
    text,
  - toolbar actions and workspace tabs for overview, building metadata,
    channels, data, and validation,
  - window-level dirty-document guards and file dialogs.
- `OverviewPage.qml`, `ChannelsPage.qml`, `DataPage.qml`, and
  `ValidationPage.qml` contain the frequently edited page bodies.
- `RawMetadataDialog.qml`, `BinaryViewerDialog.qml`,
  `PacketInspectorDialog.qml`, `DataImportMismatchDialog.qml`, and
  `TimePickerDialog.qml` contain the advanced and workflow dialogs.
- `SensorLayoutView.qml` renders the Channels page sensor layout from C++
  projected geometry data.
- `QrestTableView.qml` provides the shared corner/header/body/scrollbar table
  shell used by the binary, channel, data, and validation tables.
- `qrest_document.h/.cpp` owns the current document state:
  - `View` for opened read-only files,
  - `EditDraft` for editable copies of opened files,
  - `NewDraft` for newly created files,
  - dirty state, source path, validation, and Save As.
- `qrest_view_model.h/.cpp` provides the QML-facing backend:
  - `QrestViewModel` exposes document state, qREST fields, status messages,
    and file actions to QML,
  - `ChannelTableModel` exposes `InstrumentInfo.Channels` as a table model,
  - geometry properties derive 2.5D projected structure edges, sensor points,
    sensor directions, axes, and North direction from current metadata,
  - `ValidationTableModel` exposes validation issues with severity, area, and
    message columns,
  - `BinaryTableModel` exposes complete qREST bytes as offset/hex/ASCII rows,
  - `DataTableModel` exposes the packet body to QML as a table model.

## Data Flow

Opening a qREST file reads the full file into memory, then parses it in order:

1. `FileHeader::from_bytes()` reads the fixed 16-byte file header.
2. `Metadata::from_bytes()` parses the JSON metadata block.
3. `DataPacket::from_bytes()` parses and validates the packet header, payload,
   encoding, and checksum.
4. `DataTableModel::loadData()` exposes the packet body to `QrestTableView`.

Opened files enter read-only `View` mode. Editing requires `Edit`, which creates
an in-memory draft. Draft changes are never written back to the original file;
the UI only exposes Save As. After Save As succeeds, the saved file becomes the
current read-only `View` document.

Saving serializes the same three parts in order:

1. `m_fileHeader.to_bytes()`
2. `m_metadata.to_bytes()`
3. `m_dataPacket.to_bytes()`

The packet body is stored in memory as `double`, but the qREST packet encoding
controls how it is written to bytes. Table access follows the qREST
channel-major layout:

```text
index = channel * data_point_count + row
```

Text data import expects a row-major text matrix, where each row is one sample
time and each column is one channel. The import path converts that matrix to
the qREST channel-major packet layout.

## Current UI Behavior

- Overview tab: summarizes the current document, building, channel, data, and
  validation state.
- Building tab: exposes format summary, document units, building basics,
  geolocation, footprint, and elevation. Rectangular, circular, and polygon
  footprints are editable; polygon corner edits refresh the derived bounding
  box. The time unit is fixed to `s`.
- Channels tab: lists channel metadata and edits the schema-backed fields
  `ChannelID`, `DeviceType`, `Measurand`, `Scale`, `Azimuth`, and
  `LocationXYZ`. `UNKNOWN` is a valid ChannelID shortcut and is not treated as a
  duplicate ID. `Direction` is display-only and derived from `Azimuth`;
  `ChannelNo` and `ChannelNum` are derived when editing through this page. The
  selected-channel panel includes a 2.5D wireframe sensor layout generated from
  footprint, elevation, channel positions, channel azimuths, axes, and North.
- Data packet tab: exposes DataInfo fields, packet-body import/export, packet
  header fields, timestamp selection, encoding selection, and packet-body table
  browsing/copying. Text import checks channel count and asks before replacing
  an existing NPTS value with the imported row count.
- Validation tab: shows the current validation report, separates errors and
  warnings/info, and is refreshed by the toolbar Validate action. Format/core
  validation is shared with `qrest_data_tools_core`; GUI-only engineering
  warnings are appended in the view model.
- Advanced menu:
  - Raw Metadata JSON can view, format, and apply metadata JSON to the current
    draft. Apply restores fixed fields and normalizes derived metadata.
  - Binary Viewer shows all current qREST bytes through a row-based table model,
    with offset jump and ASCII/hex search.
  - Header / Packet Inspector shows low-level file and packet fields.
- Packet header updates also synchronize selected metadata fields:
  - `InstrumentInfo.ChannelNum`
  - `DataInfo.NPTS`
  - `DataInfo.DT`
  - `DataInfo.StartTime`

## Near-Term Improvement Points

- Extend structured metadata coverage beyond the fields exposed in the current
  Building, Channels, and Data pages.
- Extend the channel editor with explicit reordering once the data matrix
  workflow is redesigned. Channel add/delete/duplicate is currently locked
  after packet body data exists, so existing matrix columns are not silently
  remapped.
- Grow the geometry service into a dedicated model if it needs caching,
  selection metadata, or multiple camera modes. The current implementation
  computes projected edges/sensors/axes in `QrestViewModel`.
- Add issue-to-field navigation after the page components settle. The
  validation model currently carries severity, area, and message only.
- Improve large-file behavior: the binary viewer no longer pre-formats the
  whole file, but the data table still exposes all packet rows.
- Keep binary-format edits in `qrest_data_lib`; this UI should orchestrate and
  present library behavior.

## Build And Run

Use the repository's normal Xmake workflow. On Linux, the known global-dir
workaround is:

```sh
env XMAKE_GLOBALDIR=/tmp/msl_xmake_global xmake build qrest_data_tools_ui
```

Run with:

```sh
env XMAKE_GLOBALDIR=/tmp/msl_xmake_global xmake run qrest_data_tools_ui
```

Qt development libraries must be available in the active build environment.
