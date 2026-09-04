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
        channelsPage.refreshFields();
        dataPage.refreshDataFields();
        rawMetadataDialog.refreshJson();
    }

    function refreshChannelFields() { channelsPage.refreshFields(); }

    function refreshPacketFields() {
        dataPage.refreshPacketFields();
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
            channelsPage.positionSelectedChannel(viewModel.selectedChannelRow);
        }
        onConfirmDataImportNptsMismatch: function (fileUrl, expectedNpts, importedNpts, importedChannels) {
            dataImportMismatchDialog.prompt(fileUrl, expectedNpts, importedNpts, importedChannels);
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

    DataImportMismatchDialog {
        id: dataImportMismatchDialog
        viewModel: viewModel
    }

    RawMetadataDialog {
        id: rawMetadataDialog
        viewModel: viewModel
    }

    BinaryViewerDialog {
        id: binaryViewerDialog
        viewModel: viewModel
    }

    PacketInspectorDialog {
        id: inspectorDialog
        viewModel: viewModel
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
                onTriggered: rawMetadataDialog.openWithCurrentJson()
            }
            MenuItem {
                text: qsTr("Binary Viewer...")
                onTriggered: binaryViewerDialog.openWithReset()
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
                    onClicked: rawMetadataDialog.openWithCurrentJson()
                }
                ToolButton {
                    text: "Binary"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_ui/icon/Binary.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Open read-only binary viewer"
                    onClicked: binaryViewerDialog.openWithReset()
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

        OverviewPage {
            viewModel: viewModel
            onNavigateRequested: function (index) {
                tabBar.currentIndex = index;
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

        ChannelsPage {
            id: channelsPage
            viewModel: viewModel
        }

        DataPage {
            id: dataPage
            viewModel: viewModel
            active: tabBar.currentIndex === 3
            onImportDataRequested: importDataDialog.open()
            onExportDataRequested: exportDataDialog.open()
        }

        ValidationPage {
            viewModel: viewModel
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
