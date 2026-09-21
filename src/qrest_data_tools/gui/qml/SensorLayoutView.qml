import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    required property var viewModel
    property string viewMode: "Isometric"
    property var floorOptions: []
    property int selectedFloorIndex: 0
    property var hoveredSensor: null

    spacing: 6

    Component.onCompleted: refreshFloorOptions()

    function refreshFloorOptions() {
        const options = [{ label: "All Floors", z: null }];
        for (let floor of root.viewModel.geometryFloorOutlines) {
            options.push({
                label: "Z = " + Number(floor.z).toFixed(3).replace(/\.?0+$/, ""),
                z: floor.z
            });
        }
        floorOptions = options;
        if (selectedFloorIndex >= floorOptions.length)
            selectedFloorIndex = 0;
    }

    function selectedFloorZ() {
        if (selectedFloorIndex <= 0 || selectedFloorIndex >= floorOptions.length)
            return null;
        return floorOptions[selectedFloorIndex].z;
    }

    function isOnSelectedFloor(z) {
        const selectedZ = selectedFloorZ();
        return selectedZ === null || Math.abs(z - selectedZ) < 1e-6;
    }

    function visibleFloors() {
        const floors = [];
        for (let floor of root.viewModel.geometryFloorOutlines) {
            if (isOnSelectedFloor(floor.z))
                floors.push(floor);
        }
        return floors;
    }

    function visibleSensors() {
        const sensors = [];
        for (let sensor of root.viewModel.sensorLayoutPoints) {
            if (isOnSelectedFloor(sensor.z))
                sensors.push(sensor);
        }
        return sensors;
    }

    function projectPoint(x, y, z) {
        if (viewMode === "Plan")
            return { x: x, y: y };
        if (viewMode === "X-Z")
            return { x: x, y: z };
        if (viewMode === "Y-Z")
            return { x: y, y: z };
        return { x: x - y, y: (x + y) * 0.45 + z };
    }

    function projectVector(x, y, z) {
        if (viewMode === "Plan")
            return { x: x, y: y };
        if (viewMode === "X-Z")
            return { x: x, y: z };
        if (viewMode === "Y-Z")
            return { x: y, y: z };
        return { x: x - y, y: (x + y) * 0.45 + z };
    }

    function sensorPoint(sensor) {
        return projectPoint(sensor.x, sensor.y, sensor.z);
    }

    function sensorDirection(sensor) {
        return projectVector(sensor.ux, sensor.uy, sensor.uz);
    }

    function includePoint(bounds, point) {
        if (!isFinite(point.x) || !isFinite(point.y))
            return;
        if (!bounds.valid) {
            bounds.valid = true;
            bounds.minX = bounds.maxX = point.x;
            bounds.minY = bounds.maxY = point.y;
            return;
        }
        bounds.minX = Math.min(bounds.minX, point.x);
        bounds.maxX = Math.max(bounds.maxX, point.x);
        bounds.minY = Math.min(bounds.minY, point.y);
        bounds.maxY = Math.max(bounds.maxY, point.y);
    }

    function projectedBounds() {
        const bounds = {
            valid: false,
            minX: 0,
            maxX: 0,
            minY: 0,
            maxY: 0
        };
        const floors = visibleFloors();
        for (let floor of floors) {
            for (let point of floor.points) {
                includePoint(bounds, projectPoint(point.x, point.y, floor.z));
            }
        }
        for (let sensor of visibleSensors()) {
            const point = sensorPoint(sensor);
            includePoint(bounds, point);
            const direction = sensorDirection(sensor);
            includePoint(bounds, {
                x: point.x + direction.x,
                y: point.y + direction.y
            });
        }
        if (bounds.valid) {
            const padX = Math.max((bounds.maxX - bounds.minX) * 0.08, 0.5);
            const padY = Math.max((bounds.maxY - bounds.minY) * 0.08, 0.5);
            bounds.minX -= padX;
            bounds.maxX += padX;
            bounds.minY -= padY;
            bounds.maxY += padY;
        }
        return bounds;
    }

    function viewTransform(width, height, bounds) {
        const pad = 24;
        const availableWidth = Math.max(width - pad * 2, 1);
        const availableHeight = Math.max(height - pad * 2, 1);
        const spanX = Math.max(bounds.maxX - bounds.minX, 1e-6);
        const spanY = Math.max(bounds.maxY - bounds.minY, 1e-6);
        const scale = Math.min(availableWidth / spanX, availableHeight / spanY);
        const drawWidth = spanX * scale;
        const drawHeight = spanY * scale;
        return {
            minX: bounds.minX,
            maxY: bounds.maxY,
            scale: scale,
            offsetX: (width - drawWidth) / 2,
            offsetY: (height - drawHeight) / 2
        };
    }

    function screenPoint(point, transform) {
        return {
            x: transform.offsetX + (point.x - transform.minX) * transform.scale,
            y: transform.offsetY + (transform.maxY - point.y) * transform.scale
        };
    }

    function sensorAt(mouseX, mouseY, width, height) {
        const bounds = projectedBounds();
        if (!bounds.valid)
            return null;

        const transform = viewTransform(width, height, bounds);
        let bestSensor = null;
        let bestDistance = 144;
        for (let sensor of visibleSensors()) {
            const point = screenPoint(sensorPoint(sensor), transform);
            const dx = point.x - mouseX;
            const dy = point.y - mouseY;
            const distance = dx * dx + dy * dy;
            if (distance < bestDistance) {
                bestDistance = distance;
                bestSensor = sensor;
            }
        }

        return bestSensor;
    }

    function selectSensorAt(mouseX, mouseY, width, height) {
        const sensor = sensorAt(mouseX, mouseY, width, height);
        if (sensor !== null)
            root.viewModel.selectChannel(sensor.row);
    }

    function hoverText(sensor) {
        if (sensor === null)
            return "";
        const position = "(" + Number(sensor.x).toFixed(3).replace(/\.?0+$/, "") + ", "
                         + Number(sensor.y).toFixed(3).replace(/\.?0+$/, "") + ", "
                         + Number(sensor.z).toFixed(3).replace(/\.?0+$/, "") + ")";
        const direction = "(" + Number(sensor.ux).toFixed(3).replace(/\.?0+$/, "") + ", "
                          + Number(sensor.uy).toFixed(3).replace(/\.?0+$/, "") + ", "
                          + Number(sensor.uz).toFixed(3).replace(/\.?0+$/, "") + ")";
        return "Channel " + sensor.channelNo + " / " + sensor.channelId + "\n"
               + sensor.measurand + " / " + sensor.direction + "\n"
               + "Position " + position + "\n"
               + "Vector " + direction;
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Label {
            text: root.viewModel.geometrySummary
            color: "#666666"
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        ComboBox {
            id: viewModeBox
            model: ["Isometric", "Plan", "X-Z", "Y-Z"]
            currentIndex: 0
            onCurrentTextChanged: {
                root.viewMode = currentText;
                root.hoveredSensor = null;
                sensorCanvas.requestPaint();
            }
        }
        ComboBox {
            id: floorBox
            model: root.floorOptions
            textRole: "label"
            currentIndex: root.selectedFloorIndex
            Layout.preferredWidth: 150
            onCurrentIndexChanged: {
                root.selectedFloorIndex = currentIndex;
                root.hoveredSensor = null;
                sensorCanvas.requestPaint();
            }
        }
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

                const bounds = root.projectedBounds();
                const floors = root.visibleFloors();
                const sensors = root.visibleSensors();

                if (!bounds.valid) {
                    ctx.fillStyle = "#777777";
                    ctx.fillText("No geometry", 12, 24);
                    return;
                }

                const transform = root.viewTransform(width, height, bounds);

                function drawArrow(x1, y1, x2, y2, color, headSize) {
                    ctx.strokeStyle = color;
                    ctx.fillStyle = color;
                    ctx.beginPath();
                    ctx.moveTo(x1, y1);
                    ctx.lineTo(x2, y2);
                    ctx.stroke();

                    const angle = Math.atan2(y2 - y1, x2 - x1);
                    ctx.beginPath();
                    ctx.moveTo(x2, y2);
                    ctx.lineTo(x2 - headSize * Math.cos(angle - Math.PI / 6), y2 - headSize * Math.sin(angle - Math.PI / 6));
                    ctx.lineTo(x2 - headSize * Math.cos(angle + Math.PI / 6), y2 - headSize * Math.sin(angle + Math.PI / 6));
                    ctx.closePath();
                    ctx.fill();
                }

                function drawFloor(floor, filled) {
                    if (floor.points.length < 2)
                        return;
                    const first = root.screenPoint(root.projectPoint(floor.points[0].x, floor.points[0].y, floor.z), transform);
                    ctx.beginPath();
                    ctx.moveTo(first.x, first.y);
                    for (let i = 1; i < floor.points.length; ++i) {
                        const point = root.screenPoint(root.projectPoint(floor.points[i].x, floor.points[i].y, floor.z), transform);
                        ctx.lineTo(point.x, point.y);
                    }
                    if (filled) {
                        ctx.closePath();
                        ctx.fillStyle = "rgba(80, 126, 164, 0.16)";
                        ctx.fill();
                    }
                    ctx.strokeStyle = "#68707a";
                    ctx.lineWidth = 1;
                    ctx.stroke();
                }

                for (let i = 0; i < floors.length; ++i) {
                    drawFloor(floors[i], root.viewMode === "Isometric" || i === 0);
                }

                if (root.viewMode === "Isometric" && floors.length >= 2) {
                    const bottom = floors[0];
                    const top = floors[floors.length - 1];
                    const count = Math.min(bottom.points.length, top.points.length);
                    ctx.strokeStyle = "#b8c0c8";
                    ctx.lineWidth = 1;
                    for (let i = 0; i < count; ++i) {
                        const p1 = root.screenPoint(root.projectPoint(bottom.points[i].x, bottom.points[i].y, bottom.z), transform);
                        const p2 = root.screenPoint(root.projectPoint(top.points[i].x, top.points[i].y, top.z), transform);
                        ctx.beginPath();
                        ctx.moveTo(p1.x, p1.y);
                        ctx.lineTo(p2.x, p2.y);
                        ctx.stroke();
                    }
                }

                const axisOrigin = { x: 50, y: height - 50 };
                ctx.font = "12px sans-serif";
                const axisDefs = root.viewMode === "Plan"
                    ? [{ label: "X", x: 1, y: 0 }, { label: "Y", x: 0, y: 1 }]
                    : (root.viewMode === "X-Z"
                        ? [{ label: "X", x: 1, y: 0 }, { label: "Z", x: 0, y: 1 }]
                        : (root.viewMode === "Y-Z"
                            ? [{ label: "Y", x: 1, y: 0 }, { label: "Z", x: 0, y: 1 }]
                            : [{ label: "X", x: 1, y: 0.45 }, { label: "Y", x: -1, y: 0.45 }, { label: "Z", x: 0, y: 1 }]));
                for (let axis of axisDefs) {
                    const norm = Math.max(Math.sqrt(axis.x * axis.x + axis.y * axis.y), 1e-6);
                    const x2 = axisOrigin.x + axis.x / norm * 28;
                    const y2 = axisOrigin.y - axis.y / norm * 28;
                    drawArrow(axisOrigin.x, axisOrigin.y, x2, y2, "#343a40", 4);
                    ctx.fillStyle = "#343a40";
                    ctx.fillText(axis.label, x2 + 3, y2 - 3);
                }
                if (root.viewMode === "Plan" || root.viewMode === "Isometric") {
                    let north = { x: 0, y: 1 };
                    if (root.viewMode === "Plan") {
                        const parsedAngle = parseFloat(root.viewModel.northAngle);
                        const angle = (isNaN(parsedAngle) ? 0 : parsedAngle) * Math.PI / 180.0;
                        north = root.projectVector(Math.sin(angle), Math.cos(angle), 0);
                    } else {
                        for (let axis of root.viewModel.structureAxes) {
                            if (axis.label === "N") {
                                north = { x: axis.x2, y: axis.y2 };
                                break;
                            }
                        }
                    }
                    const norm = Math.max(Math.sqrt(north.x * north.x + north.y * north.y), 1e-6);
                    const x2 = axisOrigin.x + north.x / norm * 48;
                    const y2 = axisOrigin.y - north.y / norm * 48;
                    drawArrow(axisOrigin.x, axisOrigin.y, x2, y2, "#c92a2a", 5);
                    ctx.fillStyle = "#c92a2a";
                    ctx.fillText("N", x2 + 3, y2 - 3);
                }

                for (let sensor of sensors) {
                    const point = root.screenPoint(root.sensorPoint(sensor), transform);
                    const hovered = root.hoveredSensor !== null && root.hoveredSensor.row === sensor.row;
                    ctx.fillStyle = sensor.selected ? "#d92332" : (hovered ? "#1c7ed6" : "#0b7285");
                    ctx.strokeStyle = sensor.selected ? "#8f101c" : (hovered ? "#0b4f8a" : "#0b7285");
                    ctx.beginPath();
                    ctx.arc(point.x, point.y, sensor.selected ? 6 : (hovered ? 5 : 4), 0, Math.PI * 2);
                    ctx.fill();
                    if (sensor.selected || hovered) {
                        ctx.lineWidth = 1.5;
                        ctx.stroke();
                        ctx.lineWidth = 1;
                    }

                    const direction = root.sensorDirection(sensor);
                    if (Math.abs(direction.x) > 1e-9 || Math.abs(direction.y) > 1e-9) {
                        const norm = Math.max(Math.sqrt(direction.x * direction.x + direction.y * direction.y), 1e-6);
                        drawArrow(point.x, point.y, point.x + direction.x / norm * 20, point.y - direction.y / norm * 20, sensor.selected ? "#8f101c" : "#0b7285", 5);
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: sensorCanvas
            hoverEnabled: true
            onPositionChanged: function (mouse) {
                root.hoveredSensor = root.sensorAt(mouse.x, mouse.y, sensorCanvas.width, sensorCanvas.height);
                sensorCanvas.requestPaint();
            }
            onExited: {
                root.hoveredSensor = null;
                sensorCanvas.requestPaint();
            }
            onClicked: function (mouse) {
                root.selectSensorAt(mouse.x, mouse.y, sensorCanvas.width, sensorCanvas.height);
            }
        }

        Rectangle {
            visible: root.hoveredSensor !== null
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 16
            width: hoverLabel.implicitWidth + 18
            height: hoverLabel.implicitHeight + 14
            radius: 4
            color: "#ffffff"
            border.color: "#99a7b5"

            Label {
                id: hoverLabel
                anchors.centerIn: parent
                text: root.hoverText(root.hoveredSensor)
                font.pixelSize: 12
                color: "#263238"
            }
        }
    }

    Connections {
        target: root.viewModel
        function onGeometryUpdated() {
            root.refreshFloorOptions();
            root.hoveredSensor = null;
            sensorCanvas.requestPaint();
        }
    }
}
