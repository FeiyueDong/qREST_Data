import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property var viewModel

    title: "Header / Packet Inspector"
    modal: true
    width: Math.min((parent ? parent.width : 1024) - 120, 720)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    GridLayout {
        anchors.fill: parent
        columns: 2
        rowSpacing: 10
        columnSpacing: 16

        Label {
            text: "File Magic"
        }
        Label {
            text: root.viewModel.headerMagic
            font.family: "Consolas"
            font.bold: true
        }
        Label {
            text: "Metadata Size"
        }
        Label {
            text: root.viewModel.metadataSize + " bytes"
            font.family: "Consolas"
        }
        Label {
            text: "DataPacket Size"
        }
        Label {
            text: root.viewModel.dataSize + " bytes"
            font.family: "Consolas"
        }
        Label {
            text: "Source ID"
        }
        Label {
            text: root.viewModel.packetSourceId
            font.family: "Consolas"
        }
        Label {
            text: "Encoding"
        }
        Label {
            text: root.viewModel.packetDataEncodings
            font.family: "Consolas"
        }
        Label {
            text: "Packet Channels"
        }
        Label {
            text: root.viewModel.packetChannelCount
            font.family: "Consolas"
        }
        Label {
            text: "Packet NPTS"
        }
        Label {
            text: root.viewModel.packetDataPointCount
            font.family: "Consolas"
        }
        Label {
            text: "Packet Sampling Rate"
        }
        Label {
            text: root.viewModel.packetSamplingRate + " Hz"
            font.family: "Consolas"
        }
        Label {
            text: "Packet Timestamp"
        }
        Label {
            text: root.viewModel.packetTimestamp
            font.family: "Consolas"
        }
    }
}
