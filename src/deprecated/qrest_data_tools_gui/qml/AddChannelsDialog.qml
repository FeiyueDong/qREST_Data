pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property var viewModel
    property url sourceUrl: ""

    function openForFile(url) {
        sourceUrl = url;
        previewText.text = root.viewModel.previewAppendDataBody(url);
        previewText.cursorPosition = 0;
        open();
    }

    title: "Add Channels"
    modal: true
    width: Math.min(Math.max((parent ? parent.width : 1024) - 220, 520), 760)
    height: Math.min(Math.max((parent ? parent.height : 768) - 220, 360), 560)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            text: root.sourceUrl.toString()
            elide: Text.ElideLeft
            color: "#667085"
            Layout.fillWidth: true
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                id: previewText
                readOnly: true
                wrapMode: TextArea.Wrap
                font.family: "monospace"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignRight
            spacing: 8

            Button {
                text: "Cancel"
                onClicked: root.close()
            }
            Button {
                text: "Add Channels"
                enabled: previewText.text.indexOf("Compatibility: OK") >= 0
                highlighted: true
                onClicked: {
                    root.viewModel.appendDataBody(root.sourceUrl);
                    root.close();
                }
            }
        }
    }
}
