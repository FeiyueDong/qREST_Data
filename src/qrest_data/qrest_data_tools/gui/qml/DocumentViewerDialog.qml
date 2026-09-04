import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property string documentTitle: ""
    property string sourceUrl: ""

    function readResource(url) {
        const request = new XMLHttpRequest();
        request.open("GET", url, false);
        request.send();
        if (request.status === 0 || request.status === 200)
            return request.responseText;
        return "Failed to load document: " + url;
    }

    function openDocument(title, url) {
        documentTitle = title;
        sourceUrl = url;
        viewer.text = readResource(url);
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
