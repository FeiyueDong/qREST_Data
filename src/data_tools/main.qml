import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import DataTools.Backend

ApplicationWindow {
    id: window
    width: 1024
    height: 768
    visible: true
    title: qsTr("qREST Data Tools")
    color: "#f5f5f5"

    QrestViewModel {
        id: viewModel

        // 监听 C++ 发出的状态消息
        onShowMessage: function (msg, isError) {
            statusText.text = msg;
            statusText.color = isError ? "red" : "#333333";
        }
    }

    // ================= 对话框组件 =================
    FileDialog {
        id: openDialog
        title: "打开 qREST 文件"
        nameFilters: ["qREST Files (*.qrest *.bin)", "All Files (*.*)"]
        onAccepted: viewModel.openFile(selectedFile)
    }

    FileDialog {
        id: saveDialog
        title: "保存 qREST 文件"
        fileMode: FileDialog.SaveFile
        nameFilters: ["qREST Files (*.qrest)", "All Files (*.*)"]
        defaultSuffix: "qrest"
        onAccepted: viewModel.saveFile(selectedFile)
    }

    FileDialog {
        id: importMetaDialog
        title: "导入元数据 (JSON)"
        nameFilters: ["JSON Files (*.json)"]
        onAccepted: viewModel.importMetadata(selectedFile)
    }

    FileDialog {
        id: exportMetaDialog
        title: "导出元数据 (JSON)"
        fileMode: FileDialog.SaveFile
        nameFilters: ["JSON Files (*.json)"]
        defaultSuffix: "json"
        onAccepted: viewModel.exportMetadata(selectedFile)
    }

    FileDialog {
        id: importDataDialog
        title: "导入数据包体 (TXT/CSV)"
        nameFilters: ["Text Files (*.txt *.csv)", "All Files (*.*)"]
        onAccepted: viewModel.importDataBody(selectedFile)
    }

    FileDialog {
        id: exportDataDialog
        title: "导出数据包体 (TXT)"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Text Files (*.txt)"]
        defaultSuffix: "txt"
        onAccepted: viewModel.exportDataBody(selectedFile)
    }

    // ================= 顶部菜单栏 =================
    menuBar: MenuBar {
        Menu {
            title: qsTr("文件 (File)")
            MenuItem {
                text: qsTr("新建 (New)")
                onTriggered: viewModel.newFile()
            }
            MenuItem {
                text: qsTr("打开 (Open)...")
                onTriggered: openDialog.open()
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("保存 (Save)...")
                onTriggered: saveDialog.open()
            }
        }
        Menu {
            title: qsTr("数据 (Data)")
            MenuItem {
                text: qsTr("导入元数据 (Import Meta)...")
                onTriggered: importMetaDialog.open()
            }
            MenuItem {
                text: qsTr("导出元数据 (Export Meta)...")
                onTriggered: exportMetaDialog.open()
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("导入数据包体 (Import Data)...")
                onTriggered: importDataDialog.open()
            }
            MenuItem {
                text: qsTr("导出数据包体 (Export Data)...")
                onTriggered: exportDataDialog.open()
            }
        }
    }

    // ================= 核心主界面 =================
    header: TabBar {
        id: tabBar
        width: parent.width
        TabButton {
            text: "Part 0: 文件头 (Header)"
        }
        TabButton {
            text: "Part 1: 元数据 (Metadata)"
        }
        TabButton {
            text: "Part 2: 数据包 (Data Packet)"
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabBar.currentIndex

        Item {
            SplitView {
                anchors.fill: parent
                anchors.margins: 15
                orientation: Qt.Horizontal

                // 左侧：结构化信息
                ColumnLayout {
                    SplitView.preferredWidth: 300
                    spacing: 20
                    Label {
                        text: "解析信息"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 8
                        border.color: "#ccc"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            Label {
                                text: "Magic (标识):"
                                color: "#666"
                            }
                            Label {
                                text: viewModel.headerMagic
                                font.family: "Consolas"
                                font.bold: true
                                font.pixelSize: 16
                            }
                            Item {
                                height: 10
                            }
                            Label {
                                text: "Metadata Size:"
                                color: "#666"
                            }
                            Label {
                                text: viewModel.metadataSize + " Bytes"
                                font.bold: true
                                color: "#007bff"
                                font.pixelSize: 16
                            }
                            Item {
                                height: 10
                            }
                            Label {
                                text: "Data Size:"
                                color: "#666"
                            }
                            Label {
                                text: viewModel.dataSize + " Bytes"
                                font.bold: true
                                font.pixelSize: 16
                            }
                            Item {
                                Layout.fillHeight: true
                            } // 将内容顶上去
                        }
                    }
                }

                // 右侧：全文件 Hex 预览
                ColumnLayout {
                    spacing: 10
                    Label {
                        text: "全文件二进制预览 (Hex View)"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#1e1e1e"
                        radius: 8
                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 10
                            clip: true
                            TextArea {
                                readOnly: true
                                selectByMouse: true
                                font.family: "Consolas"
                                font.pixelSize: 14
                                color: '#000000' // 经典深色代码配色
                                text: viewModel.fileHexPreview
                            }
                        }
                    }
                }
            }
        }

        // --- 页面 1: 元数据 (退回到稳定的纯文本形式) ---
        Item {
            ScrollView {
                anchors.fill: parent
                anchors.margins: 20
                clip: true
                TextArea {
                    id: metaTextArea
                    width: parent.width
                    font.family: "Consolas"
                    font.pixelSize: 15
                    background: Rectangle {
                        color: "white"
                        border.color: "#ccc"
                        radius: 4
                    }
                    padding: 15
                    // 使用 stringify 格式化输出
                    text: JSON.stringify(viewModel.metadataJson, null, 4)
                    onEditingFinished: {
                        try {
                            viewModel.metadataJson = JSON.parse(text);
                        } catch (e) {
                            viewModel.showMessage("JSON 语法错误，请检查！", true);
                        }
                    }
                }
            }
        }

        // --- 页面 2: 数据包 (Data Packet) ---
        Item {
            // 注册全局快捷键 Ctrl+C
            Shortcut {
                sequence: "Ctrl+C"
                enabled: tabBar.currentIndex === 2 // 仅在当前页激活
                onActivated: {
                    console.log("检测到 Ctrl+C 动作");
                    viewModel.copySelectedCells();
                }
            }
            Shortcut {
                sequence: "Ctrl+A"
                enabled: tabBar.currentIndex === 2
                onActivated: viewModel.selectAllData()
            }
            Connections {
                target: viewModel
                function onPacketUpdated() {
                    tfSourceId.text = viewModel.packetSourceId;
                    tfSampleRate.text = viewModel.packetSamplingRate;
                    tfChannelCount.text = viewModel.packetChannelCount;
                    tfDataPointCount.text = viewModel.packetDataPointCount;
                }
            }
            Dialog {
                id: timePicker
                title: "选择起始时间"
                standardButtons: Dialog.Ok | Dialog.Cancel
                modal: true
                // 使对话框在窗口居中
                x: (window.width - width) / 2
                y: (window.height - height) / 2

                // 当对话框打开时，读取当前输入框的时间戳并反向解析为日期填充到控件中
                onOpened: {
                    let currentTs = parseInt(tfTimestamp.text);
                    // 如果时间戳无效，则默认使用当前系统时间
                    let d = (isNaN(currentTs) || currentTs <= 0) ? new Date() : new Date(currentTs);
                    yearInput.value = d.getFullYear();
                    monthInput.value = d.getMonth() + 1; // JS 的月份是 0-11，需要 +1
                    dayInput.value = d.getDate();
                    hourInput.value = d.getHours();
                    minuteInput.value = d.getMinutes();
                    secondInput.value = d.getSeconds();
                }

                // 当点击“OK”时，将选择的数值组合成 Date 对象，提取毫秒级时间戳赋给文本框
                onAccepted: {
                    let d = new Date(yearInput.value, monthInput.value - 1 // JS 构建日期时月份要 -1
                    , dayInput.value, hourInput.value, minuteInput.value, secondInput.value);
                    tfTimestamp.text = d.getTime().toString();
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

                    // 提供一个“一键设置为现在”的快捷按钮
                    Button {
                        text: "设为当前系统时间"
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignCenter
                        onClicked: {
                            let d = new Date();
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
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 15

                // 上半部：数据包头 (Packet Header) 看板
                GroupBox {
                    title: "包头信息 (Packet Header)"
                    // Layout.fillWidth: true
                    font.bold: true

                    GridLayout {
                        columns: 6
                        rowSpacing: 10
                        columnSpacing: 10
                        anchors.fill: parent

                        Label {
                            text: "数据源 ID:"
                        }
                        TextField {
                            id: tfSourceId
                            validator: IntValidator {
                                bottom: 0
                            }
                            Layout.fillWidth: true
                        }

                        Label {
                            text: "编码方式:"
                        }
                        ComboBox {
                            id: cbEncoding
                            model: ListModel {
                                ListElement {
                                    text: "Float32 (0)"
                                    value: 0
                                }
                                ListElement {
                                    text: "Float64 (1)"
                                    value: 1
                                }
                                ListElement {
                                    text: "Int16 (10)"
                                    value: 10
                                }
                                ListElement {
                                    text: "Int32 (11)"
                                    value: 11
                                }
                            }
                            textRole: "text"
                            currentIndex: [0, 1, 10, 11].indexOf(viewModel.packetDataEncodings)
                        }
                        Label {
                            text: "时间戳 (ms):"
                        }
                        RowLayout {
                            TextField {
                                id: tfTimestamp
                                text: viewModel.packetTimestamp
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "🕒"
                                onClicked: timePicker.open()
                            }
                        }

                        Label {
                            text: "采样率 (Hz):"
                        }
                        TextField {
                            id: tfSampleRate
                            validator: IntValidator {
                                bottom: 1
                            }
                            Layout.fillWidth: true
                        }

                        Label {
                            text: "通道数量:"
                        }
                        TextField {
                            id: tfChannelCount
                            validator: IntValidator {
                                bottom: 1
                            }
                            Layout.fillWidth: true
                        }

                        Label {
                            text: "单通道采样点数:"
                        }
                        TextField {
                            id: tfDataPointCount
                            validator: IntValidator {
                                bottom: 1
                            }
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "同步应用到全文件"
                            Layout.columnSpan: 6
                            Layout.fillWidth: true
                            highlighted: true
                            onClicked: {
                                viewModel.updatePacketHeader(parseInt(tfSourceId.text), parseInt(tfSampleRate.text), parseInt(tfChannelCount.text), parseInt(tfDataPointCount.text), cbEncoding.model.get(cbEncoding.currentIndex).value, parseInt(tfTimestamp.text));
                            }
                        }
                    }
                }

                // 下半部：数据包体 (Packet Body) 表格
                GroupBox {
                    title: "包体数据矩阵 (点击表头选行列，Ctrl+A 全选，Ctrl+C 复制)"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    font.bold: true

                    // 2x2 网格布局，完美还原 Excel 的表头结构
                    GridLayout {
                        anchors.fill: parent
                        // spacing: 0
                        columns: 2
                        rows: 2

                        // 左上角全选块
                        Rectangle {
                            z: 20
                            Layout.preferredWidth: 80 // 增加宽度以匹配时间列
                            Layout.preferredHeight: 30
                            color: "#d6d8db"
                            border.color: "#ccc"
                            Text {
                                anchors.centerIn: parent
                                text: "Time(s)"
                                font.pixelSize: 11
                                color: "#666"
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: viewModel.selectAllData()
                            }
                        }

                        // 横向表头 (通道)
                        HorizontalHeaderView {
                            id: hHeader
                            z: 10
                            syncView: tableView
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            delegate: Rectangle {
                                implicitWidth: 100
                                implicitHeight: 30
                                color: "#e9ecef"
                                border.color: "#ccc"
                                Text {
                                    anchors.centerIn: parent
                                    text: display
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: viewModel.selectColumn(model.column)
                                }
                            }
                        }
                        // 纵向表头 (时间)
                        VerticalHeaderView {
                            id: vHeader
                            z: 10
                            syncView: tableView
                            Layout.fillHeight: true
                            Layout.preferredWidth: 80 // 增加宽度以完整显示 0.0000 格式
                            delegate: Rectangle {
                                implicitWidth: 80
                                implicitHeight: 30
                                color: "#f1f3f5"
                                border.color: "#dee2e6"
                                Text {
                                    anchors.centerIn: parent
                                    // 这里的 display 已经是由 C++ headerData 计算出的时间字符串
                                    text: display
                                    font.family: "Consolas"
                                    font.pixelSize: 12
                                    color: "#495057"
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: viewModel.selectRow(model.row)
                                }
                            }
                        }

                        // 数据表格区
                        TableView {
                            id: tableView
                            z: 11
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            columnSpacing: 1
                            rowSpacing: 1
                            model: viewModel.tableModel
                            selectionModel: viewModel.selectionModel
                            interactive: false

                            rightMargin: ScrollBar.vertical.visible ? ScrollBar.vertical.width : 0
                            bottomMargin: ScrollBar.horizontal.visible ? ScrollBar.horizontal.height : 0

                            ScrollBar.vertical: ScrollBar {
                                z: 11
                                policy: ScrollBar.AlwaysOn
                            }
                            ScrollBar.horizontal: ScrollBar {
                                z: 11
                                policy: ScrollBar.AlwaysOn
                            }

                            SelectionRectangle {
                                target: tableView
                            }

                            MouseArea {
                                anchors.fill: parent
                                // 接受滚轮，但不阻挡点击和拖拽（使得框选正常工作）
                                acceptedButtons: Qt.NoButton
                                onWheel: function (wheel) {
                                    if (wheel.modifiers & Qt.ShiftModifier) {
                                        // 按住 Shift 进行水平滚动
                                        let newX = tableView.contentX - wheel.angleDelta.y;
                                        let maxX = Math.max(0, tableView.contentWidth - tableView.width);
                                        tableView.contentX = Math.max(0, Math.min(newX, maxX));
                                    } else {
                                        // 普通垂直滚动
                                        let newY = tableView.contentY - wheel.angleDelta.y;
                                        let maxY = Math.max(0, tableView.contentHeight - tableView.height);
                                        tableView.contentY = Math.max(0, Math.min(newY, maxY));
                                    }
                                }
                            }

                            delegate: Rectangle {
                                implicitWidth: 100
                                implicitHeight: 30
                                required property bool selected
                                color: selected ? "#0078d7" : ((row % 2 == 0) ? "#ffffff" : "#f8f9fa")
                                border.color: "#eee"

                                Text {
                                    anchors.centerIn: parent
                                    text: display
                                    font.family: "Consolas"
                                    color: selected ? "white" : "black"
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ================= 底部状态栏 =================
    footer: ToolBar {
        background: Rectangle {
            color: "#e0e0e0"
        }
        Label {
            id: statusText
            text: "就绪"
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 10
            font.pixelSize: 13
        }
    }
}
