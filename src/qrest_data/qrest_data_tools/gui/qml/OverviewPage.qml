import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var viewModel

    signal navigateRequested(int index)

    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 14

            Label {
                text: root.viewModel.projectName === "" ? "Untitled qREST File" : root.viewModel.projectName
                font.pixelSize: 24
                font.bold: true
            }

            Label {
                text: root.viewModel.documentStatus
                color: "#666666"
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 14
                columnSpacing: 14

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
                            text: root.viewModel.structuralType === "" ? "-" : root.viewModel.structuralType
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
                            text: "Elevation"
                        }
                        Label {
                            text: root.viewModel.elevationSummary
                            font.bold: true
                        }
                    }
                }

                GroupBox {
                    title: "Channels"
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
                            text: root.viewModel.provider === "" ? "-" : root.viewModel.provider
                            font.bold: true
                        }
                        Label {
                            text: "ChannelNum"
                        }
                        Label {
                            text: root.viewModel.channelNum
                            font.bold: true
                        }
                        Label {
                            text: "Order"
                        }
                        Label {
                            text: root.viewModel.canEditChannelOrder ? "Editable" : "Locked"
                            font.bold: true
                        }
                    }
                }

                GroupBox {
                    title: "Data"
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
                            text: root.viewModel.eventName === "" ? "-" : root.viewModel.eventName
                            font.bold: true
                        }
                        Label {
                            text: "Sampling"
                        }
                        Label {
                            text: root.viewModel.samplingRate + " Hz / " + root.viewModel.samplingIntervalText
                            font.bold: true
                        }
                        Label {
                            text: "NPTS"
                        }
                        Label {
                            text: root.viewModel.dataNpts
                            font.bold: true
                        }
                    }
                }

                GroupBox {
                    title: "Validation"
                    Layout.fillWidth: true

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        rowSpacing: 8
                        columnSpacing: 14

                        Label {
                            text: "Status"
                        }
                        Label {
                            text: root.viewModel.validationStatusText
                            color: root.viewModel.validationErrorCount > 0 ? "#c92a2a" : (root.viewModel.validationWarningCount > 0 ? "#9a6700" : "#2b8a3e")
                            font.bold: true
                        }
                        Label {
                            text: "Binary"
                        }
                        Label {
                            text: root.viewModel.binarySummary
                            font.bold: true
                        }
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8
                Button {
                    text: "Building"
                    onClicked: root.navigateRequested(1)
                }
                Button {
                    text: "Channels"
                    onClicked: root.navigateRequested(2)
                }
                Button {
                    text: "Data"
                    onClicked: root.navigateRequested(3)
                }
                Button {
                    text: "Validation"
                    onClicked: root.navigateRequested(4)
                }
            }
        }
    }
}
