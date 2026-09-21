import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property var viewModel
    required property var helpRegistry

    function encodingIndex(value) {
        const encodings = [0, 1, 10, 11];
        const index = encodings.indexOf(value);
        return index >= 0 ? index : 0;
    }

    function refreshPacketFields() {
        sourceIdField.text = root.viewModel.packetSourceId;
        samplingRateField.text = root.viewModel.packetSamplingRate;
        channelCountField.text = root.viewModel.packetChannelCount;
        dataPointCountField.text = root.viewModel.packetDataPointCount;
        timestampField.text = root.viewModel.packetTimestamp;
        encodingBox.currentIndex = encodingIndex(root.viewModel.packetDataEncodings);
    }

    function openWithCurrentPacket() {
        refreshPacketFields();
        open();
    }

    title: "Header / Packet Inspector"
    modal: true
    width: Math.min(Math.max((parent ? parent.width : 1024) - 120, 520), 760)
    height: Math.min(Math.max((parent ? parent.height : 768) - 120, 520), 680)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    onOpened: refreshPacketFields()

    TimePickerDialog {
        id: timePicker
        viewModel: root.viewModel
        onPacketTimestampSelected: function (timestampText) {
            timestampField.text = timestampText;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        GroupBox {
            title: "File Header"
            Layout.fillWidth: true

            GridLayout {
                anchors.fill: parent
                columns: 2
                rowSpacing: 8
                columnSpacing: 16

                Label {
                    text: "Magic"
                }
                Label {
                    text: root.viewModel.headerMagic
                    font.family: "Consolas"
                    font.bold: true
                }
                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "FileHeader.MetadataSize"; fallbackText: "Metadata Size" }
                Label {
                    text: root.viewModel.metadataSize + " bytes"
                    font.family: "Consolas"
                }
                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "FileHeader.DataSize"; fallbackText: "DataPacket Size" }
                Label {
                    text: root.viewModel.dataSize + " bytes"
                    font.family: "Consolas"
                }
            }
        }

        GroupBox {
            title: "Packet Header"
            Layout.fillWidth: true

            GridLayout {
                anchors.fill: parent
                columns: 4
                rowSpacing: 10
                columnSpacing: 12

                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "Packet.SourceID"; fallbackText: "Source ID" }
                TextField {
                    id: sourceIdField
                    readOnly: !root.viewModel.canModify
                    validator: IntValidator {
                        bottom: 0
                    }
                    Layout.fillWidth: true
                }
                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "Packet.DataEncodings"; fallbackText: "Encoding" }
                ComboBox {
                    id: encodingBox
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
                    Layout.fillWidth: true
                }

                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "Packet.SamplingRate"; fallbackText: "Sampling Rate" }
                TextField {
                    id: samplingRateField
                    readOnly: !root.viewModel.canModify
                    validator: IntValidator {
                        bottom: 1
                    }
                    Layout.fillWidth: true
                }
                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "Packet.ChannelCount"; fallbackText: "Channel Count" }
                TextField {
                    id: channelCountField
                    readOnly: !root.viewModel.canModify
                    validator: IntValidator {
                        bottom: 1
                    }
                    Layout.fillWidth: true
                }

                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "Packet.DataPointCount"; fallbackText: "NPTS" }
                TextField {
                    id: dataPointCountField
                    readOnly: !root.viewModel.canModify
                    validator: IntValidator {
                        bottom: 1
                    }
                    Layout.fillWidth: true
                }
                FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "Packet.Timestamp"; fallbackText: "Timestamp" }
                RowLayout {
                    Layout.fillWidth: true

                    TextField {
                        id: timestampField
                        readOnly: !root.viewModel.canModify
                        Layout.fillWidth: true
                    }
                    Button {
                        text: "Set"
                        enabled: root.viewModel.canModify
                        onClicked: timePicker.openForPacketHeader(timestampField.text)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignRight
            spacing: 8

            Button {
                text: "Refresh"
                onClicked: root.refreshPacketFields()
            }
            Button {
                text: "Apply To File"
                enabled: root.viewModel.canModify
                highlighted: true
                onClicked: {
                    root.viewModel.updatePacketHeader(parseInt(sourceIdField.text), parseInt(samplingRateField.text), parseInt(channelCountField.text), parseInt(dataPointCountField.text), encodingBox.model.get(encodingBox.currentIndex).value, parseInt(timestampField.text));
                }
            }
            Button {
                text: "Close"
                onClicked: root.close()
            }
        }
    }

    Component.onCompleted: refreshPacketFields()
}
