import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var viewModel
    required property var helpRegistry

    function indexOfValue(values, value) {
        const index = values.indexOf(value);
        return index >= 0 ? index : 0;
    }

    function refreshFields() {
        providerField.text = root.viewModel.provider;
        channelNoLabel.text = root.viewModel.hasSelectedChannel ? root.viewModel.selectedChannelNo : "-";
        channelIdField.text = root.viewModel.selectedChannelId;
        channelDeviceTypeBox.currentIndex = indexOfValue(["Accelerometer", "Velocity Sensor", "Displacement Sensor", "Strain Gauge", "Temperature Sensor", "Unknown", "Other"], root.viewModel.selectedChannelDeviceType);
        channelDeviceTypeBox.editText = root.viewModel.selectedChannelDeviceType;
        channelMeasurandBox.currentIndex = indexOfValue(["Acceleration", "Velocity", "Displacement", "Strain", "Temperature", "Other"], root.viewModel.selectedChannelMeasurand);
        channelMeasurandBox.editText = root.viewModel.selectedChannelMeasurand;
        channelScaleField.text = root.viewModel.hasSelectedChannel ? root.viewModel.selectedChannelScale : "";
        channelAzimuthField.text = root.viewModel.hasSelectedChannel ? root.viewModel.selectedChannelAzimuth : "";
        channelXField.text = root.viewModel.hasSelectedChannel ? root.viewModel.selectedChannelX : "";
        channelYField.text = root.viewModel.hasSelectedChannel ? root.viewModel.selectedChannelY : "";
        channelZField.text = root.viewModel.hasSelectedChannel ? root.viewModel.selectedChannelZ : "";
        channelDirectionLabel.text = root.viewModel.hasSelectedChannel ? root.viewModel.selectedChannelDirection : "-";
    }

    function positionSelectedChannel(row) {
        if (row >= 0) {
            channelTable.positionViewAtRow(row, TableView.AlignCenter);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Channels"
                font.pixelSize: 18
                font.bold: true
            }
            Label {
                text: root.viewModel.canEditChannelOrder ? "" : "通道数量/顺序已锁定"
                color: "#9a6700"
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                text: "Add"
                enabled: root.viewModel.canEditChannelOrder
                onClicked: root.viewModel.addChannel()
            }
            Button {
                text: "Duplicate"
                enabled: root.viewModel.canEditChannelOrder && root.viewModel.hasSelectedChannel
                onClicked: root.viewModel.duplicateSelectedChannel()
            }
            Button {
                text: "Delete"
                enabled: root.viewModel.canEditChannelOrder && root.viewModel.hasSelectedChannel
                onClicked: root.viewModel.deleteSelectedChannel()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "InstrumentInfo.Provider"; fallbackText: "Provider" }
            TextField {
                id: providerField
                readOnly: !root.viewModel.canModify
                Layout.preferredWidth: Math.max(220, root.width * 0.28)
            }
            Button {
                text: "Apply Provider"
                enabled: root.viewModel.canModify
                onClicked: root.viewModel.updateProvider(providerField.text)
            }
            FieldLabel {
                helpRegistry: root.helpRegistry
                fieldKey: "InstrumentInfo.ChannelNum"
                fallbackText: "ChannelNum"
                color: "#666666"
            }
            Label {
                text: root.viewModel.channelNum
                color: "#666666"
            }
            Item {
                Layout.fillWidth: true
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            GroupBox {
                title: "Channel List"
                SplitView.preferredWidth: Math.max(520, root.width * 0.62)
                SplitView.minimumWidth: 460
                Layout.fillHeight: true

                QrestTableView {
                    id: channelTable
                    anchors.fill: parent
                    model: root.viewModel.channelModel
                    selectionModel: root.viewModel.channelSelectionModel
                    alwaysShowHorizontalScrollbar: true
                    numericColumnStart: 5
                    columnWidthProvider: function (column) {
                        if (column === 1)
                            return 180;
                        if (column === 2)
                            return 140;
                        if (column === 3)
                            return 130;
                        if (column === 4)
                            return 110;
                        return 90;
                    }
                    onCellClicked: function (row, column) {
                        root.viewModel.selectChannel(row);
                    }
                }
            }

            GroupBox {
                title: "Selected Channel"
                SplitView.preferredWidth: Math.max(320, root.width * 0.34)
                SplitView.minimumWidth: 280

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 10
                        columnSpacing: 10

                        FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "InstrumentInfo.Channels[].ChannelNo"; fallbackText: "ChannelNo" }
                        Label {
                            id: channelNoLabel
                            text: "-"
                            font.bold: true
                        }

                        FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "InstrumentInfo.Channels[].ChannelID"; fallbackText: "ChannelID" }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                id: channelIdField
                                readOnly: !root.viewModel.canModify || !root.viewModel.hasSelectedChannel
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "Set UNKNOWN"
                                enabled: root.viewModel.canModify && root.viewModel.hasSelectedChannel
                                onClicked: root.viewModel.setSelectedChannelUnknown()
                            }
                        }

                        FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "InstrumentInfo.Channels[].DeviceType"; fallbackText: "DeviceType" }
                        ComboBox {
                            id: channelDeviceTypeBox
                            model: ["Accelerometer", "Velocity Sensor", "Displacement Sensor", "Strain Gauge", "Temperature Sensor", "Unknown", "Other"]
                            editable: true
                            enabled: root.viewModel.canModify && root.viewModel.hasSelectedChannel
                            Layout.fillWidth: true
                        }

                        FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "InstrumentInfo.Channels[].Measurand"; fallbackText: "Measurand" }
                        ComboBox {
                            id: channelMeasurandBox
                            model: ["Acceleration", "Velocity", "Displacement", "Strain", "Temperature", "Other"]
                            editable: true
                            enabled: root.viewModel.canModify && root.viewModel.hasSelectedChannel
                            Layout.fillWidth: true
                        }

                        FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "InstrumentInfo.Channels[].Scale"; fallbackText: "Scale" }
                        TextField {
                            id: channelScaleField
                            readOnly: !root.viewModel.canModify || !root.viewModel.hasSelectedChannel
                            validator: DoubleValidator {}
                            Layout.fillWidth: true
                        }

                        FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "InstrumentInfo.Channels[].Azimuth"; fallbackText: "Azimuth" }
                        TextField {
                            id: channelAzimuthField
                            readOnly: !root.viewModel.canModify || !root.viewModel.hasSelectedChannel
                            validator: DoubleValidator {
                                bottom: -1
                                top: 359.999
                            }
                            Layout.fillWidth: true
                        }

                        FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "InstrumentInfo.Channels[].Direction"; fallbackText: "Direction" }
                        Label {
                            id: channelDirectionLabel
                            text: "-"
                            font.bold: true
                        }

                        FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "InstrumentInfo.Channels[].LocationXYZ.X"; fallbackText: "X (" + root.viewModel.distanceUnit + ")" }
                        TextField {
                            id: channelXField
                            readOnly: !root.viewModel.canModify || !root.viewModel.hasSelectedChannel
                            validator: DoubleValidator {}
                            Layout.fillWidth: true
                        }

                        FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "InstrumentInfo.Channels[].LocationXYZ.Y"; fallbackText: "Y (" + root.viewModel.distanceUnit + ")" }
                        TextField {
                            id: channelYField
                            readOnly: !root.viewModel.canModify || !root.viewModel.hasSelectedChannel
                            validator: DoubleValidator {}
                            Layout.fillWidth: true
                        }

                        FieldLabel { helpRegistry: root.helpRegistry; fieldKey: "InstrumentInfo.Channels[].LocationXYZ.Z"; fallbackText: "Z (" + root.viewModel.distanceUnit + ")" }
                        TextField {
                            id: channelZField
                            readOnly: !root.viewModel.canModify || !root.viewModel.hasSelectedChannel
                            validator: DoubleValidator {}
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Apply Channel"
                            enabled: root.viewModel.canModify && root.viewModel.hasSelectedChannel
                            Layout.columnSpan: 2
                            Layout.alignment: Qt.AlignRight
                            onClicked: root.viewModel.updateSelectedChannel(channelIdField.text, channelDeviceTypeBox.editText, channelMeasurandBox.editText, parseFloat(channelScaleField.text), parseFloat(channelAzimuthField.text), parseFloat(channelXField.text), parseFloat(channelYField.text), parseFloat(channelZField.text))
                        }
                    }

                    GroupBox {
                        title: "Sensor Layout"
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        SensorLayoutView {
                            anchors.fill: parent
                            viewModel: root.viewModel
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: refreshFields()
}
