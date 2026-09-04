import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var viewModel

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "Validation"
                font.pixelSize: 18
                font.bold: true
            }
            Label {
                text: root.viewModel.validationStatusText
                color: root.viewModel.validationErrorCount > 0 ? "#c92a2a" : (root.viewModel.validationWarningCount > 0 ? "#9a6700" : "#2b8a3e")
                font.bold: true
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                text: "Run Validation"
                onClicked: root.viewModel.runValidationReport()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 120
                Layout.preferredHeight: 56
                color: "#fff5f5"
                border.color: "#ffc9c9"
                radius: 6
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    Label {
                        text: root.viewModel.validationErrorCount
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "#c92a2a"
                        font.bold: true
                        font.pixelSize: 20
                    }
                    Label {
                        text: "Errors"
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "#7d2a2a"
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 120
                Layout.preferredHeight: 56
                color: "#fff9db"
                border.color: "#ffec99"
                radius: 6
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    Label {
                        text: root.viewModel.validationWarningCount
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "#9a6700"
                        font.bold: true
                        font.pixelSize: 20
                    }
                    Label {
                        text: "Warnings"
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "#7c5f00"
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 120
                Layout.preferredHeight: 56
                color: "#ebfbee"
                border.color: "#b2f2bb"
                radius: 6
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    Label {
                        text: root.viewModel.validationInfoCount
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "#2b8a3e"
                        font.bold: true
                        font.pixelSize: 20
                    }
                    Label {
                        text: "Info"
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "#2f6f3e"
                    }
                }
            }
        }

        GroupBox {
            title: "Issues"
            Layout.fillWidth: true
            Layout.fillHeight: true

            QrestTableView {
                id: validationTable
                anchors.fill: parent
                model: root.viewModel.validationModel
                rowHeight: 32
                severityColors: true
                columnWidthProvider: function (column) {
                    if (column === 0)
                        return 100;
                    if (column === 1)
                        return 140;
                    return Math.max(420, validationTable.width - 240);
                }
            }
        }
    }
}
