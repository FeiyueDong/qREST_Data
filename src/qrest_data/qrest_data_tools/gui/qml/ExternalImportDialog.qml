pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property var viewModel
    property string importFormat: ""
    property url importUrl: ""
    property var selectedTargets: []
    property bool advancedOpen: false

    function openForImport(format, url) {
        importFormat = format;
        importUrl = url;
        selectedTargets = [];
        advancedOpen = false;
        root.viewModel.clearExternalImport();
        open();
    }

    function selectedPathText() {
        if (root.viewModel.externalImportPath !== "")
            return root.viewModel.externalImportPath;
        if (importUrl === "")
            return "-";
        return importUrl.toString();
    }

    function importTitle() {
        if (importFormat === "tdms")
            return "TDMS";
        if (importFormat === "mseed")
            return "Modified MiniSEED";
        if (importFormat === "hdf5")
            return "HDF5";
        return "External";
    }

    function importOptions() {
        if (importFormat === "tdms") {
            return {
                targetUnit: tdmsTargetUnitBox.currentText,
                sensitivitySelection: tdmsSensitivityBox.currentValue,
                explicitSensitivity: Number(tdmsExplicitSensitivityField.text),
                sensitivityStorageScale: Number(tdmsStorageScaleField.text),
                postScale: Number(tdmsPostScaleField.text),
                outputCounts: tdmsOutputCountsBox.checked,
                verifyTimeAxis: tdmsTimeAxisBox.checked
            };
        }
        if (importFormat === "mseed") {
            return {
                groupIndex: mseedGroupIndexBox.value,
                includeDimensionless: mseedDimensionlessBox.checked,
                gapPolicy: mseedGapPolicyBox.currentValue
            };
        }
        return {};
    }

    function previewImport() {
        selectedTargets = [];
        root.viewModel.loadExternalData(importFormat, importUrl, importOptions());
    }

    function applyMapping() {
        const targets = selectedTargets.slice(0, sourceRepeater.count);
        const used = {};
        for (let i = 0; i < sourceRepeater.count; ++i) {
            if (targets[i] === undefined || targets[i] < 0) {
                root.viewModel.showMessage("Channel mapping is incomplete", true);
                return;
            }
            if (used[targets[i]]) {
                root.viewModel.showMessage("Channel mapping contains duplicate qREST channels", true);
                return;
            }
            used[targets[i]] = true;
        }
        root.viewModel.applyExternalImport(targets);
    }

    function setSelectedTarget(sourceIndex, targetIndex) {
        const targets = selectedTargets.slice();
        targets[sourceIndex] = targetIndex;
        selectedTargets = targets;
    }

    title: importTitle() + " Import"
    modal: true
    width: Math.min(Math.max((parent ? parent.width : 1024) - 120, 660), 960)
    height: Math.min(Math.max((parent ? parent.height : 768) - 100, 520), 760)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 6
            columnSpacing: 14

            Label { text: "Format" }
            Label {
                text: root.viewModel.externalImportFormat === "" ? root.importTitle() : root.viewModel.externalImportFormat
                font.bold: true
            }
            Label { text: "Path" }
            Label {
                text: root.selectedPathText()
                elide: Text.ElideLeft
                Layout.fillWidth: true
            }
            Label { text: "Channels" }
            Label {
                text: root.viewModel.externalImportChannelCount
                font.bold: true
            }
            Label { text: "Samples" }
            Label {
                text: root.viewModel.externalImportSampleCount
                font.bold: true
            }
            Label { text: "Sample Rate" }
            Label {
                text: root.viewModel.externalImportSampleRate > 0 ? root.viewModel.externalImportSampleRate.toFixed(6).replace(/\.?0+$/, "") + " Hz" : "-"
                font.bold: true
            }
        }

        GroupBox {
            title: "Import Options"
            visible: root.importFormat === "tdms" || root.importFormat === "mseed"
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                GridLayout {
                    visible: root.importFormat === "tdms"
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 10
                    rowSpacing: 6

                    Label { text: "Target Unit" }
                    ComboBox {
                        id: tdmsTargetUnitBox
                        model: ["cm/s^2", "m/s^2"]
                        enabled: !root.viewModel.externalImportLoading
                        Layout.preferredWidth: 130
                    }
                    Label { text: "Sensitivity" }
                    ComboBox {
                        id: tdmsSensitivityBox
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: "Acquisition", value: "acquisition" },
                            { text: "First", value: "first" },
                            { text: "Last", value: "last" },
                            { text: "Explicit", value: "explicit" }
                        ]
                        enabled: !root.viewModel.externalImportLoading
                        Layout.preferredWidth: 150
                    }
                    CheckBox {
                        id: tdmsTimeAxisBox
                        text: "Time Verification"
                        checked: true
                        enabled: !root.viewModel.externalImportLoading
                        Layout.columnSpan: 2
                    }
                    CheckBox {
                        id: tdmsOutputCountsBox
                        text: "Raw Counts"
                        checked: false
                        enabled: !root.viewModel.externalImportLoading
                        Layout.columnSpan: 2
                    }
                }

                GridLayout {
                    visible: root.importFormat === "mseed"
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 10
                    rowSpacing: 6

                    Label { text: "Group" }
                    SpinBox {
                        id: mseedGroupIndexBox
                        from: 0
                        to: 9999
                        value: 0
                        enabled: !root.viewModel.externalImportLoading
                        Layout.preferredWidth: 110
                    }
                    Label { text: "Gap Policy" }
                    ComboBox {
                        id: mseedGapPolicyBox
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: "Fill NaN", value: "fill_nan" },
                            { text: "Error", value: "error" },
                            { text: "Ignore", value: "ignore" }
                        ]
                        enabled: !root.viewModel.externalImportLoading
                        Layout.preferredWidth: 140
                    }
                    CheckBox {
                        id: mseedDimensionlessBox
                        text: "Include Dimensionless"
                        checked: false
                        enabled: !root.viewModel.externalImportLoading
                        Layout.columnSpan: 4
                    }
                }

                ToolButton {
                    text: root.advancedOpen ? "Advanced -" : "Advanced +"
                    checkable: true
                    checked: root.advancedOpen
                    visible: root.importFormat === "tdms"
                    enabled: !root.viewModel.externalImportLoading
                    onToggled: root.advancedOpen = checked
                }

                GridLayout {
                    visible: root.importFormat === "tdms" && root.advancedOpen
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 6

                    Label { text: "Explicit Sensitivity" }
                    TextField {
                        id: tdmsExplicitSensitivityField
                        text: "0"
                        enabled: !root.viewModel.externalImportLoading
                        validator: DoubleValidator { bottom: 0 }
                        Layout.fillWidth: true
                    }
                    Label { text: "Storage Scale" }
                    TextField {
                        id: tdmsStorageScaleField
                        text: "100"
                        enabled: !root.viewModel.externalImportLoading
                        validator: DoubleValidator { bottom: 0 }
                        Layout.fillWidth: true
                    }
                    Label { text: "Post Scale" }
                    TextField {
                        id: tdmsPostScaleField
                        text: "1"
                        enabled: !root.viewModel.externalImportLoading
                        validator: DoubleValidator {}
                        Layout.fillWidth: true
                    }
                }
            }
        }

        ProgressBar {
            visible: root.viewModel.externalImportLoading
            indeterminate: root.viewModel.externalImportLoading
            Layout.fillWidth: true
        }

        GroupBox {
            title: "Channel Mapping"
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        text: "External Channel"
                        font.bold: true
                        Layout.fillWidth: true
                    }
                    Label {
                        text: "qREST Channel"
                        font.bold: true
                        Layout.preferredWidth: 260
                    }
                }

                ScrollView {
                    id: mappingScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ColumnLayout {
                        width: Math.max(mappingScroll.availableWidth, 560)
                        spacing: 4

                        Repeater {
                            id: sourceRepeater
                            model: root.viewModel.externalImportSourceChannels

                            RowLayout {
                                id: mappingRow

                                required property var modelData

                                Layout.fillWidth: true
                                spacing: 10

                                Label {
                                    text: "Channel " + (mappingRow.modelData.index + 1) + " / " + mappingRow.modelData.label
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                ComboBox {
                                    id: targetBox
                                    model: root.viewModel.externalImportTargetChannels
                                    textRole: "label"
                                    currentIndex: Math.min(mappingRow.modelData.defaultTarget, count - 1)
                                    enabled: root.viewModel.externalImportReady && !root.viewModel.externalImportLoading
                                    Layout.preferredWidth: 260
                                    Component.onCompleted: root.setSelectedTarget(mappingRow.modelData.index, currentIndex)
                                    onCurrentIndexChanged: root.setSelectedTarget(mappingRow.modelData.index, currentIndex)
                                }
                            }
                        }
                    }
                }
            }
        }

        TextArea {
            text: root.viewModel.externalImportStatus
            readOnly: true
            wrapMode: TextArea.Wrap
            Layout.fillWidth: true
            Layout.preferredHeight: 72
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignRight
            spacing: 8

            Button {
                text: "Preview"
                enabled: !root.viewModel.externalImportLoading && root.importFormat !== "" && root.importUrl !== ""
                onClicked: root.previewImport()
            }
            Button {
                text: "Cancel"
                enabled: root.viewModel.externalImportLoading
                onClicked: root.viewModel.cancelExternalImport()
            }
            Button {
                text: "Close"
                onClicked: root.close()
            }
            Button {
                text: "Apply To Draft"
                enabled: root.viewModel.externalImportReady && !root.viewModel.externalImportLoading && root.viewModel.externalImportTargetChannels.length === root.viewModel.externalImportChannelCount
                highlighted: true
                onClicked: root.applyMapping()
            }
        }
    }
}
