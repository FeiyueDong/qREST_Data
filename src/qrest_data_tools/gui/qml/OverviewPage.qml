import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var viewModel
    property int maxContentWidth: 1020

    function valueOrDash(value) {
        return value === "" ? "-" : value;
    }

    function durationText() {
        const rate = root.viewModel.samplingRate;
        const npts = root.viewModel.dataNpts;
        if (rate <= 0 || npts <= 0)
            return "-";

        const seconds = npts / rate;
        if (seconds < 120)
            return seconds.toFixed(2) + " s";
        if (seconds < 7200)
            return (seconds / 60).toFixed(2) + " min";
        return (seconds / 3600).toFixed(2) + " h";
    }

    component SummaryTile: Rectangle {
        id: tile

        required property string label
        required property string value
        property color accentColor: "#0b7285"

        Layout.fillWidth: true
        Layout.preferredHeight: 78
        radius: 6
        color: "#ffffff"
        border.color: "#d8dee4"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 4

            Label {
                text: tile.label
                color: "#667085"
                font.pixelSize: 12
            }
            Label {
                text: tile.value
                color: tile.accentColor
                font.pixelSize: 22
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        clip: true

        ColumnLayout {
            width: Math.min(Math.max(root.width - 40, 280), root.maxContentWidth)
            x: Math.max(0, (root.width - width) / 2)
            spacing: 18

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: root.viewModel.projectName === "" ? "Untitled qREST File" : root.viewModel.projectName
                    font.pixelSize: 30
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: root.viewModel.documentStatus
                    color: "#667085"
                    font.pixelSize: 14
                    elide: Text.ElideLeft
                    Layout.fillWidth: true
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.width >= 900 ? 4 : (root.width >= 620 ? 2 : 1)
                rowSpacing: 12
                columnSpacing: 12

                SummaryTile {
                    label: "Channels"
                    value: root.viewModel.channelNum.toString()
                    accentColor: "#0b7285"
                }
                SummaryTile {
                    label: "Elevation"
                    value: root.viewModel.elevationNum + " levels"
                    accentColor: "#6741d9"
                }
                SummaryTile {
                    label: "Sampling"
                    value: root.viewModel.samplingRate + " Hz"
                    accentColor: "#1864ab"
                }
                SummaryTile {
                    label: "Duration"
                    value: root.durationText()
                    accentColor: "#087f5b"
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.width >= 900 ? 2 : 1
                rowSpacing: 14
                columnSpacing: 14

                GroupBox {
                    title: "File / Format"
                    Layout.fillWidth: true

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        rowSpacing: 8
                        columnSpacing: 14

                        Label {
                            text: "File"
                        }
                        Label {
                            text: root.viewModel.sourceFileName
                            font.bold: true
                            elide: Text.ElideLeft
                            Layout.fillWidth: true
                        }
                        Label {
                            text: "Mode"
                        }
                        Label {
                            text: root.viewModel.documentModeName
                            font.bold: true
                        }
                        Label {
                            text: "Version"
                        }
                        Label {
                            text: root.viewModel.metadataVersionText
                            font.bold: true
                        }
                        Label {
                            text: "File Size"
                        }
                        Label {
                            text: root.viewModel.binaryByteCount + " bytes"
                            font.bold: true
                        }
                    }
                }

                GroupBox {
                    title: "Building"
                    Layout.fillWidth: true

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        rowSpacing: 8
                        columnSpacing: 14

                        Label {
                            text: "Structural Type"
                        }
                        Label {
                            text: root.valueOrDash(root.viewModel.structuralType)
                            font.bold: true
                        }
                        Label {
                            text: "Footprint"
                        }
                        Label {
                            text: root.viewModel.footprintShape
                            font.bold: true
                        }
                        Label {
                            text: "Distance Unit"
                        }
                        Label {
                            text: root.viewModel.distanceUnit
                            font.bold: true
                        }
                        Label {
                            text: "Elevation Count"
                        }
                        Label {
                            text: root.viewModel.elevationNum
                            font.bold: true
                        }
                    }
                }

                GroupBox {
                    title: "Instrumentation"
                    Layout.fillWidth: true

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        rowSpacing: 8
                        columnSpacing: 14

                        Label {
                            text: "Provider"
                        }
                        Label {
                            text: root.valueOrDash(root.viewModel.provider)
                            font.bold: true
                        }
                        Label {
                            text: "Channel Count"
                        }
                        Label {
                            text: root.viewModel.channelNum
                            font.bold: true
                        }
                        Label {
                            text: "Channel Order"
                        }
                        Label {
                            text: root.viewModel.canEditChannelOrder ? "Editable" : "Locked"
                            font.bold: true
                        }
                    }
                }

                GroupBox {
                    title: "Data / Validation"
                    Layout.fillWidth: true

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        rowSpacing: 8
                        columnSpacing: 14

                        Label {
                            text: "Event"
                        }
                        Label {
                            text: root.valueOrDash(root.viewModel.eventName)
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: "NPTS"
                        }
                        Label {
                            text: root.viewModel.dataNpts
                            font.bold: true
                        }
                        Label {
                            text: "Sampling Interval"
                        }
                        Label {
                            text: root.viewModel.samplingIntervalText
                            font.bold: true
                        }
                        Label {
                            text: "Validation"
                        }
                        Label {
                            text: root.viewModel.validationStatusText
                            color: root.viewModel.validationErrorCount > 0 ? "#c92a2a" : (root.viewModel.validationWarningCount > 0 ? "#9a6700" : "#2b8a3e")
                            font.bold: true
                        }
                    }
                }
            }
        }
    }
}
