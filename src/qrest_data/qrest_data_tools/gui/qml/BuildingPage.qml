import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var viewModel
    property int maxContentWidth: 1020

    function indexOfValue(values, value) {
        const index = values.indexOf(value);
        return index >= 0 ? index : 0;
    }

    function refreshFields() {
        distanceUnitBox.currentIndex = indexOfValue(["m", "mm", "cm"], root.viewModel.distanceUnit);
        timeUnitBox.currentIndex = 0;
        projectNameField.text = root.viewModel.projectName;
        structuralTypeBox.currentIndex = indexOfValue(["RC Frame", "Shear Wall", "Steel Frame", "Masonry", "Mixed Structure", "Other"], root.viewModel.structuralType);
        structuralTypeBox.editText = root.viewModel.structuralType;
        longitudeField.text = root.viewModel.longitude;
        latitudeField.text = root.viewModel.latitude;
        northAngleField.text = root.viewModel.northAngle;
        footprintShapeBox.currentIndex = indexOfValue(["Rectangular", "Circular", "Polygon"], root.viewModel.footprintShape);
        footprintLengthField.text = root.viewModel.footprintLength;
        footprintWidthField.text = root.viewModel.footprintWidth;
        footprintRadiusField.text = root.viewModel.footprintRadius;
        polygonCornersField.text = root.viewModel.polygonCornersText;
        elevationField.text = root.viewModel.elevationText;
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        clip: true

        ColumnLayout {
            width: Math.min(Math.max(root.width - 40, 280), root.maxContentWidth)
            x: Math.max(0, (root.width - width) / 2)
            spacing: 14

            GridLayout {
                Layout.fillWidth: true
                columns: root.width >= 900 ? 2 : 1
                rowSpacing: 14
                columnSpacing: 14

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
                            text: root.viewModel.metadataHeader
                            font.bold: true
                        }
                        Label {
                            text: "Version"
                        }
                        Label {
                            text: root.viewModel.metadataVersionText
                            font.bold: true
                        }

                        Label {
                            text: "Distance Unit"
                        }
                        ComboBox {
                            id: distanceUnitBox
                            model: ["m", "mm", "cm"]
                            enabled: root.viewModel.canModify
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
                            enabled: root.viewModel.canModify
                            Layout.columnSpan: 4
                            Layout.alignment: Qt.AlignRight
                            onClicked: root.viewModel.updateUnits(distanceUnitBox.currentText, timeUnitBox.currentText)
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
                            readOnly: !root.viewModel.canModify
                            Layout.fillWidth: true
                        }
                        Label {
                            text: "Structural Type"
                        }
                        ComboBox {
                            id: structuralTypeBox
                            model: ["RC Frame", "Shear Wall", "Steel Frame", "Masonry", "Mixed Structure", "Other"]
                            editable: true
                            enabled: root.viewModel.canModify
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Apply Building"
                            enabled: root.viewModel.canModify
                            Layout.columnSpan: 4
                            Layout.alignment: Qt.AlignRight
                            onClicked: root.viewModel.updateBuildingBasic(projectNameField.text, structuralTypeBox.editText)
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
                            readOnly: !root.viewModel.canModify
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
                            readOnly: !root.viewModel.canModify
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
                            readOnly: !root.viewModel.canModify
                            validator: DoubleValidator {
                                bottom: 0
                                top: 359.999
                            }
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Apply Location"
                            enabled: root.viewModel.canModify
                            Layout.columnSpan: 6
                            Layout.alignment: Qt.AlignRight
                            onClicked: root.viewModel.updateGeoLocation(parseFloat(longitudeField.text), parseFloat(latitudeField.text), parseFloat(northAngleField.text))
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
                            text: "Elevation (" + root.viewModel.distanceUnit + ")"
                        }
                        ScrollView {
                            Layout.columnSpan: 3
                            Layout.fillWidth: true
                            Layout.preferredHeight: 88
                            clip: true

                            TextArea {
                                id: elevationField
                                readOnly: !root.viewModel.canModify
                                wrapMode: TextArea.Wrap
                            }
                        }
                        Label {
                            text: "ElevationNum"
                        }
                        Label {
                            text: root.viewModel.elevationNum
                            font.bold: true
                        }
                        Label {
                            text: root.viewModel.elevationSummary
                            Layout.columnSpan: 2
                        }

                        Button {
                            text: "Apply Elevation"
                            enabled: root.viewModel.canModify
                            Layout.columnSpan: 4
                            Layout.alignment: Qt.AlignRight
                            onClicked: root.viewModel.updateElevationText(elevationField.text)
                        }
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
                            enabled: root.viewModel.canModify
                            Layout.fillWidth: true
                        }
                        Label {
                            text: "Bounding Box"
                        }
                        Label {
                            text: root.viewModel.boundingBoxText
                            font.family: "Consolas"
                            Layout.fillWidth: true
                        }

                        Label {
                            text: "Length (" + root.viewModel.distanceUnit + ")"
                            visible: footprintShapeBox.currentText === "Rectangular"
                        }
                        TextField {
                            id: footprintLengthField
                            visible: footprintShapeBox.currentText === "Rectangular"
                            readOnly: !root.viewModel.canModify
                            validator: DoubleValidator {
                                bottom: 0.000001
                            }
                            Layout.fillWidth: true
                        }
                        Label {
                            text: "Width (" + root.viewModel.distanceUnit + ")"
                            visible: footprintShapeBox.currentText === "Rectangular"
                        }
                        TextField {
                            id: footprintWidthField
                            visible: footprintShapeBox.currentText === "Rectangular"
                            readOnly: !root.viewModel.canModify
                            validator: DoubleValidator {
                                bottom: 0.000001
                            }
                            Layout.fillWidth: true
                        }

                        Label {
                            text: "Radius (" + root.viewModel.distanceUnit + ")"
                            visible: footprintShapeBox.currentText === "Circular"
                        }
                        TextField {
                            id: footprintRadiusField
                            visible: footprintShapeBox.currentText === "Circular"
                            readOnly: !root.viewModel.canModify
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
                            text: "Corners (" + root.viewModel.distanceUnit + ")"
                            visible: footprintShapeBox.currentText === "Polygon"
                        }
                        TextArea {
                            id: polygonCornersField
                            visible: footprintShapeBox.currentText === "Polygon"
                            readOnly: !root.viewModel.canModify
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
                            enabled: root.viewModel.canModify
                            Layout.columnSpan: 4
                            Layout.alignment: Qt.AlignRight
                            onClicked: {
                                if (footprintShapeBox.currentText === "Polygon") {
                                    root.viewModel.updatePolygonCornersText(polygonCornersField.text);
                                } else {
                                    root.viewModel.updateFootprint(footprintShapeBox.currentText, parseFloat(footprintLengthField.text), parseFloat(footprintWidthField.text), parseFloat(footprintRadiusField.text));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: refreshFields()
}
