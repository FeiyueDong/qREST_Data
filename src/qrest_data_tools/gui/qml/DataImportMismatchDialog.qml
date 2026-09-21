import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property var viewModel
    property string fileUrl: ""

    function prompt(fileUrl, expectedNpts, importedNpts, importedChannels) {
        root.fileUrl = fileUrl;
        messageLabel.text = "当前 NPTS 为 " + expectedNpts + "，导入数据为 " + importedNpts + " 行 / " + importedChannels + " 通道。";
        open();
    }

    title: "NPTS 不一致"
    modal: true
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    ColumnLayout {
        spacing: 12
        Label {
            id: messageLabel
            Layout.preferredWidth: 420
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                text: "Cancel Import"
                onClicked: {
                    root.fileUrl = "";
                    root.close();
                }
            }
            Button {
                text: "Use Imported Value"
                highlighted: true
                onClicked: {
                    const pendingFileUrl = root.fileUrl;
                    root.fileUrl = "";
                    root.close();
                    root.viewModel.confirmImportDataBody(pendingFileUrl);
                }
            }
        }
    }
}
