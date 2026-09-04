import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

pragma ComponentBehavior: Bound

GridLayout {
    id: root

    required property var model
    property var selectionModel: null
    property var columnWidthProvider: function () {
        return 120;
    }
    property int rowHeight: 30
    property int headerHeight: 30
    property int cornerWidth: 48
    property string cornerText: "#"
    property int numericColumnStart: 9999
    property bool severityColors: false
    property bool alwaysShowHorizontalScrollbar: false
    property bool interactive: true
    property bool enableSelectionRectangle: false
    property bool handleWheel: true

    signal cornerClicked()
    signal cellClicked(int row, int column)
    signal horizontalHeaderClicked(int column)
    signal verticalHeaderClicked(int row)

    function positionViewAtRow(row, mode) {
        table.positionViewAtRow(row, mode);
    }

    columns: 2
    rows: 2

    Rectangle {
        Layout.row: 0
        Layout.column: 0
        Layout.preferredWidth: root.cornerWidth
        Layout.preferredHeight: root.headerHeight
        color: "#d6d8db"
        border.color: "#cccccc"

        Text {
            anchors.centerIn: parent
            text: root.cornerText
            font.pixelSize: 11
            color: "#666666"
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.cornerClicked()
        }
    }

    HorizontalHeaderView {
        id: hHeader
        z: 10
        Layout.row: 0
        Layout.column: 1
        Layout.fillWidth: true
        Layout.preferredHeight: root.headerHeight
        clip: true
        syncView: table

        delegate: Rectangle {
            id: horizontalHeaderCell
            required property int column
            required property var display
            implicitWidth: root.columnWidthProvider(horizontalHeaderCell.column)
            implicitHeight: root.headerHeight
            color: "#e9ecef"
            border.color: "#cccccc"

            Text {
                anchors.fill: parent
                anchors.margins: 5
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: horizontalHeaderCell.display
                font.bold: true
                elide: Text.ElideRight
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.horizontalHeaderClicked(horizontalHeaderCell.column)
            }
        }
    }

    VerticalHeaderView {
        id: vHeader
        z: 10
        Layout.row: 1
        Layout.column: 0
        Layout.fillHeight: true
        Layout.preferredWidth: root.cornerWidth
        clip: true
        syncView: table

        delegate: Rectangle {
            id: verticalHeaderCell
            required property int row
            required property var display
            implicitWidth: root.cornerWidth
            implicitHeight: root.rowHeight
            color: "#f1f3f5"
            border.color: "#dee2e6"

            Text {
                anchors.centerIn: parent
                text: verticalHeaderCell.display
                font.pixelSize: 12
                color: "#495057"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.verticalHeaderClicked(verticalHeaderCell.row)
            }
        }
    }

    TableView {
        id: table
        z: 1
        Layout.row: 1
        Layout.column: 1
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        columnSpacing: 1
        rowSpacing: 1
        model: root.model
        selectionModel: root.selectionModel
        interactive: root.interactive
        rowHeightProvider: function () {
            return root.rowHeight;
        }
        columnWidthProvider: function (column) {
            return root.columnWidthProvider(column);
        }
        rightMargin: ScrollBar.vertical.visible ? ScrollBar.vertical.width : 0
        bottomMargin: ScrollBar.horizontal.visible ? ScrollBar.horizontal.height : 0

        ScrollBar.vertical: ScrollBar {
            z: 20
            policy: ScrollBar.AlwaysOn
        }
        ScrollBar.horizontal: ScrollBar {
            z: 20
            policy: root.alwaysShowHorizontalScrollbar ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        }

        delegate: Rectangle {
            id: cell
            required property bool selected
            required property int row
            required property int column
            required property var display
            implicitWidth: root.columnWidthProvider(cell.column)
            implicitHeight: root.rowHeight
            color: cell.selected ? "#0078d7" : ((cell.row % 2 === 0) ? "#ffffff" : "#f8f9fa")
            border.color: "#eeeeee"

            Text {
                anchors.fill: parent
                anchors.margins: 6
                verticalAlignment: Text.AlignVCenter
                text: cell.display
                elide: Text.ElideRight
                font.family: cell.column >= root.numericColumnStart ? "Consolas" : "Sans Serif"
                color: {
                    if (cell.selected)
                        return "white";
                    if (root.severityColors && cell.column === 0 && cell.display === "Error")
                        return "#c92a2a";
                    if (root.severityColors && cell.column === 0 && cell.display === "Warning")
                        return "#9a6700";
                    if (root.severityColors && cell.column === 0 && cell.display === "Info")
                        return "#2b8a3e";
                    return "#1f2933";
                }
                font.bold: root.severityColors && cell.column === 0
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.cellClicked(cell.row, cell.column)
            }
        }

        SelectionRectangle {
            target: root.enableSelectionRectangle ? table : null
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            enabled: root.handleWheel
            onWheel: function (wheel) {
                if (wheel.modifiers & Qt.ShiftModifier) {
                    const newX = table.contentX - wheel.angleDelta.y;
                    const maxX = Math.max(0, table.contentWidth - table.width);
                    table.contentX = Math.max(0, Math.min(newX, maxX));
                } else {
                    const newY = table.contentY - wheel.angleDelta.y;
                    const maxY = Math.max(0, table.contentHeight - table.height);
                    table.contentY = Math.max(0, Math.min(newY, maxY));
                }
            }
        }
    }
}
