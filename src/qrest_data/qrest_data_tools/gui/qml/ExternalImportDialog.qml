pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property var viewModel
    property var selectedTargets: []

    function openForImport() {
        selectedTargets = [];
        open();
    }

    function applyMapping() {
        const targets = selectedTargets.slice(0, sourceRepeater.count);
        for (let i = 0; i < sourceRepeater.count; ++i) {
            if (targets[i] === undefined || targets[i] < 0) {
                root.viewModel.showMessage("Channel mapping is incomplete", true);
                return;
            }
        }
        root.viewModel.applyExternalImport(targets);
    }

    function setSelectedTarget(sourceIndex, targetIndex) {
        const targets = selectedTargets.slice();
        targets[sourceIndex] = targetIndex;
        selectedTargets = targets;
    }

    title: "External Dataset Preview"
    modal: true
    width: Math.min(Math.max((parent ? parent.width : 1024) - 120, 620), 920)
    height: Math.min(Math.max((parent ? parent.height : 768) - 120, 460), 720)
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
                text: root.viewModel.externalImportFormat === "" ? "-" : root.viewModel.externalImportFormat
                font.bold: true
            }
            Label { text: "Path" }
            Label {
                text: root.viewModel.externalImportPath === "" ? "-" : root.viewModel.externalImportPath
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
