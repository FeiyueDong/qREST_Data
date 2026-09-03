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
        timeUnitBox.currentIndex = indexOfValue(["s", "ms"], viewModel.timeUnit);
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

    function geometryBounds() {
        const floors = viewModel.geometryFloorOutlines;
        const sensors = viewModel.sensorLayoutPoints;
        let minX = Number.POSITIVE_INFINITY;
        let maxX = Number.NEGATIVE_INFINITY;
        let minY = Number.POSITIVE_INFINITY;
        let maxY = Number.NEGATIVE_INFINITY;

        function includePoint(x, y) {
            minX = Math.min(minX, x);
            maxX = Math.max(maxX, x);
            minY = Math.min(minY, y);
            maxY = Math.max(maxY, y);
        }

        for (let floor of floors) {
            for (let point of floor.points)
                includePoint(point.x, point.y);
        }
        for (let sensor of sensors)
            includePoint(sensor.x, sensor.y);

        return {
            valid: isFinite(minX) && isFinite(minY),
            minX: minX,
            maxX: maxX,
            minY: minY,
            maxY: maxY
        };
    }

    function canvasPoint(x, y, width, height, bounds) {
        const pad = 18;
        const planWidth = Math.max(width - 70, 80);
        const spanX = Math.max(bounds.maxX - bounds.minX, 1e-6);
        const spanY = Math.max(bounds.maxY - bounds.minY, 1e-6);
        const scale = Math.min((planWidth - pad * 2) / spanX, (height - pad * 2) / spanY);
        return {
            x: pad + (x - bounds.minX) * scale,
            y: height - pad - (y - bounds.minY) * scale
        };
    }

    function selectSensorAt(mouseX, mouseY, width, height) {
        const bounds = geometryBounds();
        if (!bounds.valid)
            return;

        let bestRow = -1;
        let bestDistance = 144;
        for (let sensor of viewModel.sensorLayoutPoints) {
            const point = canvasPoint(sensor.x, sensor.y, width, height, bounds);
            const dx = point.x - mouseX;
            const dy = point.y - mouseY;
            const distance = dx * dx + dy * dy;
            if (distance < bestDistance) {
                bestDistance = distance;
                bestRow = sensor.row;
            }
        }

        if (bestRow >= 0) {
            viewModel.selectChannel(bestRow);
        }
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
        onGeometryUpdated: sensorCanvas.requestPaint()
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

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 2
                rows: 2

                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 30
                    color: "#d6d8db"
                    border.color: "#cccccc"
                    Text {
                        anchors.centerIn: parent
                        text: "#"
                        font.pixelSize: 11
                        color: "#666666"
                    }
                }

                HorizontalHeaderView {
                    syncView: binaryTable
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    delegate: Rectangle {
                        id: binaryHorizontalHeaderCell
                        implicitWidth: 180
                        implicitHeight: 30
                        required property var display
                        color: "#e9ecef"
                        border.color: "#cccccc"
                        Text {
                            anchors.centerIn: parent
                            text: binaryHorizontalHeaderCell.display
                            font.bold: true
                        }
                    }
                }

                VerticalHeaderView {
                    syncView: binaryTable
                    Layout.fillHeight: true
                    Layout.preferredWidth: 48
                    delegate: Rectangle {
                        id: binaryVerticalHeaderCell
                        implicitWidth: 48
                        implicitHeight: 30
                        required property var display
                        color: "#f1f3f5"
                        border.color: "#dee2e6"
                        Text {
                            anchors.centerIn: parent
                            text: binaryVerticalHeaderCell.display
                            font.pixelSize: 12
                            color: "#495057"
                        }
                    }
                }

                TableView {
                    id: binaryTable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    columnSpacing: 1
                    rowSpacing: 1
                    model: viewModel.binaryModel
                    columnWidthProvider: function (column) {
                        if (column === 0)
                            return 100;
                        if (column === 1)
                            return 430;
                        return 220;
                    }

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AlwaysOn
                    }
                    ScrollBar.horizontal: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        id: binaryCell
                        implicitWidth: 180
                        implicitHeight: 28
                        required property int row
                        required property var display
                        color: row % 2 === 0 ? "#ffffff" : "#f8f9fa"
                        border.color: "#eeeeee"

                        Text {
                            anchors.fill: parent
                            anchors.margins: 6
                            verticalAlignment: Text.AlignVCenter
                            text: binaryCell.display
                            elide: Text.ElideRight
                            font.family: "Consolas"
                            color: "#1f2933"
                        }
                    }
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

                Button {
                    text: "New"
                    onClicked: window.requestGuardedAction("new")
                }
                Button {
                    text: "Open"
                    onClicked: window.requestGuardedAction("open")
                }
                Button {
                    text: "Edit"
                    enabled: viewModel.canEdit
                    onClicked: viewModel.beginEdit()
                }
                Button {
                    text: "Validate"
                    onClicked: {
                        viewModel.validateDocument();
                        tabBar.currentIndex = 4;
                    }
                }
                Button {
                    text: "Save As"
                    enabled: viewModel.canSaveAs
                    highlighted: viewModel.isDirty
                    onClicked: saveDialog.open()
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
                                model: ["s", "ms"]
                                enabled: viewModel.canModify
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

                    GroupBox {
                        title: "Instrument"
                        Layout.fillWidth: true

                        GridLayout {
                            anchors.fill: parent
                            columns: 4
                            rowSpacing: 10
                            columnSpacing: 10

                            Label {
                                text: "Provider"
                            }
                            TextField {
                                id: providerField
                                readOnly: !viewModel.canModify
                                Layout.fillWidth: true
                            }
                            Label {
                                text: "ChannelNum"
                            }
                            Label {
                                text: viewModel.channelNum
                                font.bold: true
                            }

                            Button {
                                text: "Apply Instrument"
                                enabled: viewModel.canModify
                                Layout.columnSpan: 4
                                Layout.alignment: Qt.AlignRight
                                onClicked: viewModel.updateProvider(providerField.text)
                            }
                        }
                    }

                    GroupBox {
                        title: "Data Info"
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

                            Button {
                                text: "Apply Data Info"
                                enabled: viewModel.canModify
                                Layout.columnSpan: 4
                                Layout.alignment: Qt.AlignRight
                                onClicked: viewModel.updateDataInfo(eventNameField.text, parseInt(samplingRateField.text), parseInt(nptsField.text), correctedBox.currentText)
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
                        text: "Provider: " + (viewModel.provider === "" ? "-" : viewModel.provider)
                        color: "#666666"
                    }
                    Label {
                        text: "ChannelNum: " + viewModel.channelNum
                        color: "#666666"
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

                SplitView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: Qt.Horizontal

                    GroupBox {
                        title: "Channel List"
                        SplitView.preferredWidth: 670
                        SplitView.minimumWidth: 460
                        Layout.fillHeight: true

                        GridLayout {
                            anchors.fill: parent
                            columns: 2
                            rows: 2

                            Rectangle {
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 30
                                color: "#d6d8db"
                                border.color: "#cccccc"
                                Text {
                                    anchors.centerIn: parent
                                    text: "#"
                                    font.pixelSize: 11
                                    color: "#666666"
                                }
                            }

                            HorizontalHeaderView {
                                id: channelHeader
                                syncView: channelTable
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                delegate: Rectangle {
                                    id: channelHorizontalHeaderCell
                                    implicitWidth: 110
                                    implicitHeight: 30
                                    required property var display
                                    color: "#e9ecef"
                                    border.color: "#cccccc"
                                    Text {
                                        anchors.centerIn: parent
                                        text: channelHorizontalHeaderCell.display
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            VerticalHeaderView {
                                syncView: channelTable
                                Layout.fillHeight: true
                                Layout.preferredWidth: 48
                                delegate: Rectangle {
                                    id: channelVerticalHeaderCell
                                    implicitWidth: 48
                                    implicitHeight: 30
                                    required property var display
                                    color: "#f1f3f5"
                                    border.color: "#dee2e6"
                                    Text {
                                        anchors.centerIn: parent
                                        text: channelVerticalHeaderCell.display
                                        font.pixelSize: 12
                                        color: "#495057"
                                    }
                                }
                            }

                            TableView {
                                id: channelTable
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                columnSpacing: 1
                                rowSpacing: 1
                                model: viewModel.channelModel
                                selectionModel: viewModel.channelSelectionModel
                                columnWidthProvider: function (column) {
                                    if (column === 1)
                                        return 180;
                                    if (column === 2)
                                        return 130;
                                    if (column === 3)
                                        return 110;
                                    return 90;
                                }

                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AlwaysOn
                                }
                                ScrollBar.horizontal: ScrollBar {
                                    policy: ScrollBar.AlwaysOn
                                }

                                delegate: Rectangle {
                                    id: channelCell
                                    implicitWidth: 100
                                    implicitHeight: 30
                                    required property bool selected
                                    required property int row
                                    required property int column
                                    required property var display
                                    color: channelCell.selected ? "#0078d7" : ((channelCell.row % 2 === 0) ? "#ffffff" : "#f8f9fa")
                                    border.color: "#eeeeee"

                                    Text {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        verticalAlignment: Text.AlignVCenter
                                        text: channelCell.display
                                        elide: Text.ElideRight
                                        font.family: channelCell.column >= 4 ? "Consolas" : "Sans Serif"
                                        color: channelCell.selected ? "white" : "black"
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: viewModel.selectChannel(channelCell.row)
                                    }
                                }
                            }
                        }
                    }

                    GroupBox {
                        title: "Selected Channel"
                        SplitView.preferredWidth: 330
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
                                TextField {
                                    id: channelIdField
                                    readOnly: !viewModel.canModify || !viewModel.hasSelectedChannel
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
                                    onClicked: viewModel.updateSelectedChannel(channelIdField.text, channelMeasurandBox.editText, parseFloat(channelScaleField.text), parseFloat(channelAzimuthField.text), parseFloat(channelXField.text), parseFloat(channelYField.text), parseFloat(channelZField.text))
                                }
                            }

                            GroupBox {
                                title: "Sensor Layout"
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 6

                                    Label {
                                        text: viewModel.geometrySummary
                                        color: "#666666"
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.minimumHeight: 180
                                        color: "#ffffff"
                                        border.color: "#cccccc"

                                        Canvas {
                                            id: sensorCanvas
                                            anchors.fill: parent
                                            anchors.margins: 8
                                            onPaint: {
                                                const ctx = getContext("2d");
                                                ctx.clearRect(0, 0, width, height);

                                                const floors = viewModel.geometryFloorOutlines;
                                                const sensors = viewModel.sensorLayoutPoints;
                                                let minX = Number.POSITIVE_INFINITY;
                                                let maxX = Number.NEGATIVE_INFINITY;
                                                let minY = Number.POSITIVE_INFINITY;
                                                let maxY = Number.NEGATIVE_INFINITY;
                                                let minZ = Number.POSITIVE_INFINITY;
                                                let maxZ = Number.NEGATIVE_INFINITY;

                                                function includePoint(x, y) {
                                                    minX = Math.min(minX, x);
                                                    maxX = Math.max(maxX, x);
                                                    minY = Math.min(minY, y);
                                                    maxY = Math.max(maxY, y);
                                                }

                                                for (let floor of floors) {
                                                    minZ = Math.min(minZ, floor.z);
                                                    maxZ = Math.max(maxZ, floor.z);
                                                    for (let point of floor.points)
                                                        includePoint(point.x, point.y);
                                                }
                                                for (let sensor of sensors)
                                                    includePoint(sensor.x, sensor.y);

                                                if (!isFinite(minX) || !isFinite(minY)) {
                                                    ctx.fillStyle = "#777777";
                                                    ctx.fillText("No geometry", 12, 24);
                                                    return;
                                                }

                                                const pad = 18;
                                                const planWidth = Math.max(width - 70, 80);
                                                const spanX = Math.max(maxX - minX, 1e-6);
                                                const spanY = Math.max(maxY - minY, 1e-6);
                                                const scale = Math.min((planWidth - pad * 2) / spanX, (height - pad * 2) / spanY);

                                                function sx(x) {
                                                    return pad + (x - minX) * scale;
                                                }
                                                function sy(y) {
                                                    return height - pad - (y - minY) * scale;
                                                }

                                                ctx.lineWidth = 1;
                                                ctx.strokeStyle = "#8a8f98";
                                                for (let floor of floors) {
                                                    if (floor.points.length < 2)
                                                        continue;
                                                    ctx.beginPath();
                                                    ctx.moveTo(sx(floor.points[0].x), sy(floor.points[0].y));
                                                    for (let i = 1; i < floor.points.length; ++i)
                                                        ctx.lineTo(sx(floor.points[i].x), sy(floor.points[i].y));
                                                    ctx.stroke();
                                                }

                                                if (isFinite(minZ) && isFinite(maxZ) && floors.length > 0) {
                                                    const axisX = Math.max(planWidth + 12, width - 54);
                                                    const labelX = Math.min(axisX + 6, width - 34);
                                                    const spanZ = Math.max(maxZ - minZ, 1e-6);

                                                    function sz(z) {
                                                        return height - pad - (z - minZ) / spanZ * (height - pad * 2);
                                                    }

                                                    ctx.strokeStyle = "#adb5bd";
                                                    ctx.lineWidth = 1;
                                                    ctx.beginPath();
                                                    ctx.moveTo(axisX, pad);
                                                    ctx.lineTo(axisX, height - pad);
                                                    ctx.stroke();

                                                    ctx.fillStyle = "#5f6b7a";
                                                    ctx.font = "10px sans-serif";
                                                    for (let floor of floors) {
                                                        const y = sz(floor.z);
                                                        ctx.strokeStyle = "#6c757d";
                                                        ctx.beginPath();
                                                        ctx.moveTo(axisX - 4, y);
                                                        ctx.lineTo(width - 8, y);
                                                        ctx.stroke();
                                                        ctx.fillText(Number(floor.z).toFixed(2), labelX, y - 3);
                                                    }
                                                }

                                                for (let sensor of sensors) {
                                                    const x = sx(sensor.x);
                                                    const y = sy(sensor.y);
                                                    ctx.fillStyle = sensor.selected ? "#d92332" : "#0b7285";
                                                    ctx.strokeStyle = sensor.selected ? "#d92332" : "#0b7285";
                                                    ctx.beginPath();
                                                    ctx.arc(x, y, sensor.selected ? 5 : 4, 0, Math.PI * 2);
                                                    ctx.fill();

                                                    if (Math.abs(sensor.ux) > 1e-9 || Math.abs(sensor.uy) > 1e-9) {
                                                        ctx.beginPath();
                                                        ctx.moveTo(x, y);
                                                        ctx.lineTo(x + sensor.ux * 16, y - sensor.uy * 16);
                                                        ctx.stroke();
                                                    }
                                                }
                                            }
                                        }
                                        MouseArea {
                                            anchors.fill: sensorCanvas
                                            onClicked: function (mouse) {
                                                window.selectSensorAt(mouse.x, mouse.y, sensorCanvas.width, sensorCanvas.height);
                                            }
                                        }
                                    }
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
                                id: dataHorizontalHeaderCell
                                implicitWidth: 100
                                implicitHeight: 30
                                required property int column
                                required property var display
                                color: "#e9ecef"
                                border.color: "#ccc"
                                Text {
                                    anchors.centerIn: parent
                                    text: dataHorizontalHeaderCell.display
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: viewModel.selectColumn(dataHorizontalHeaderCell.column)
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
                                id: dataVerticalHeaderCell
                                implicitWidth: 80
                                implicitHeight: 30
                                required property int row
                                required property var display
                                color: "#f1f3f5"
                                border.color: "#dee2e6"
                                Text {
                                    anchors.centerIn: parent
                                    // 这里的 display 已经是由 C++ headerData 计算出的时间字符串
                                    text: dataVerticalHeaderCell.display
                                    font.family: "Consolas"
                                    font.pixelSize: 12
                                    color: "#495057"
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: viewModel.selectRow(dataVerticalHeaderCell.row)
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
                                id: dataCell
                                implicitWidth: 100
                                implicitHeight: 30
                                required property bool selected
                                required property int row
                                required property var display
                                color: dataCell.selected ? "#0078d7" : ((dataCell.row % 2 == 0) ? "#ffffff" : "#f8f9fa")
                                border.color: "#eee"

                                Text {
                                    anchors.centerIn: parent
                                    text: dataCell.display
                                    font.family: "Consolas"
                                    color: dataCell.selected ? "white" : "black"
                                }
                            }
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
                }

                GroupBox {
                    title: "Issues"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        rows: 2

                        Rectangle {
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 30
                            color: "#d6d8db"
                            border.color: "#cccccc"
                            Text {
                                anchors.centerIn: parent
                                text: "#"
                                font.pixelSize: 11
                                color: "#666666"
                            }
                        }

                        HorizontalHeaderView {
                            syncView: validationTable
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            delegate: Rectangle {
                                id: validationHorizontalHeaderCell
                                implicitWidth: 160
                                implicitHeight: 30
                                required property var display
                                color: "#e9ecef"
                                border.color: "#cccccc"
                                Text {
                                    anchors.centerIn: parent
                                    text: validationHorizontalHeaderCell.display
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        VerticalHeaderView {
                            syncView: validationTable
                            Layout.fillHeight: true
                            Layout.preferredWidth: 48
                            delegate: Rectangle {
                                id: validationVerticalHeaderCell
                                implicitWidth: 48
                                implicitHeight: 30
                                required property var display
                                color: "#f1f3f5"
                                border.color: "#dee2e6"
                                Text {
                                    anchors.centerIn: parent
                                    text: validationVerticalHeaderCell.display
                                    font.pixelSize: 12
                                    color: "#495057"
                                }
                            }
                        }

                        TableView {
                            id: validationTable
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            columnSpacing: 1
                            rowSpacing: 1
                            model: viewModel.validationModel
                            columnWidthProvider: function (column) {
                                if (column === 0)
                                    return 100;
                                if (column === 1)
                                    return 140;
                                return Math.max(420, validationTable.width - 240);
                            }

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AlwaysOn
                            }
                            ScrollBar.horizontal: ScrollBar {
                                policy: ScrollBar.AsNeeded
                            }

                            delegate: Rectangle {
                                id: validationCell
                                implicitWidth: 160
                                implicitHeight: 32
                                required property int row
                                required property int column
                                required property var display
                                color: row % 2 === 0 ? "#ffffff" : "#f8f9fa"
                                border.color: "#eeeeee"

                                Text {
                                    anchors.fill: parent
                                    anchors.margins: 7
                                    verticalAlignment: Text.AlignVCenter
                                    text: validationCell.display
                                    elide: Text.ElideRight
                                    color: validationCell.column === 0 && validationCell.display === "Error" ? "#c92a2a" : (validationCell.column === 0 && validationCell.display === "Warning" ? "#9a6700" : "#1f2933")
                                    font.bold: validationCell.column === 0
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
