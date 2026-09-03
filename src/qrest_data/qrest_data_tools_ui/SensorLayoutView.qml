import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    required property var viewModel

    spacing: 6

    function geometryBounds() {
        return root.viewModel.structureViewBounds;
    }

    function canvasPoint(x, y, width, height, bounds) {
        const pad = 18;
        const spanX = Math.max(bounds.maxX - bounds.minX, 1e-6);
        const spanY = Math.max(bounds.maxY - bounds.minY, 1e-6);
        const scale = Math.min((width - pad * 2) / spanX, (height - pad * 2) / spanY);
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
        for (let sensor of root.viewModel.structureSensors) {
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

                const pad = 18;
                const spanX = Math.max(bounds.maxX - bounds.minX, 1e-6);
                const spanY = Math.max(bounds.maxY - bounds.minY, 1e-6);
                const scale = Math.min((width - pad * 2) / spanX, (height - pad * 2) / spanY);

                function screenPoint(x, y) {
                    return {
                        x: pad + (x - bounds.minX) * scale,
                        y: height - pad - (y - bounds.minY) * scale
                    };
                }

                ctx.lineWidth = 1;
                for (let edge of edges) {
                    const p1 = screenPoint(edge.x1, edge.y1);
                    const p2 = screenPoint(edge.x2, edge.y2);
                    ctx.strokeStyle = edge.kind === "vertical" ? "#b8c0c8" : "#68707a";
                    ctx.beginPath();
                    ctx.moveTo(p1.x, p1.y);
                    ctx.lineTo(p2.x, p2.y);
                    ctx.stroke();
                }

                const axisOrigin = screenPoint(bounds.minX, bounds.maxY);
                ctx.font = "11px sans-serif";
                for (let axis of axes) {
                    const length = axis.label === "N" ? 26 : 22;
                    const norm = Math.max(Math.abs(axis.x2) + Math.abs(axis.y2), 1e-6);
                    const x2 = axisOrigin.x + axis.x2 * scale / norm * length;
                    const y2 = axisOrigin.y - axis.y2 * scale / norm * length;
                    ctx.strokeStyle = axis.label === "N" ? "#c92a2a" : "#343a40";
                    ctx.fillStyle = ctx.strokeStyle;
                    ctx.beginPath();
                    ctx.moveTo(axisOrigin.x, axisOrigin.y);
                    ctx.lineTo(x2, y2);
                    ctx.stroke();
                    ctx.fillText(axis.label, x2 + 3, y2 - 3);
                }

                for (let sensor of sensors) {
                    const point = screenPoint(sensor.x, sensor.y);
                    ctx.fillStyle = sensor.selected ? "#d92332" : "#0b7285";
                    ctx.strokeStyle = sensor.selected ? "#d92332" : "#0b7285";
                    ctx.beginPath();
                    ctx.arc(point.x, point.y, sensor.selected ? 5 : 4, 0, Math.PI * 2);
                    ctx.fill();

                    if (Math.abs(sensor.dx) > 1e-9 || Math.abs(sensor.dy) > 1e-9) {
                        const norm = Math.max(Math.sqrt(sensor.dx * sensor.dx + sensor.dy * sensor.dy), 1e-6);
                        ctx.beginPath();
                        ctx.moveTo(point.x, point.y);
                        ctx.lineTo(point.x + sensor.dx / norm * 18, point.y - sensor.dy / norm * 18);
                        ctx.stroke();
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
