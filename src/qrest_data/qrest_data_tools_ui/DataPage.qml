import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var viewModel
    property bool active: false

    signal importDataRequested()
    signal exportDataRequested()

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

    function refreshPacketFields() {
        tfSourceId.text = root.viewModel.packetSourceId;
        tfSampleRate.text = root.viewModel.packetSamplingRate;
        tfChannelCount.text = root.viewModel.packetChannelCount;
        tfDataPointCount.text = root.viewModel.packetDataPointCount;
        tfTimestamp.text = root.viewModel.packetTimestamp;
        cbEncoding.currentIndex = [0, 1, 10, 11].indexOf(root.viewModel.packetDataEncodings);
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
        onPacketTimestampSelected: function (timestampText) {
            tfTimestamp.text = timestampText;
        }
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

                Label {
                    text: "Event Name"
                }
                TextField {
                    id: eventNameField
                    readOnly: !root.viewModel.canModify
                    Layout.fillWidth: true
                }
                Label {
                    text: "StartTime"
                }
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

                Label {
                    text: "Sampling Rate (Hz)"
                }
                TextField {
                    id: samplingRateField
                    readOnly: !root.viewModel.canModify
                    validator: IntValidator {
                        bottom: 1
                        top: 65535
                    }
                    Layout.fillWidth: true
                }
                Label {
                    text: "Sampling Interval"
                }
                Label {
                    text: root.viewModel.samplingIntervalText
                    font.bold: true
                }

                Label {
                    text: "NPTS"
                }
                TextField {
                    id: nptsField
                    readOnly: !root.viewModel.canModify || root.viewModel.packetDataPointCount > 0
                    validator: IntValidator {
                        bottom: 0
                    }
                    Layout.fillWidth: true
                }
                Label {
                    text: "Corrected"
                }
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
                        text: "Apply Data Info"
                        enabled: root.viewModel.canModify
                        onClicked: root.viewModel.updateDataInfo(eventNameField.text, parseInt(samplingRateField.text), parseInt(nptsField.text), correctedBox.currentText)
                    }
                }
            }
        }

        GroupBox {
            title: "包头信息 (Packet Header)"
            font.bold: true

            GridLayout {
                columns: 6
                rowSpacing: 10
                columnSpacing: 10
                anchors.fill: parent

                Label {
                    text: "数据源 ID:"
                }
                TextField {
                    id: tfSourceId
                    readOnly: !root.viewModel.canModify
                    validator: IntValidator {
                        bottom: 0
                    }
                    Layout.fillWidth: true
                }

                Label {
                    text: "编码方式:"
                }
                ComboBox {
                    id: cbEncoding
                    enabled: root.viewModel.canModify
                    model: ListModel {
                        ListElement {
                            text: "Float32 (0)"
                            value: 0
                        }
                        ListElement {
                            text: "Float64 (1)"
                            value: 1
                        }
                        ListElement {
                            text: "Int16 (10)"
                            value: 10
                        }
                        ListElement {
                            text: "Int32 (11)"
                            value: 11
                        }
                    }
                    textRole: "text"
                    currentIndex: [0, 1, 10, 11].indexOf(root.viewModel.packetDataEncodings)
                }
                Label {
                    text: "时间戳 (ms):"
                }
                RowLayout {
                    TextField {
                        id: tfTimestamp
                        text: root.viewModel.packetTimestamp
                        readOnly: !root.viewModel.canModify
                        Layout.fillWidth: true
                    }
                    Button {
                        text: "Set"
                        enabled: root.viewModel.canModify
                        onClicked: timePicker.openForPacketHeader(tfTimestamp.text)
                    }
                }

                Label {
                    text: "采样率 (Hz):"
                }
                TextField {
                    id: tfSampleRate
                    readOnly: !root.viewModel.canModify
                    validator: IntValidator {
                        bottom: 1
                    }
                    Layout.fillWidth: true
                }

                Label {
                    text: "通道数量:"
                }
                TextField {
                    id: tfChannelCount
                    readOnly: !root.viewModel.canModify
                    validator: IntValidator {
                        bottom: 1
                    }
                    Layout.fillWidth: true
                }

                Label {
                    text: "单通道采样点数:"
                }
                TextField {
                    id: tfDataPointCount
                    readOnly: !root.viewModel.canModify
                    validator: IntValidator {
                        bottom: 1
                    }
                    Layout.fillWidth: true
                }

                Button {
                    text: "同步应用到全文件"
                    enabled: root.viewModel.canModify
                    Layout.columnSpan: 6
                    Layout.fillWidth: true
                    highlighted: true
                    onClicked: {
                        root.viewModel.updatePacketHeader(parseInt(tfSourceId.text), parseInt(tfSampleRate.text), parseInt(tfChannelCount.text), parseInt(tfDataPointCount.text), cbEncoding.model.get(cbEncoding.currentIndex).value, parseInt(tfTimestamp.text));
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
        refreshPacketFields();
    }
}
