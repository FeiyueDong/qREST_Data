import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property string documentTitle: ""
    property string sourceUrl: ""

    function openDocument(title, text, source) {
        documentTitle = title;
        sourceUrl = source;
        viewer.text = text;
        viewer.cursorPosition = 0;
        open();
    }

    title: documentTitle
    modal: true
    width: Math.min(Math.max((parent ? parent.width : 1024) - 120, 520), 920)
    height: Math.min(Math.max((parent ? parent.height : 768) - 120, 420), 720)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        ScrollView {
            id: documentScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextEdit {
                id: viewer
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.MarkdownText
                wrapMode: TextEdit.Wrap
                padding: 12
                font.pixelSize: 14
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignRight

            Label {
                text: root.sourceUrl
                color: "#667085"
                elide: Text.ElideLeft
                Layout.fillWidth: true
            }
            Button {
                text: "Close"
                onClicked: root.close()
            }
        }
    }
}
