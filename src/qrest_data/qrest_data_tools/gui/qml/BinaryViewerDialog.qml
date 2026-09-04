import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property var viewModel
    property int searchStartRow: 0

    function openWithReset() {
        searchStartRow = 0;
        open();
    }

    function jumpOffset() {
        const row = root.viewModel.binaryRowForOffset(parseInt(binaryOffsetField.text));
        if (row >= 0) {
            binaryTable.positionViewAtRow(row, TableView.AlignTop);
            root.viewModel.showMessage("已跳转到 offset", false);
        } else {
            root.viewModel.showMessage("Offset 超出文件范围", true);
        }
    }

    function findPattern() {
        const row = binarySearchMode.currentIndex === 0 ? root.viewModel.findBinaryAscii(binarySearchField.text, root.searchStartRow) : root.viewModel.findBinaryHex(binarySearchField.text, root.searchStartRow);
        if (row >= 0) {
            binaryTable.positionViewAtRow(row, TableView.AlignTop);
            root.searchStartRow = row + 1;
            root.viewModel.showMessage("已找到匹配内容", false);
        } else {
            root.viewModel.showMessage("未找到匹配内容", true);
        }
    }

    title: "Binary Viewer"
    modal: true
    width: Math.min((parent ? parent.width : 1024) - 80, 980)
    height: Math.min((parent ? parent.height : 768) - 80, 650)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            text: root.viewModel.binarySummary
            color: "#666666"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Offset"
            }
            TextField {
                id: binaryOffsetField
                Layout.preferredWidth: 120
                validator: IntValidator {
                    bottom: 0
                }
                onAccepted: root.jumpOffset()
            }
            Button {
                text: "Jump"
                onClicked: root.jumpOffset()
            }
            Item {
                Layout.preferredWidth: 12
            }
            ComboBox {
                id: binarySearchMode
                model: ["ASCII", "Hex"]
                Layout.preferredWidth: 100
                onCurrentIndexChanged: root.searchStartRow = 0
            }
            TextField {
                id: binarySearchField
                Layout.fillWidth: true
                placeholderText: binarySearchMode.currentIndex === 0 ? "qREST" : "71 52 45 53"
                onTextChanged: root.searchStartRow = 0
                onAccepted: root.findPattern()
            }
            Button {
                text: "Find Next"
                onClicked: root.findPattern()
            }
        }

        QrestTableView {
            id: binaryTable
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.viewModel.binaryModel
            rowHeight: 28
            numericColumnStart: 0
            columnWidthProvider: function (column) {
                if (column === 0)
                    return 100;
                if (column === 1)
                    return 430;
                return 220;
            }
        }
    }
}
