import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    required property var helpRegistry
    required property string fieldKey
    property string fallbackText: ""
    property alias color: title.color
    property alias font: title.font

    spacing: 4

    Label {
        id: title
        text: root.helpRegistry.label(root.fieldKey, root.fallbackText)
    }

    Rectangle {
        id: helpBadge
        visible: root.helpRegistry.hasHelp(root.fieldKey)
        Layout.preferredWidth: 15
        Layout.preferredHeight: 15
        radius: 7
        color: "#eef2f6"
        border.color: "#98a2b3"

        Label {
            anchors.centerIn: parent
            text: "i"
            color: "#475467"
            font.pixelSize: 10
            font.bold: true
        }

        HoverHandler {
            id: hoverHandler
        }

        ToolTip.visible: hoverHandler.hovered
        ToolTip.delay: 600
        ToolTip.timeout: 12000
        ToolTip.text: root.helpRegistry.helpText(root.fieldKey)
    }
}
