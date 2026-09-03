# qREST Data Tools UI

This directory contains the Qt Quick UI target for inspecting and creating
qREST data files. The target is intentionally kept as a small application layer
over `qrest_data_lib`; qREST binary layout, JSON metadata parsing, packet
serialization, and checksum behavior should continue to come from the library
types instead of being reimplemented in QML.

## Target

- Xmake target: `qrest_data_tools_ui`
- Build file: `xmake.lua`
- Main dependency: `qrest_data_lib`
- QML resource bundle: `qml.qrc`
- QML backend type: `QrestViewModel`, registered as
  `DataTools.Backend 1.0`

## Current Structure

- `main.cpp` starts `QGuiApplication`, registers `QrestViewModel`, and loads
  `main.qml` from the Qt resource path.
- `main.qml` defines the current single-window UI:
  - file menu actions for new/open/save qREST files,
  - data menu actions for importing/exporting metadata JSON and packet body
    text,
  - workspace tabs for overview, building metadata, channels, data, and
    validation,
  - advanced dialogs for raw metadata JSON, binary bytes, and header/packet
    inspection,
  - a packet-body `TableView` with row/column selection and copy support.
- `qrest_document.h/.cpp` owns the current document state:
  - `View` for opened read-only files,
  - `EditDraft` for editable copies of opened files,
  - `NewDraft` for newly created files,
  - dirty state, source path, validation, and Save As.
- `qrest_view_model.h/.cpp` provides the QML-facing backend:
  - `QrestViewModel` exposes document state, qREST fields, status messages,
    and file actions to QML,
  - `ChannelTableModel` exposes `InstrumentInfo.Channels` as a table model,
  - geometry properties derive floor outlines and sensor layout points from
    current metadata for lightweight preview rendering,
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
4. `DataTableModel::loadData()` exposes the packet body to `TableView`.

Opened files enter read-only `View` mode. Editing requires `Edit`, which creates
an in-memory draft. Draft changes are never written back to the original file;
the UI only exposes Save As.

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
- Building tab: exposes the first structured metadata editor for document
  units, building basics, geolocation, footprint, elevation, instrument
  provider, and data information. Rectangular, circular, and polygon footprints
  are editable; polygon corner edits refresh the derived bounding box.
- Channels tab: lists channel metadata and edits the schema-backed fields
  `ChannelID`, `Measurand`, `Scale`, `Azimuth`, and `LocationXYZ`. `Direction`
  is display-only and derived from `Azimuth`; `ChannelNo` and `ChannelNum` are
  derived when editing through this page. The selected-channel panel includes a
  lightweight top-view sensor layout preview generated from footprint,
  elevation, channel positions, and channel azimuths, plus an elevation ruler
  for the configured levels.
- Data packet tab: exposes packet header fields, timestamp selection, encoding
  selection, packet-body import/export, and packet-body table browsing/copying.
  Text import checks channel count and asks before replacing an existing NPTS
  value with the imported row count.
- Validation tab: shows the current validation report, separates errors and
  warnings, and is refreshed by the toolbar Validate action.
- Advanced menu:
  - Raw Metadata JSON can view, format, and apply metadata JSON to the current
    draft.
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
- Grow the geometry preview into a dedicated `StructureGeometryModel` and
  richer interactive view. The current preview supports display and sensor
  selection only.
- Add issue-to-field navigation after the main navigation is finalized. The
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
