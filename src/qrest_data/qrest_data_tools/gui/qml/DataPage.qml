import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var viewModel
    required property var helpRegistry
    property bool active: false

    signal importDataRequested()
    signal exportDataRequested()
    signal advancedPacketRequested()

    function indexOfValue(values, value) {
        const index = values.indexOf(value);
        return index >= 0 ? index : 0;
    }

    function refreshDataFields() {
        eventNameField.text = root.viewModel.eventName;
        samplingRateField.text = root.viewModel.samplingRate;
        nptsField.text = root.viewModel.dataNpts;
        correctedBox.currentIndex = indexOfValue(["NULL", "Corrected", "Unknown"], root.viewModel.corrected);
    }

    Shortcut {
        sequence: "Ctrl+C"
        enabled: root.active
        onActivated: root.viewModel.copySelectedCells()
    }
    Shortcut {
        sequence: "Ctrl+A"
        enabled: root.active
        onActivated: root.viewModel.selectAllData()
    }

    TimePickerDialog {
        id: timePicker
        viewModel: root.viewModel
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 15

        GroupBox {
            title: "Data Information"
            Layout.fillWidth: true

            GridLayout {
                anchors.fill: parent
                columns: 4
                rowSpacing: 10
                columnSpacing: 10

                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "DataInfo.EventName"; fallbackText: "Event Name" }
                TextField {
                    id: eventNameField
                    readOnly: !root.viewModel.canModify
                    Layout.fillWidth: true
                }
                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "DataInfo.StartTime"; fallbackText: "StartTime" }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: root.viewModel.startTime === "" ? "Auto" : root.viewModel.startTime
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Button {
                        text: "Set"
                        enabled: root.viewModel.canModify
                        onClicked: timePicker.openForDataInfo()
                    }
                }

                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "DataInfo.SamplingRate"; fallbackText: "Sampling Rate (Hz)" }
                TextField {
                    id: samplingRateField
                    readOnly: !root.viewModel.canModify
                    validator: IntValidator {
                        bottom: 1
                        top: 65535
                    }
                    Layout.fillWidth: true
                }
                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "DataInfo.DT"; fallbackText: "Sampling Interval" }
                Label {
                    text: root.viewModel.samplingIntervalText
                    font.bold: true
                }

                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "DataInfo.NPTS"; fallbackText: "NPTS" }
                TextField {
                    id: nptsField
                    readOnly: !root.viewModel.canModify || root.viewModel.packetDataPointCount > 0
                    validator: IntValidator {
                        bottom: 0
                    }
                    Layout.fillWidth: true
                }
                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "DataInfo.Corrected"; fallbackText: "Corrected" }
                ComboBox {
                    id: correctedBox
                    model: ["NULL", "Corrected", "Unknown"]
                    enabled: root.viewModel.canModify
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.columnSpan: 4
                    Layout.alignment: Qt.AlignRight
                    spacing: 8

                    Button {
                        text: "Import Data"
                        enabled: root.viewModel.canModify
                        onClicked: root.importDataRequested()
                    }
                    Button {
                        text: "Export Data"
                        onClicked: root.exportDataRequested()
                    }
                    Button {
                        text: "Packet Header..."
                        onClicked: root.advancedPacketRequested()
                    }
                    Button {
                        text: "Apply Data Info"
                        enabled: root.viewModel.canModify
                        onClicked: root.viewModel.updateDataInfo(eventNameField.text, parseInt(samplingRateField.text), parseInt(nptsField.text), correctedBox.currentText)
                    }
                }
            }
        }

        GroupBox {
            title: "包体数据矩阵 (点击表头选行列，Ctrl+A 全选，Ctrl+C 复制)"
            Layout.fillWidth: true
            Layout.fillHeight: true
            font.bold: true

            QrestTableView {
                anchors.fill: parent
                model: root.viewModel.tableModel
                selectionModel: root.viewModel.selectionModel
                cornerWidth: 80
                cornerText: "Time(s)"
                numericColumnStart: 0
                alwaysShowHorizontalScrollbar: true
                interactive: false
                enableSelectionRectangle: true
                columnWidthProvider: function (column) {
                    return 100;
                }
                onCornerClicked: root.viewModel.selectAllData()
                onHorizontalHeaderClicked: function (column) {
                    root.viewModel.selectColumn(column);
                }
                onVerticalHeaderClicked: function (row) {
                    root.viewModel.selectRow(row);
                }
            }
        }
    }

    Component.onCompleted: {
        refreshDataFields();
    }
}
