import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property var viewModel

    function refreshJson() {
        rawMetaText.text = JSON.stringify(root.viewModel.metadataJson, null, 4);
    }

    function openWithCurrentJson() {
        refreshJson();
        open();
    }

    title: "Raw Metadata JSON"
    modal: true
    width: Math.min((parent ? parent.width : 1024) - 80, 900)
    height: Math.min((parent ? parent.height : 768) - 80, 650)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    ScrollView {
        anchors.fill: parent
        spacing: 8

        TextArea {
            id: rawMetaText
            Layout.fillWidth: true
            Layout.fillHeight: true
            readOnly: !root.viewModel.canModify
            selectByMouse: true
            font.family: "Consolas"
            font.pixelSize: 13
            wrapMode: TextArea.NoWrap
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                text: "Format"
                onClicked: {
                    try {
                        rawMetaText.text = JSON.stringify(JSON.parse(rawMetaText.text), null, 4);
                        root.viewModel.showMessage("JSON 已格式化", false);
                    } catch (e) {
                        root.viewModel.showMessage("JSON 语法错误，请检查！", true);
                    }
                }
            }
            Button {
                text: "Apply To Draft"
                enabled: root.viewModel.canModify
                onClicked: {
                    try {
                        root.viewModel.metadataJson = JSON.parse(rawMetaText.text);
                        root.close();
                    } catch (e) {
                        root.viewModel.showMessage("JSON 语法错误，请检查！", true);
                    }
                }
            }
            Button {
                text: "Close"
                onClicked: root.close()
            }
        }
    }
}
