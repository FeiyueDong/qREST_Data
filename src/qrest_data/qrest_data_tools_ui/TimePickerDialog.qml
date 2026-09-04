import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property var viewModel
    property string target: "packetHeader"
    property string packetTimestampText: ""

    signal packetTimestampSelected(string timestampText)

    function openForDataInfo() {
        target = "dataInfo";
        open();
    }

    function openForPacketHeader(timestampText) {
        target = "packetHeader";
        packetTimestampText = timestampText;
        open();
    }

    title: "选择起始时间"
    standardButtons: Dialog.Ok | Dialog.Cancel
    modal: true
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    onOpened: {
        const currentTs = root.target === "dataInfo" ? root.viewModel.startTimestamp : parseInt(root.packetTimestampText);
        const d = (isNaN(currentTs) || currentTs <= 0) ? new Date() : new Date(currentTs);
        yearInput.value = d.getFullYear();
        monthInput.value = d.getMonth() + 1;
        dayInput.value = d.getDate();
        hourInput.value = d.getHours();
        minuteInput.value = d.getMinutes();
        secondInput.value = d.getSeconds();
    }

    onAccepted: {
        const d = new Date(yearInput.value, monthInput.value - 1, dayInput.value, hourInput.value, minuteInput.value, secondInput.value);
        if (root.target === "dataInfo") {
            root.viewModel.updateStartTimestamp(d.getTime());
        } else {
            root.packetTimestampSelected(d.getTime().toString());
        }
    }

    GridLayout {
        columns: 6
        rowSpacing: 15
        columnSpacing: 15

        Label {
            text: "年:"
        }
        SpinBox {
            id: yearInput
            from: 1970
            to: 2100
            editable: true
            Layout.preferredWidth: 100
        }

        Label {
            text: "月:"
        }
        SpinBox {
            id: monthInput
            from: 1
            to: 12
            editable: true
            Layout.preferredWidth: 100
        }

        Label {
            text: "日:"
        }
        SpinBox {
            id: dayInput
            from: 1
            to: 31
            editable: true
            Layout.preferredWidth: 100
        }

        Label {
            text: "时:"
        }
        SpinBox {
            id: hourInput
            from: 0
            to: 23
            editable: true
            Layout.preferredWidth: 100
        }

        Label {
            text: "分:"
        }
        SpinBox {
            id: minuteInput
            from: 0
            to: 59
            editable: true
            Layout.preferredWidth: 100
        }

        Label {
            text: "秒:"
        }
        SpinBox {
            id: secondInput
            from: 0
            to: 59
            editable: true
            Layout.preferredWidth: 100
        }

        Button {
            text: "设为当前系统时间"
            Layout.columnSpan: 2
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignCenter
            onClicked: {
                const d = new Date();
                yearInput.value = d.getFullYear();
                monthInput.value = d.getMonth() + 1;
                dayInput.value = d.getDate();
                hourInput.value = d.getHours();
                minuteInput.value = d.getMinutes();
                secondInput.value = d.getSeconds();
            }
        }
    }
}
