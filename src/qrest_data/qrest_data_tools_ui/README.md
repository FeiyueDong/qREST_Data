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
  - three tabs for file header, metadata, and data packet,
  - a packet-body `TableView` with row/column selection and copy support.
- `qrest_view_model.h/.cpp` provides the QML-facing backend:
  - `QrestViewModel` owns the current `FileHeader`, `Metadata`, `DataPacket`,
    raw file bytes, status messages, and file operations,
  - `DataTableModel` exposes the packet body to QML as a table model.

## Data Flow

Opening a qREST file reads the full file into memory, then parses it in order:

1. `FileHeader::from_bytes()` reads the fixed 16-byte file header.
2. `Metadata::from_bytes()` parses the JSON metadata block.
3. `DataPacket::from_bytes()` parses and validates the packet header, payload,
   encoding, and checksum.
4. `DataTableModel::loadData()` exposes the packet body to `TableView`.

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

- Header tab: shows parsed file-header sizes and a segmented hex preview of the
  header, metadata, and packet header.
- Metadata tab: currently exposes the full metadata object as editable JSON
  text.
- Data packet tab: exposes packet header fields, timestamp selection, encoding
  selection, and packet-body table browsing/copying.
- Packet header updates also synchronize selected metadata fields:
  - `InstrumentInfo.ChannelNum`
  - `DataInfo.NPTS`
  - `DataInfo.DT`
  - `DataInfo.StartTime`

## Near-Term Improvement Points

- Replace the raw JSON editor with structured controls backed by metadata
  field properties or dedicated metadata models.
- Add a channel table editor for `InstrumentInfo.Channels`; this should keep
  `ChannelNum`, channel list length, and packet channel count consistent.
- Surface validation failures in the UI before saving, especially mismatches
  between metadata, packet dimensions, and imported text matrix shape.
- Improve large-file behavior: the hex preview currently keeps and formats the
  full metadata block, and the table model exposes all packet rows.
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
