import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property var viewModel

    function refreshJson() {
        rawMetaText.text = JSON.stringify(root.viewModel.metadataJson, null, 4);
        resetViewPosition();
    }

    function resetViewPosition() {
        rawMetaText.cursorPosition = 0;
        jsonScrollView.ScrollBar.vertical.position = 0;
        jsonScrollView.ScrollBar.horizontal.position = 0;
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

    onOpened: Qt.callLater(resetViewPosition)

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        ScrollView {
            id: jsonScrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                id: rawMetaText
                readOnly: !root.viewModel.canModify
                selectByMouse: true
                font.family: "Consolas"
                font.pixelSize: 13
                wrapMode: TextArea.NoWrap

                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse
                    onWheel: function (event) {
                        const bar = jsonScrollView.ScrollBar.vertical;
                        bar.position = Math.max(0, Math.min(1 - bar.size, bar.position - event.angleDelta.y / 2400));
                        event.accepted = true;
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
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
