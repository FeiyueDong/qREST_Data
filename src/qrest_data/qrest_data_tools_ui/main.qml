pragma ComponentBehavior: Bound

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
    title: qsTr("qREST Data Tools") + " - " + viewModel.documentStatus
    color: "#f5f5f5"
    property string pendingGuardedAction: ""
    property bool forceClose: false
    property string pendingImportDataUrl: ""
    property int binarySearchStartRow: 0
    property string timePickerTarget: "packetHeader"

    function requestGuardedAction(action) {
        if (viewModel.isDirty) {
            window.pendingGuardedAction = action;
            unsavedDialog.open();
            return;
        }
        window.runGuardedAction(action);
    }

    function runGuardedAction(action) {
        window.pendingGuardedAction = "";
        if (action === "new") {
            viewModel.newFile();
        } else if (action === "open") {
            openDialog.open();
        } else if (action === "close") {
            window.forceClose = true;
            window.close();
        }
    }

    function indexOfValue(values, value) {
        const index = values.indexOf(value);
        return index >= 0 ? index : 0;
    }

    function refreshMetadataFields() {
        distanceUnitBox.currentIndex = indexOfValue(["m", "mm", "cm"], viewModel.distanceUnit);
        timeUnitBox.currentIndex = 0;
        projectNameField.text = viewModel.projectName;
        structuralTypeBox.currentIndex = indexOfValue(["RC Frame", "Shear Wall", "Steel Frame", "Masonry", "Mixed Structure", "Other"], viewModel.structuralType);
        structuralTypeBox.editText = viewModel.structuralType;
        longitudeField.text = viewModel.longitude;
        latitudeField.text = viewModel.latitude;
        northAngleField.text = viewModel.northAngle;
        footprintShapeBox.currentIndex = indexOfValue(["Rectangular", "Circular", "Polygon"], viewModel.footprintShape);
        footprintLengthField.text = viewModel.footprintLength;
        footprintWidthField.text = viewModel.footprintWidth;
        footprintRadiusField.text = viewModel.footprintRadius;
        polygonCornersField.text = viewModel.polygonCornersText;
        elevationField.text = viewModel.elevationText;
        providerField.text = viewModel.provider;
        eventNameField.text = viewModel.eventName;
        samplingRateField.text = viewModel.samplingRate;
        nptsField.text = viewModel.dataNpts;
        correctedBox.currentIndex = indexOfValue(["NULL", "Corrected", "Unknown"], viewModel.corrected);
        rawMetaText.text = JSON.stringify(viewModel.metadataJson, null, 4);
    }

    function refreshChannelFields() {
        channelNoLabel.text = viewModel.hasSelectedChannel ? viewModel.selectedChannelNo : "-";
        channelIdField.text = viewModel.selectedChannelId;
        channelDeviceTypeBox.currentIndex = indexOfValue(["Accelerometer", "Velocity Sensor", "Displacement Sensor", "Strain Gauge", "Temperature Sensor", "Unknown", "Other"], viewModel.selectedChannelDeviceType);
        channelDeviceTypeBox.editText = viewModel.selectedChannelDeviceType;
        channelMeasurandBox.currentIndex = indexOfValue(["Acceleration", "Velocity", "Displacement", "Strain", "Temperature", "Other"], viewModel.selectedChannelMeasurand);
        channelMeasurandBox.editText = viewModel.selectedChannelMeasurand;
        channelScaleField.text = viewModel.hasSelectedChannel ? viewModel.selectedChannelScale : "";
        channelAzimuthField.text = viewModel.hasSelectedChannel ? viewModel.selectedChannelAzimuth : "";
        channelXField.text = viewModel.hasSelectedChannel ? viewModel.selectedChannelX : "";
        channelYField.text = viewModel.hasSelectedChannel ? viewModel.selectedChannelY : "";
        channelZField.text = viewModel.hasSelectedChannel ? viewModel.selectedChannelZ : "";
        channelDirectionLabel.text = viewModel.hasSelectedChannel ? viewModel.selectedChannelDirection : "-";
    }

    function refreshPacketFields() {
        tfSourceId.text = viewModel.packetSourceId;
        tfSampleRate.text = viewModel.packetSamplingRate;
        tfChannelCount.text = viewModel.packetChannelCount;
        tfDataPointCount.text = viewModel.packetDataPointCount;
        tfTimestamp.text = viewModel.packetTimestamp;
        cbEncoding.currentIndex = [0, 1, 10, 11].indexOf(viewModel.packetDataEncodings);
    }

    function jumpBinaryOffset() {
        const row = viewModel.binaryRowForOffset(parseInt(binaryOffsetField.text));
        if (row >= 0) {
            binaryTable.positionViewAtRow(row, TableView.AlignTop);
            statusText.text = "已跳转到 offset";
            statusText.color = "#333333";
        } else {
            viewModel.showMessage("Offset 超出文件范围", true);
        }
    }

    function findBinaryPattern() {
        const row = binarySearchMode.currentIndex === 0 ? viewModel.findBinaryAscii(binarySearchField.text, window.binarySearchStartRow) : viewModel.findBinaryHex(binarySearchField.text, window.binarySearchStartRow);
        if (row >= 0) {
            binaryTable.positionViewAtRow(row, TableView.AlignTop);
            window.binarySearchStartRow = row + 1;
            statusText.text = "已找到匹配内容";
            statusText.color = "#333333";
        } else {
            viewModel.showMessage("未找到匹配内容", true);
        }
    }

    onClosing: function (close) {
        if (!window.forceClose && viewModel.isDirty) {
            close.accepted = false;
            window.pendingGuardedAction = "close";
            unsavedDialog.open();
        }
    }

    QrestViewModel {
        id: viewModel

        // 监听 C++ 发出的状态消息
        onShowMessage: function (msg, isError) {
            statusText.text = msg;
            statusText.color = isError ? "red" : "#333333";
        }
        onMetadataUpdated: window.refreshMetadataFields()
        onPacketUpdated: {
            window.refreshMetadataFields();
            window.refreshPacketFields();
        }
        onChannelsUpdated: window.refreshChannelFields()
        onSelectedChannelUpdated: {
            window.refreshChannelFields();
            if (viewModel.selectedChannelRow >= 0)
                channelTable.positionViewAtRow(viewModel.selectedChannelRow, TableView.AlignCenter);
        }
        onConfirmDataImportNptsMismatch: function (fileUrl, expectedNpts, importedNpts, importedChannels) {
            window.pendingImportDataUrl = fileUrl;
            dataImportMismatchLabel.text = "当前 NPTS 为 " + expectedNpts + "，导入数据为 " + importedNpts + " 行 / " + importedChannels + " 通道。";
            dataImportMismatchDialog.open();
        }
    }

    Component.onCompleted: {
        refreshMetadataFields();
        refreshChannelFields();
        refreshPacketFields();
    }

    // ================= 对话框组件 =================
    Dialog {
        id: unsavedDialog
        title: "未保存的 Draft"
        modal: true
        x: (window.width - width) / 2
        y: (window.height - height) / 2

        ColumnLayout {
            spacing: 12
            Label {
                text: "当前编辑副本包含未保存修改。"
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button {
                    text: "取消"
                    onClicked: {
                        window.pendingGuardedAction = "";
                        unsavedDialog.close();
                    }
                }
                Button {
                    text: "放弃修改"
                    highlighted: true
                    onClicked: {
                        const action = window.pendingGuardedAction;
                        unsavedDialog.close();
                        window.runGuardedAction(action);
                    }
                }
            }
        }
    }

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

    Dialog {
        id: dataImportMismatchDialog
        title: "NPTS 不一致"
        modal: true
        x: (window.width - width) / 2
        y: (window.height - height) / 2

        ColumnLayout {
            spacing: 12
            Label {
                id: dataImportMismatchLabel
                Layout.preferredWidth: 420
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button {
                    text: "Cancel Import"
                    onClicked: {
                        window.pendingImportDataUrl = "";
                        dataImportMismatchDialog.close();
                    }
                }
                Button {
                    text: "Use Imported Value"
                    highlighted: true
                    onClicked: {
                        const fileUrl = window.pendingImportDataUrl;
                        window.pendingImportDataUrl = "";
                        dataImportMismatchDialog.close();
                        viewModel.confirmImportDataBody(fileUrl);
                    }
                }
            }
        }
    }

    Dialog {
        id: rawMetadataDialog
        title: "Raw Metadata JSON"
        modal: true
        width: Math.min(window.width - 80, 900)
        height: Math.min(window.height - 80, 650)
        x: (window.width - width) / 2
        y: (window.height - height) / 2

        ScrollView {
            anchors.fill: parent
            spacing: 8

            TextArea {
                id: rawMetaText
                Layout.fillWidth: true
                Layout.fillHeight: true
                readOnly: !viewModel.canModify
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
                            viewModel.showMessage("JSON 已格式化", false);
                        } catch (e) {
                            viewModel.showMessage("JSON 语法错误，请检查！", true);
                        }
                    }
                }
                Button {
                    text: "Apply To Draft"
                    enabled: viewModel.canModify
                    onClicked: {
                        try {
                            viewModel.metadataJson = JSON.parse(rawMetaText.text);
                            rawMetadataDialog.close();
                        } catch (e) {
                            viewModel.showMessage("JSON 语法错误，请检查！", true);
                        }
                    }
                }
                Button {
                    text: "Close"
                    onClicked: rawMetadataDialog.close()
                }
            }
        }
    }

    Dialog {
        id: binaryViewerDialog
        title: "Binary Viewer"
        modal: true
        width: Math.min(window.width - 80, 980)
        height: Math.min(window.height - 80, 650)
        x: (window.width - width) / 2
        y: (window.height - height) / 2

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            Label {
                text: viewModel.binarySummary
                color: "#666666"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: "Offset"
                }
                TextField {
                    id: binaryOffsetField
                    Layout.preferredWidth: 120
                    validator: IntValidator {
                        bottom: 0
                    }
                    onAccepted: window.jumpBinaryOffset()
                }
                Button {
                    text: "Jump"
                    onClicked: window.jumpBinaryOffset()
                }
                Item {
                    Layout.preferredWidth: 12
                }
                ComboBox {
                    id: binarySearchMode
                    model: ["ASCII", "Hex"]
                    Layout.preferredWidth: 100
                    onCurrentIndexChanged: window.binarySearchStartRow = 0
                }
                TextField {
                    id: binarySearchField
                    Layout.fillWidth: true
                    placeholderText: binarySearchMode.currentIndex === 0 ? "qREST" : "71 52 45 53"
                    onTextChanged: window.binarySearchStartRow = 0
                    onAccepted: window.findBinaryPattern()
                }
                Button {
                    text: "Find Next"
                    onClicked: window.findBinaryPattern()
                }
            }

            QrestTableView {
                id: binaryTable
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: viewModel.binaryModel
                rowHeight: 28
                numericColumnStart: 0
                columnWidthProvider: function (column) {
                    if (column === 0)
                        return 100;
                    if (column === 1)
                        return 430;
                    return 220;
                }
            }
        }
    }

    Dialog {
        id: inspectorDialog
        title: "Header / Packet Inspector"
        modal: true
        width: Math.min(window.width - 120, 720)
        x: (window.width - width) / 2
        y: (window.height - height) / 2

        GridLayout {
            anchors.fill: parent
            columns: 2
            rowSpacing: 10
            columnSpacing: 16

            Label {
                text: "File Magic"
            }
            Label {
                text: viewModel.headerMagic
                font.family: "Consolas"
                font.bold: true
            }
            Label {
                text: "Metadata Size"
            }
            Label {
                text: viewModel.metadataSize + " bytes"
                font.family: "Consolas"
            }
            Label {
                text: "DataPacket Size"
            }
            Label {
                text: viewModel.dataSize + " bytes"
                font.family: "Consolas"
            }
            Label {
                text: "Source ID"
            }
            Label {
                text: viewModel.packetSourceId
                font.family: "Consolas"
            }
            Label {
                text: "Encoding"
            }
            Label {
                text: viewModel.packetDataEncodings
                font.family: "Consolas"
            }
            Label {
                text: "Packet Channels"
            }
            Label {
                text: viewModel.packetChannelCount
                font.family: "Consolas"
            }
            Label {
                text: "Packet NPTS"
            }
            Label {
                text: viewModel.packetDataPointCount
                font.family: "Consolas"
            }
            Label {
                text: "Packet Sampling Rate"
            }
            Label {
                text: viewModel.packetSamplingRate + " Hz"
                font.family: "Consolas"
            }
            Label {
                text: "Packet Timestamp"
            }
            Label {
                text: viewModel.packetTimestamp
                font.family: "Consolas"
            }
        }
    }

    // ================= 顶部菜单栏 =================
    menuBar: MenuBar {
        Menu {
            title: qsTr("文件 (File)")
            MenuItem {
                text: qsTr("新建 (New)")
                onTriggered: window.requestGuardedAction("new")
            }
            MenuItem {
                text: qsTr("打开 (Open)...")
                onTriggered: window.requestGuardedAction("open")
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("另存为 (Save As)...")
                enabled: viewModel.canSaveAs
                onTriggered: saveDialog.open()
            }
        }
        Menu {
            title: qsTr("数据 (Data)")
            MenuItem {
                text: qsTr("导入元数据 (Import Meta)...")
                enabled: viewModel.canModify
                onTriggered: importMetaDialog.open()
            }
            MenuItem {
                text: qsTr("导出元数据 (Export Meta)...")
                onTriggered: exportMetaDialog.open()
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("导入数据包体 (Import Data)...")
                enabled: viewModel.canModify
                onTriggered: importDataDialog.open()
            }
            MenuItem {
                text: qsTr("导出数据包体 (Export Data)...")
                onTriggered: exportDataDialog.open()
            }
        }
        Menu {
            title: qsTr("高级 (Advanced)")
            MenuItem {
                text: qsTr("Raw Metadata JSON...")
                onTriggered: {
                    window.refreshMetadataFields();
                    rawMetadataDialog.open();
                }
            }
            MenuItem {
                text: qsTr("Binary Viewer...")
                onTriggered: {
                    window.binarySearchStartRow = 0;
                    binaryViewerDialog.open();
                }
            }
            MenuItem {
                text: qsTr("Header / Packet Inspector...")
                onTriggered: inspectorDialog.open()
            }
        }
    }

    // ================= 核心主界面 =================
    header: ColumnLayout {
        width: parent.width
        spacing: 0

        ToolBar {
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                ToolButton {
                    text: "New"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_ui/icon/New.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Create new qREST file"
                    onClicked: window.requestGuardedAction("new")
                }
                ToolButton {
                    text: "Open"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_ui/icon/Open.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Open qREST file"
                    onClicked: window.requestGuardedAction("open")
                }
                ToolSeparator {}
                ToolButton {
                    text: "Edit"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_ui/icon/Edit.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Create editable copy"
                    enabled: viewModel.canEdit
                    onClicked: viewModel.beginEdit()
                }
                ToolButton {
                    text: "Validate"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_ui/icon/Validate.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Validate current document"
                    onClicked: {
                        viewModel.validateDocument();
                        tabBar.currentIndex = 4;
                    }
                }
                ToolButton {
                    text: "Save As"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_ui/icon/SaveAs.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Save draft as a new file"
                    enabled: viewModel.canSaveAs
                    highlighted: viewModel.isDirty
                    onClicked: saveDialog.open()
                }
                ToolSeparator {}
                ToolButton {
                    text: "JSON"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_ui/icon/JSON.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Open raw metadata JSON"
                    onClicked: {
                        window.refreshMetadataFields();
                        rawMetadataDialog.open();
                    }
                }
                ToolButton {
                    text: "Binary"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_ui/icon/Binary.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Open read-only binary viewer"
                    onClicked: {
                        window.binarySearchStartRow = 0;
                        binaryViewerDialog.open();
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    text: viewModel.documentStatus
                    font.bold: true
                    elide: Text.ElideLeft
                    Layout.maximumWidth: 360
                }
            }
        }

        TabBar {
            id: tabBar
            Layout.fillWidth: true
            TabButton {
                text: "Overview"
            }
            TabButton {
                text: "Building"
            }
            TabButton {
                text: "Channels"
            }
            TabButton {
                text: "Data"
            }
            TabButton {
                text: viewModel.validationErrorCount > 0 ? "Validation (" + viewModel.validationErrorCount + "E)" : (viewModel.validationWarningCount > 0 ? "Validation (" + viewModel.validationWarningCount + "W)" : "Validation")
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabBar.currentIndex

        Item {
            ScrollView {
                anchors.fill: parent
                anchors.margins: 20
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 14

                    Label {
                        text: viewModel.projectName === "" ? "Untitled qREST File" : viewModel.projectName
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Label {
                        text: viewModel.documentStatus
                        color: "#666666"
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 14
                        columnSpacing: 14

                        GroupBox {
                            title: "Building"
                            Layout.fillWidth: true

                            GridLayout {
                                anchors.fill: parent
                                columns: 2
                                rowSpacing: 8
                                columnSpacing: 14

                                Label {
                                    text: "Structural Type"
                                }
                                Label {
                                    text: viewModel.structuralType === "" ? "-" : viewModel.structuralType
                                    font.bold: true
                                }
                                Label {
                                    text: "Footprint"
                                }
                                Label {
                                    text: viewModel.footprintShape
                                    font.bold: true
                                }
                                Label {
                                    text: "Elevation"
                                }
                                Label {
                                    text: viewModel.elevationSummary
                                    font.bold: true
                                }
                            }
                        }

                        GroupBox {
                            title: "Channels"
                            Layout.fillWidth: true

                            GridLayout {
                                anchors.fill: parent
                                columns: 2
                                rowSpacing: 8
                                columnSpacing: 14

                                Label {
                                    text: "Provider"
                                }
                                Label {
                                    text: viewModel.provider === "" ? "-" : viewModel.provider
                                    font.bold: true
                                }
                                Label {
                                    text: "ChannelNum"
                                }
                                Label {
                                    text: viewModel.channelNum
                                    font.bold: true
                                }
                                Label {
                                    text: "Order"
                                }
                                Label {
                                    text: viewModel.canEditChannelOrder ? "Editable" : "Locked"
                                    font.bold: true
                                }
                            }
                        }

                        GroupBox {
                            title: "Data"
                            Layout.fillWidth: true

                            GridLayout {
                                anchors.fill: parent
                                columns: 2
                                rowSpacing: 8
                                columnSpacing: 14

                                Label {
                                    text: "Event"
                                }
                                Label {
                                    text: viewModel.eventName === "" ? "-" : viewModel.eventName
                                    font.bold: true
                                }
                                Label {
                                    text: "Sampling"
                                }
                                Label {
                                    text: viewModel.samplingRate + " Hz / " + viewModel.samplingIntervalText
                                    font.bold: true
                                }
                                Label {
                                    text: "NPTS"
                                }
                                Label {
                                    text: viewModel.dataNpts
                                    font.bold: true
                                }
                            }
                        }

                        GroupBox {
                            title: "Validation"
                            Layout.fillWidth: true

                            GridLayout {
                                anchors.fill: parent
                                columns: 2
                                rowSpacing: 8
                                columnSpacing: 14

                                Label {
                                    text: "Status"
                                }
                                Label {
                                    text: viewModel.validationStatusText
                                    color: viewModel.validationErrorCount > 0 ? "#c92a2a" : (viewModel.validationWarningCount > 0 ? "#9a6700" : "#2b8a3e")
                                    font.bold: true
                                }
                                Label {
                                    text: "Binary"
                                }
                                Label {
                                    text: viewModel.binarySummary
                                    font.bold: true
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignRight
                        spacing: 8
                        Button {
                            text: "Building"
                            onClicked: tabBar.currentIndex = 1
                        }
                        Button {
                            text: "Channels"
                            onClicked: tabBar.currentIndex = 2
                        }
                        Button {
                            text: "Data"
                            onClicked: tabBar.currentIndex = 3
                        }
                        Button {
                            text: "Validation"
                            onClicked: tabBar.currentIndex = 4
                        }
                    }
                }
            }
        }

        // --- 页面 1: 元数据结构化编辑 ---
        Item {
            ScrollView {
                anchors.fill: parent
                anchors.margins: 20
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 14

                    GroupBox {
                        title: "Document"
                        Layout.fillWidth: true

                        GridLayout {
                            anchors.fill: parent
                            columns: 4
                            rowSpacing: 10
                            columnSpacing: 10

                            Label {
                                text: "Header"
                            }
                            Label {
                                text: viewModel.metadataHeader
                                font.bold: true
                            }
                            Label {
                                text: "Version"
                            }
                            Label {
                                text: viewModel.metadataVersionText
                                font.bold: true
                            }

                            Label {
                                text: "Distance Unit"
                            }
                            ComboBox {
                                id: distanceUnitBox
                                model: ["m", "mm", "cm"]
                                enabled: viewModel.canModify
                                Layout.fillWidth: true
                            }
                            Label {
                                text: "Time Unit"
                            }
                            ComboBox {
                                id: timeUnitBox
                                model: ["s"]
                                enabled: false
                                Layout.fillWidth: true
                            }

                            Button {
                                text: "Apply Units"
                                enabled: viewModel.canModify
                                Layout.columnSpan: 4
                                Layout.alignment: Qt.AlignRight
                                onClicked: viewModel.updateUnits(distanceUnitBox.currentText, timeUnitBox.currentText)
                            }
                        }
                    }

                    GroupBox {
                        title: "Building"
                        Layout.fillWidth: true

                        GridLayout {
                            anchors.fill: parent
                            columns: 4
                            rowSpacing: 10
                            columnSpacing: 10

                            Label {
                                text: "Project Name"
                            }
                            TextField {
                                id: projectNameField
                                readOnly: !viewModel.canModify
                                Layout.fillWidth: true
                            }
                            Label {
                                text: "Structural Type"
                            }
                            ComboBox {
                                id: structuralTypeBox
                                model: ["RC Frame", "Shear Wall", "Steel Frame", "Masonry", "Mixed Structure", "Other"]
                                editable: true
                                enabled: viewModel.canModify
                                Layout.fillWidth: true
                            }

                            Button {
                                text: "Apply Building"
                                enabled: viewModel.canModify
                                Layout.columnSpan: 4
                                Layout.alignment: Qt.AlignRight
                                onClicked: viewModel.updateBuildingBasic(projectNameField.text, structuralTypeBox.editText)
                            }
                        }
                    }

                    GroupBox {
                        title: "Geo Location"
                        Layout.fillWidth: true

                        GridLayout {
                            anchors.fill: parent
                            columns: 6
                            rowSpacing: 10
                            columnSpacing: 10

                            Label {
                                text: "Longitude"
                            }
                            TextField {
                                id: longitudeField
                                readOnly: !viewModel.canModify
                                validator: DoubleValidator {
                                    bottom: -180
                                    top: 180
                                }
                                Layout.fillWidth: true
                            }
                            Label {
                                text: "Latitude"
                            }
                            TextField {
                                id: latitudeField
                                readOnly: !viewModel.canModify
                                validator: DoubleValidator {
                                    bottom: -90
                                    top: 90
                                }
                                Layout.fillWidth: true
                            }
                            Label {
                                text: "NorthAngle"
                            }
                            TextField {
                                id: northAngleField
                                readOnly: !viewModel.canModify
                                validator: DoubleValidator {
                                    bottom: 0
                                    top: 359.999
                                }
                                Layout.fillWidth: true
                            }

                            Button {
                                text: "Apply Location"
                                enabled: viewModel.canModify
                                Layout.columnSpan: 6
                                Layout.alignment: Qt.AlignRight
                                onClicked: viewModel.updateGeoLocation(parseFloat(longitudeField.text), parseFloat(latitudeField.text), parseFloat(northAngleField.text))
                            }
                        }
                    }

                    GroupBox {
                        title: "Structural Footprint"
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 4
                                rowSpacing: 10
                                columnSpacing: 10

                                Label {
                                    text: "Shape"
                                }
                                ComboBox {
                                    id: footprintShapeBox
                                    model: ["Rectangular", "Circular", "Polygon"]
                                    enabled: viewModel.canModify
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: "Bounding Box"
                                }
                                Label {
                                    text: viewModel.boundingBoxText
                                    font.family: "Consolas"
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: "Length (" + viewModel.distanceUnit + ")"
                                    visible: footprintShapeBox.currentText === "Rectangular"
                                }
                                TextField {
                                    id: footprintLengthField
                                    visible: footprintShapeBox.currentText === "Rectangular"
                                    readOnly: !viewModel.canModify
                                    validator: DoubleValidator {
                                        bottom: 0.000001
                                    }
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: "Width (" + viewModel.distanceUnit + ")"
                                    visible: footprintShapeBox.currentText === "Rectangular"
                                }
                                TextField {
                                    id: footprintWidthField
                                    visible: footprintShapeBox.currentText === "Rectangular"
                                    readOnly: !viewModel.canModify
                                    validator: DoubleValidator {
                                        bottom: 0.000001
                                    }
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: "Radius (" + viewModel.distanceUnit + ")"
                                    visible: footprintShapeBox.currentText === "Circular"
                                }
                                TextField {
                                    id: footprintRadiusField
                                    visible: footprintShapeBox.currentText === "Circular"
                                    readOnly: !viewModel.canModify
                                    validator: DoubleValidator {
                                        bottom: 0.000001
                                    }
                                    Layout.fillWidth: true
                                }
                                Item {
                                    visible: footprintShapeBox.currentText === "Circular"
                                }
                                Item {
                                    visible: footprintShapeBox.currentText === "Circular"
                                }

                                Label {
                                    text: "Corners (" + viewModel.distanceUnit + ")"
                                    visible: footprintShapeBox.currentText === "Polygon"
                                }
                                TextArea {
                                    id: polygonCornersField
                                    visible: footprintShapeBox.currentText === "Polygon"
                                    readOnly: !viewModel.canModify
                                    Layout.columnSpan: 3
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 96
                                    wrapMode: TextArea.Wrap
                                    placeholderText: "-10, -5\n10, -5\n10, 5\n-10, 5"
                                }
                                Label {
                                    text: "Use one X,Y pair per line or separate pairs with semicolons"
                                    visible: footprintShapeBox.currentText === "Polygon"
                                    Layout.columnSpan: 4
                                    color: "#666666"
                                }

                                Button {
                                    text: "Apply Footprint"
                                    enabled: viewModel.canModify
                                    Layout.columnSpan: 4
                                    Layout.alignment: Qt.AlignRight
                                    onClicked: {
                                        if (footprintShapeBox.currentText === "Polygon") {
                                            viewModel.updatePolygonCornersText(polygonCornersField.text);
                                        } else {
                                            viewModel.updateFootprint(footprintShapeBox.currentText, parseFloat(footprintLengthField.text), parseFloat(footprintWidthField.text), parseFloat(footprintRadiusField.text));
                                        }
                                    }
                                }
                            }
                        }
                    }

                    GroupBox {
                        title: "Elevation"
                        Layout.fillWidth: true

                        GridLayout {
                            anchors.fill: parent
                            columns: 4
                            rowSpacing: 10
                            columnSpacing: 10

                            Label {
                                text: "Elevation (" + viewModel.distanceUnit + ")"
                            }
                            TextArea {
                                id: elevationField
                                readOnly: !viewModel.canModify
                                Layout.columnSpan: 3
                                Layout.fillWidth: true
                                Layout.preferredHeight: 72
                                wrapMode: TextArea.Wrap
                            }
                            Label {
                                text: "ElevationNum"
                            }
                            Label {
                                text: viewModel.elevationNum
                                font.bold: true
                            }
                            Label {
                                text: viewModel.elevationSummary
                                Layout.columnSpan: 2
                            }

                            Button {
                                text: "Apply Elevation"
                                enabled: viewModel.canModify
                                Layout.columnSpan: 4
                                Layout.alignment: Qt.AlignRight
                                onClicked: viewModel.updateElevationText(elevationField.text)
                            }
                        }
                    }

                }
            }
        }

        // --- 页面 2: 通道表 (Channels) ---
        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Channels"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Label {
                        text: viewModel.canEditChannelOrder ? "" : "通道数量/顺序已锁定"
                        color: "#9a6700"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Button {
                        text: "Add"
                        enabled: viewModel.canEditChannelOrder
                        onClicked: viewModel.addChannel()
                    }
                    Button {
                        text: "Duplicate"
                        enabled: viewModel.canEditChannelOrder && viewModel.hasSelectedChannel
                        onClicked: viewModel.duplicateSelectedChannel()
                    }
                    Button {
                        text: "Delete"
                        enabled: viewModel.canEditChannelOrder && viewModel.hasSelectedChannel
                        onClicked: viewModel.deleteSelectedChannel()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Provider"
                    }
                    TextField {
                        id: providerField
                        readOnly: !viewModel.canModify
                        Layout.preferredWidth: Math.max(220, window.width * 0.28)
                    }
                    Button {
                        text: "Apply Provider"
                        enabled: viewModel.canModify
                        onClicked: viewModel.updateProvider(providerField.text)
                    }
                    Label {
                        text: "ChannelNum: " + viewModel.channelNum
                        color: "#666666"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }

                SplitView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: Qt.Horizontal

                    GroupBox {
                        title: "Channel List"
                        SplitView.preferredWidth: Math.max(520, window.width * 0.62)
                        SplitView.minimumWidth: 460
                        Layout.fillHeight: true

                        QrestTableView {
                            id: channelTable
                            anchors.fill: parent
                            model: viewModel.channelModel
                            selectionModel: viewModel.channelSelectionModel
                            alwaysShowHorizontalScrollbar: true
                            numericColumnStart: 5
                            columnWidthProvider: function (column) {
                                if (column === 1)
                                    return 180;
                                if (column === 2)
                                    return 140;
                                if (column === 3)
                                    return 130;
                                if (column === 4)
                                    return 110;
                                return 90;
                            }
                            onCellClicked: function (row, column) {
                                viewModel.selectChannel(row);
                            }
                        }
                    }

                    GroupBox {
                        title: "Selected Channel"
                        SplitView.preferredWidth: Math.max(320, window.width * 0.34)
                        SplitView.minimumWidth: 280

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                rowSpacing: 10
                                columnSpacing: 10

                                Label {
                                    text: "ChannelNo"
                                }
                                Label {
                                    id: channelNoLabel
                                    text: "-"
                                    font.bold: true
                                }

                                Label {
                                    text: "ChannelID"
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    TextField {
                                        id: channelIdField
                                        readOnly: !viewModel.canModify || !viewModel.hasSelectedChannel
                                        Layout.fillWidth: true
                                    }
                                    Button {
                                        text: "Set UNKNOWN"
                                        enabled: viewModel.canModify && viewModel.hasSelectedChannel
                                        onClicked: viewModel.setSelectedChannelUnknown()
                                    }
                                }

                                Label {
                                    text: "DeviceType"
                                }
                                ComboBox {
                                    id: channelDeviceTypeBox
                                    model: ["Accelerometer", "Velocity Sensor", "Displacement Sensor", "Strain Gauge", "Temperature Sensor", "Unknown", "Other"]
                                    editable: true
                                    enabled: viewModel.canModify && viewModel.hasSelectedChannel
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: "Measurand"
                                }
                                ComboBox {
                                    id: channelMeasurandBox
                                    model: ["Acceleration", "Velocity", "Displacement", "Strain", "Temperature", "Other"]
                                    editable: true
                                    enabled: viewModel.canModify && viewModel.hasSelectedChannel
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: "Scale"
                                }
                                TextField {
                                    id: channelScaleField
                                    readOnly: !viewModel.canModify || !viewModel.hasSelectedChannel
                                    validator: DoubleValidator {}
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: "Azimuth"
                                }
                                TextField {
                                    id: channelAzimuthField
                                    readOnly: !viewModel.canModify || !viewModel.hasSelectedChannel
                                    validator: DoubleValidator {
                                        bottom: -1
                                        top: 359.999
                                    }
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: "Direction"
                                }
                                Label {
                                    id: channelDirectionLabel
                                    text: "-"
                                    font.bold: true
                                }

                                Label {
                                    text: "X (" + viewModel.distanceUnit + ")"
                                }
                                TextField {
                                    id: channelXField
                                    readOnly: !viewModel.canModify || !viewModel.hasSelectedChannel
                                    validator: DoubleValidator {}
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: "Y (" + viewModel.distanceUnit + ")"
                                }
                                TextField {
                                    id: channelYField
                                    readOnly: !viewModel.canModify || !viewModel.hasSelectedChannel
                                    validator: DoubleValidator {}
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: "Z (" + viewModel.distanceUnit + ")"
                                }
                                TextField {
                                    id: channelZField
                                    readOnly: !viewModel.canModify || !viewModel.hasSelectedChannel
                                    validator: DoubleValidator {}
                                    Layout.fillWidth: true
                                }

                                Button {
                                    text: "Apply Channel"
                                    enabled: viewModel.canModify && viewModel.hasSelectedChannel
                                    Layout.columnSpan: 2
                                    Layout.alignment: Qt.AlignRight
                                    onClicked: viewModel.updateSelectedChannel(channelIdField.text, channelDeviceTypeBox.editText, channelMeasurandBox.editText, parseFloat(channelScaleField.text), parseFloat(channelAzimuthField.text), parseFloat(channelXField.text), parseFloat(channelYField.text), parseFloat(channelZField.text))
                                }
                            }

                            GroupBox {
                                title: "Sensor Layout"
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                SensorLayoutView {
                                    anchors.fill: parent
                                    viewModel: viewModel
                                }
                            }
                        }
                    }
                }
            }
        }

        // --- 页面 3: 数据包 (Data Packet) ---
        Item {
            // 注册全局快捷键 Ctrl+C
            Shortcut {
                sequence: "Ctrl+C"
                enabled: tabBar.currentIndex === 3 // 仅在当前页激活
                onActivated: {
                    console.log("检测到 Ctrl+C 动作");
                    viewModel.copySelectedCells();
                }
            }
            Shortcut {
                sequence: "Ctrl+A"
                enabled: tabBar.currentIndex === 3
                onActivated: viewModel.selectAllData()
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
                    let currentTs = window.timePickerTarget === "dataInfo" ? viewModel.startTimestamp : parseInt(tfTimestamp.text);
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
                    if (window.timePickerTarget === "dataInfo") {
                        viewModel.updateStartTimestamp(d.getTime());
                    } else {
                        tfTimestamp.text = d.getTime().toString();
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

                GroupBox {
                    title: "Data Information"
                    Layout.fillWidth: true

                    GridLayout {
                        anchors.fill: parent
                        columns: 4
                        rowSpacing: 10
                        columnSpacing: 10

                        Label {
                            text: "Event Name"
                        }
                        TextField {
                            id: eventNameField
                            readOnly: !viewModel.canModify
                            Layout.fillWidth: true
                        }
                        Label {
                            text: "StartTime"
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: viewModel.startTime === "" ? "Auto" : viewModel.startTime
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "Set"
                                enabled: viewModel.canModify
                                onClicked: {
                                    window.timePickerTarget = "dataInfo";
                                    timePicker.open();
                                }
                            }
                        }

                        Label {
                            text: "Sampling Rate (Hz)"
                        }
                        TextField {
                            id: samplingRateField
                            readOnly: !viewModel.canModify
                            validator: IntValidator {
                                bottom: 1
                                top: 65535
                            }
                            Layout.fillWidth: true
                        }
                        Label {
                            text: "Sampling Interval"
                        }
                        Label {
                            text: viewModel.samplingIntervalText
                            font.bold: true
                        }

                        Label {
                            text: "NPTS"
                        }
                        TextField {
                            id: nptsField
                            readOnly: !viewModel.canModify || viewModel.packetDataPointCount > 0
                            validator: IntValidator {
                                bottom: 0
                            }
                            Layout.fillWidth: true
                        }
                        Label {
                            text: "Corrected"
                        }
                        ComboBox {
                            id: correctedBox
                            model: ["NULL", "Corrected", "Unknown"]
                            enabled: viewModel.canModify
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.columnSpan: 4
                            Layout.alignment: Qt.AlignRight
                            spacing: 8

                            Button {
                                text: "Import Data"
                                enabled: viewModel.canModify
                                onClicked: importDataDialog.open()
                            }
                            Button {
                                text: "Export Data"
                                onClicked: exportDataDialog.open()
                            }
                            Button {
                                text: "Apply Data Info"
                                enabled: viewModel.canModify
                                onClicked: viewModel.updateDataInfo(eventNameField.text, parseInt(samplingRateField.text), parseInt(nptsField.text), correctedBox.currentText)
                            }
                        }
                    }
                }

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
                            readOnly: !viewModel.canModify
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
                            enabled: viewModel.canModify
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
                                readOnly: !viewModel.canModify
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "Set"
                                enabled: viewModel.canModify
                                onClicked: {
                                    window.timePickerTarget = "packetHeader";
                                    timePicker.open();
                                }
                            }
                        }

                        Label {
                            text: "采样率 (Hz):"
                        }
                        TextField {
                            id: tfSampleRate
                            readOnly: !viewModel.canModify
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
                            readOnly: !viewModel.canModify
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
                            readOnly: !viewModel.canModify
                            validator: IntValidator {
                                bottom: 1
                            }
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "同步应用到全文件"
                            enabled: viewModel.canModify
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

                    QrestTableView {
                        id: tableView
                        anchors.fill: parent
                        model: viewModel.tableModel
                        selectionModel: viewModel.selectionModel
                        cornerWidth: 80
                        cornerText: "Time(s)"
                        numericColumnStart: 0
                        alwaysShowHorizontalScrollbar: true
                        interactive: false
                        enableSelectionRectangle: true
                        columnWidthProvider: function (column) {
                            return 100;
                        }
                        onCornerClicked: viewModel.selectAllData()
                        onHorizontalHeaderClicked: function (column) {
                            viewModel.selectColumn(column);
                        }
                        onVerticalHeaderClicked: function (row) {
                            viewModel.selectRow(row);
                        }
                    }
                }
            }
        }

        // --- 页面 4: 校验报告 (Validation) ---
        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: "Validation"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Label {
                        text: viewModel.validationStatusText
                        color: viewModel.validationErrorCount > 0 ? "#c92a2a" : (viewModel.validationWarningCount > 0 ? "#9a6700" : "#2b8a3e")
                        font.bold: true
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Button {
                        text: "Run Validation"
                        onClicked: viewModel.runValidationReport()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 56
                        color: "#fff5f5"
                        border.color: "#ffc9c9"
                        radius: 6
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Label {
                                text: viewModel.validationErrorCount
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "#c92a2a"
                                font.bold: true
                                font.pixelSize: 20
                            }
                            Label {
                                text: "Errors"
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "#7d2a2a"
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 56
                        color: "#fff9db"
                        border.color: "#ffec99"
                        radius: 6
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Label {
                                text: viewModel.validationWarningCount
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "#9a6700"
                                font.bold: true
                                font.pixelSize: 20
                            }
                            Label {
                                text: "Warnings"
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "#7c5f00"
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 56
                        color: "#ebfbee"
                        border.color: "#b2f2bb"
                        radius: 6
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Label {
                                text: viewModel.validationInfoCount
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "#2b8a3e"
                                font.bold: true
                                font.pixelSize: 20
                            }
                            Label {
                                text: "Info"
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "#2f6f3e"
                            }
                        }
                    }
                }

                GroupBox {
                    title: "Issues"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    QrestTableView {
                        id: validationTable
                        anchors.fill: parent
                        model: viewModel.validationModel
                        rowHeight: 32
                        severityColors: true
                        columnWidthProvider: function (column) {
                            if (column === 0)
                                return 100;
                            if (column === 1)
                                return 140;
                            return Math.max(420, validationTable.width - 240);
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
