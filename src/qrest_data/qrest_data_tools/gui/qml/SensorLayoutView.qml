import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    required property var viewModel

    spacing: 6

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
            minY: bounds.minY,
            maxY: bounds.maxY,
            scale: scale,
            offsetX: (width - drawWidth) / 2,
            offsetY: (height - drawHeight) / 2
        };
    }

    function screenPoint(x, y, transform) {
        return {
            x: transform.offsetX + (x - transform.minX) * transform.scale,
            y: transform.offsetY + (transform.maxY - y) * transform.scale
        };
    }

    function selectSensorAt(mouseX, mouseY, width, height) {
        const bounds = root.viewModel.structureViewBounds;
        if (!bounds.valid)
            return;

        const transform = viewTransform(width, height, bounds);
        let bestRow = -1;
        let bestDistance = 144;
        for (let sensor of root.viewModel.structureSensors) {
            const point = screenPoint(sensor.x, sensor.y, transform);
            const dx = point.x - mouseX;
            const dy = point.y - mouseY;
            const distance = dx * dx + dy * dy;
            if (distance < bestDistance) {
                bestDistance = distance;
                bestRow = sensor.row;
            }
        }

        if (bestRow >= 0) {
            root.viewModel.selectChannel(bestRow);
        }
    }

    Label {
        text: root.viewModel.geometrySummary
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

                const bounds = root.viewModel.structureViewBounds;
                const edges = root.viewModel.structureEdges;
                const axes = root.viewModel.structureAxes;
                const sensors = root.viewModel.structureSensors;

                if (!bounds.valid) {
                    ctx.fillStyle = "#777777";
                    ctx.fillText("No geometry", 12, 24);
                    return;
                }

                const transform = root.viewTransform(width, height, bounds);

                ctx.lineWidth = 1;
                for (let edge of edges) {
                    const p1 = root.screenPoint(edge.x1, edge.y1, transform);
                    const p2 = root.screenPoint(edge.x2, edge.y2, transform);
                    ctx.strokeStyle = edge.kind === "vertical" ? "#b8c0c8" : "#68707a";
                    ctx.beginPath();
                    ctx.moveTo(p1.x, p1.y);
                    ctx.lineTo(p2.x, p2.y);
                    ctx.stroke();
                }

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

                const axisOrigin = {
                    x: 26,
                    y: height - 26
                };
                ctx.font = "11px sans-serif";
                for (let axis of axes) {
                    const length = axis.label === "N" ? 30 : 24;
                    const norm = Math.max(Math.abs(axis.x2) + Math.abs(axis.y2), 1e-6);
                    const x2 = axisOrigin.x + axis.x2 / norm * length;
                    const y2 = axisOrigin.y - axis.y2 / norm * length;
                    const color = axis.label === "N" ? "#c92a2a" : "#343a40";
                    drawArrow(axisOrigin.x, axisOrigin.y, x2, y2, color, 4);
                    ctx.fillStyle = color;
                    ctx.fillText(axis.label, x2 + 3, y2 - 3);
                }

                for (let sensor of sensors) {
                    const point = root.screenPoint(sensor.x, sensor.y, transform);
                    ctx.fillStyle = sensor.selected ? "#d92332" : "#0b7285";
                    ctx.strokeStyle = sensor.selected ? "#d92332" : "#0b7285";
                    ctx.beginPath();
                    ctx.arc(point.x, point.y, sensor.selected ? 7 : 4, 0, Math.PI * 2);
                    ctx.fill();
                    if (sensor.selected) {
                        ctx.lineWidth = 2;
                        ctx.strokeStyle = "#8f101c";
                        ctx.stroke();
                        ctx.lineWidth = 1;
                    }

                    if (Math.abs(sensor.dx) > 1e-9 || Math.abs(sensor.dy) > 1e-9) {
                        const norm = Math.max(Math.sqrt(sensor.dx * sensor.dx + sensor.dy * sensor.dy), 1e-6);
                        drawArrow(point.x, point.y, point.x + sensor.dx / norm * 20, point.y - sensor.dy / norm * 20, sensor.selected ? "#8f101c" : "#0b7285", 5);
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: sensorCanvas
            onClicked: function (mouse) {
                root.selectSensorAt(mouse.x, mouse.y, sensorCanvas.width, sensorCanvas.height);
            }
        }
    }

    Connections {
        target: root.viewModel
        function onGeometryUpdated() {
            sensorCanvas.requestPaint();
        }
    }
}
