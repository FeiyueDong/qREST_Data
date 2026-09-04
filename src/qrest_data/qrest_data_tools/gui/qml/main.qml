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
    property string helpResourceUrl: "qrc:/qt/qml/qrest_data_tools_gui/doc/helper.md"
    property string formatSpecResourceUrl: "qrc:/qt/qml/qrest_data_tools_gui/doc/file_format.md"
    property string projectHomepageUrl: "https://www.qu-zhe.net/qrest.htm"

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

    function refreshMetadataFields() {
        buildingPage.refreshFields();
        channelsPage.refreshFields();
        dataPage.refreshDataFields();
        rawMetadataDialog.refreshJson();
    }

    function refreshChannelFields() { channelsPage.refreshFields(); }

    function refreshPacketFields() {
        inspectorDialog.refreshPacketFields();
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

    FieldHelpRegistry {
        id: fieldHelp
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

    FileDialog {
        id: exportHdf5Dialog
        title: "导出 HDF5"
        fileMode: FileDialog.SaveFile
        nameFilters: ["HDF5 Files (*.h5 *.hdf5)", "All Files (*.*)"]
        defaultSuffix: "h5"
        onAccepted: viewModel.exportHdf5Data(selectedFile)
    }

    FileDialog {
        id: importTdmsDialog
        title: "导入 TDMS 文件"
        nameFilters: ["TDMS Files (*.tdms)", "All Files (*.*)"]
        onAccepted: externalImportDialog.openForImport("tdms", selectedFile)
    }

    FolderDialog {
        id: importTdmsFolderDialog
        title: "导入 TDMS 目录"
        onAccepted: externalImportDialog.openForImport("tdms", selectedFolder)
    }

    FileDialog {
        id: importMseedDialog
        title: "导入 Modified MiniSEED 文件"
        nameFilters: ["MiniSEED Files (*.mseed *.miniseed)", "All Files (*.*)"]
        onAccepted: externalImportDialog.openForImport("mseed", selectedFile)
    }

    FolderDialog {
        id: importMseedFolderDialog
        title: "导入 Modified MiniSEED 目录"
        onAccepted: externalImportDialog.openForImport("mseed", selectedFolder)
    }

    FileDialog {
        id: importHdf5Dialog
        title: "导入 HDF5"
        nameFilters: ["HDF5 Files (*.h5 *.hdf5)", "All Files (*.*)"]
        onAccepted: externalImportDialog.openForImport("hdf5", selectedFile)
    }

    DataImportMismatchDialog {
        id: dataImportMismatchDialog
        viewModel: viewModel
    }

    ExternalImportDialog {
        id: externalImportDialog
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
        helpRegistry: fieldHelp
    }

    DocumentViewerDialog {
        id: documentViewerDialog
    }

    Dialog {
        id: aboutDialog
        title: "About qREST Data Tools"
        modal: true
        width: 440
        x: (window.width - width) / 2
        y: (window.height - height) / 2

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Label {
                text: "qREST Data Tools"
                font.pixelSize: 22
                font.bold: true
                Layout.fillWidth: true
            }
            Label {
                text: "Application Version: " + Qt.application.version
                Layout.fillWidth: true
            }
            Label {
                text: "Supported qREST Format Version: " + viewModel.metadataVersionText
                Layout.fillWidth: true
            }
            Label {
                text: "Project Homepage: " + (window.projectHomepageUrl === "" ? "Not configured" : window.projectHomepageUrl)
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                text: "License: See project repository"
                Layout.fillWidth: true
            }
            Label {
                text: "Copyright: qREST"
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight

                Button {
                    text: "Close"
                    onClicked: aboutDialog.close()
                }
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
            Menu {
                title: qsTr("Import Data Body")
                MenuItem {
                    text: qsTr("Text / CSV...")
                    enabled: viewModel.canModify
                    onTriggered: importDataDialog.open()
                }
            }
            Menu {
                title: qsTr("Import External Data")
                MenuItem {
                    text: qsTr("TDMS File...")
                    enabled: viewModel.canModify
                    onTriggered: importTdmsDialog.open()
                }
                MenuItem {
                    text: qsTr("TDMS Directory...")
                    enabled: viewModel.canModify
                    onTriggered: importTdmsFolderDialog.open()
                }
                MenuItem {
                    text: qsTr("Modified MiniSEED File...")
                    enabled: viewModel.canModify
                    onTriggered: importMseedDialog.open()
                }
                MenuItem {
                    text: qsTr("Modified MiniSEED Directory...")
                    enabled: viewModel.canModify
                    onTriggered: importMseedFolderDialog.open()
                }
                MenuItem {
                    text: qsTr("HDF5 File...")
                    enabled: viewModel.canModify
                    onTriggered: importHdf5Dialog.open()
                }
            }
            Menu {
                title: qsTr("Export")
                MenuItem {
                    text: qsTr("Text...")
                    onTriggered: exportDataDialog.open()
                }
                MenuItem {
                    text: qsTr("HDF5...")
                    onTriggered: exportHdf5Dialog.open()
                }
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("Import Metadata JSON...")
                enabled: viewModel.canModify
                onTriggered: importMetaDialog.open()
            }
            MenuItem {
                text: qsTr("Export Metadata JSON...")
                onTriggered: exportMetaDialog.open()
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
                onTriggered: inspectorDialog.openWithCurrentPacket()
            }
        }
        Menu {
            title: qsTr("帮助 (Help)")
            MenuItem {
                text: qsTr("User Guide")
                onTriggered: documentViewerDialog.openDocument("User Guide", window.helpResourceUrl)
            }
            MenuItem {
                text: qsTr("qREST File Format Specification")
                onTriggered: documentViewerDialog.openDocument("qREST File Format Specification", window.formatSpecResourceUrl)
            }
            MenuItem {
                text: qsTr("Project Homepage")
                enabled: window.projectHomepageUrl !== ""
                onTriggered: Qt.openUrlExternally(window.projectHomepageUrl)
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("About qREST Data Tools")
                onTriggered: aboutDialog.open()
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
                    icon.source: "qrc:/qt/qml/qrest_data_tools_gui/icon/New.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Create new qREST file"
                    onClicked: window.requestGuardedAction("new")
                }
                ToolButton {
                    text: "Open"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_gui/icon/Open.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Open qREST file"
                    onClicked: window.requestGuardedAction("open")
                }
                ToolSeparator {}
                ToolButton {
                    text: "Edit"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_gui/icon/Edit.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Create editable copy"
                    enabled: viewModel.canEdit
                    onClicked: viewModel.beginEdit()
                }
                ToolButton {
                    text: "Validate"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_gui/icon/Validate.png"
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
                    icon.source: "qrc:/qt/qml/qrest_data_tools_gui/icon/SaveAs.png"
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
                    icon.source: "qrc:/qt/qml/qrest_data_tools_gui/icon/JSON.png"
                    display: AbstractButton.TextBesideIcon
                    ToolTip.visible: hovered
                    ToolTip.text: "Open raw metadata JSON"
                    onClicked: rawMetadataDialog.openWithCurrentJson()
                }
                ToolButton {
                    text: "Binary"
                    icon.source: "qrc:/qt/qml/qrest_data_tools_gui/icon/Binary.png"
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
        }

        BuildingPage {
            id: buildingPage
            viewModel: viewModel
            helpRegistry: fieldHelp
        }

        ChannelsPage {
            id: channelsPage
            viewModel: viewModel
            helpRegistry: fieldHelp
        }

        DataPage {
            id: dataPage
            viewModel: viewModel
            helpRegistry: fieldHelp
            active: tabBar.currentIndex === 3
            onImportDataRequested: importDataDialog.open()
            onExportDataRequested: exportDataDialog.open()
            onAdvancedPacketRequested: inspectorDialog.openWithCurrentPacket()
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
