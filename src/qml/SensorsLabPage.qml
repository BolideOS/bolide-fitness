import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: sensorsPage

    Rectangle { anchors.fill: parent; color: "#000000" }

    property var sensorList: SensorsLab.availableSensorList
    property bool recording: SensorsLab.recording
    property int selectedSensor: -1  // selected sensor type for graph

    Flickable {
        anchors.fill: parent
        contentHeight: col.height + Dims.h(12)
        flickableDirection: Flickable.VerticalFlick
        clip: true

        Column {
            id: col
            width: parent.width
            topPadding: Dims.h(12)
            bottomPadding: Dims.h(8)
            spacing: Dims.h(2)

            // ── Title ───────────────────────────────────────────────
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Sensors & Lab"
                text: qsTrId("id-sensors-lab")
                font.pixelSize: Dims.l(5)
                font.bold: true
                color: "white"
            }

            // ── Recording controls ──────────────────────────────────
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Dims.w(3)

                Rectangle {
                    width: Dims.w(34); height: Dims.h(7)
                    radius: height / 2
                    color: recording ? "#F44336" : "#4CAF50"

                    Label {
                        anchors.centerIn: parent
                        text: recording ? "⏹ Stop" : "⏺ Record"
                        font.pixelSize: Dims.l(2.8)
                        color: "white"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: recording ? SensorsLab.stopRecording() : SensorsLab.startRecording()
                    }
                }

                Rectangle {
                    width: Dims.w(30); height: Dims.h(7)
                    radius: height / 2
                    color: "#333"
                    visible: SensorsLab.sampleCount > 0

                    Label {
                        anchors.centerIn: parent
                        text: "📁 Export"
                        font.pixelSize: Dims.l(2.8)
                        color: "white"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: SensorsLab.exportCSV()
                    }
                }
            }

            // ── Recording info ──────────────────────────────────────
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Dims.w(4)
                visible: recording || SensorsLab.sampleCount > 0

                Label {
                    text: SensorsLab.sampleCount + " samples"
                    font.pixelSize: Dims.l(2.5)
                    color: "#80FFFFFF"
                }

                Label {
                    text: SensorsLab.recordingDuration.toFixed(1) + "s"
                    font.pixelSize: Dims.l(2.5)
                    color: "#80FFFFFF"
                    visible: recording
                }

                // Recording dot
                Rectangle {
                    width: Dims.w(2.5); height: width
                    radius: width / 2
                    color: "#F44336"
                    visible: recording
                    anchors.verticalCenter: parent.verticalCenter

                    SequentialAnimation on opacity {
                        running: recording; loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 500 }
                        NumberAnimation { to: 1.0; duration: 500 }
                    }
                }
            }

            // ── Live graph (if sensor selected) ─────────────────────
            Item {
                width: Dims.w(86)
                height: selectedSensor >= 0 ? Dims.h(22) : 0
                anchors.horizontalCenter: parent.horizontalCenter
                visible: selectedSensor >= 0
                clip: true

                Canvas {
                    id: liveGraph
                    anchors.fill: parent

                    property var samples: selectedSensor >= 0 ? SensorsLab.recentSamples(selectedSensor, 100) : []

                    Timer {
                        interval: 200
                        running: selectedSensor >= 0
                        repeat: true
                        onTriggered: {
                            liveGraph.samples = SensorsLab.recentSamples(selectedSensor, 100)
                            liveGraph.requestPaint()
                        }
                    }

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);

                        if (!samples || samples.length < 2) return;

                        var minVal = samples[0].v, maxVal = samples[0].v;
                        for (var i = 1; i < samples.length; i++) {
                            if (samples[i].v < minVal) minVal = samples[i].v;
                            if (samples[i].v > maxVal) maxVal = samples[i].v;
                        }

                        var range = maxVal - minVal;
                        if (range < 0.01) range = 1;

                        var padY = height * 0.08;
                        var graphH = height - 2 * padY;
                        var stepX = width / (samples.length - 1);

                        // Background grid
                        ctx.strokeStyle = "#15FFFFFF";
                        ctx.lineWidth = 1;
                        for (var g = 0; g <= 3; g++) {
                            var gy = padY + (g / 3.0) * graphH;
                            ctx.beginPath();
                            ctx.moveTo(0, gy);
                            ctx.lineTo(width, gy);
                            ctx.stroke();
                        }

                        // Data line
                        ctx.strokeStyle = "#E91E63";
                        ctx.lineWidth = 2;
                        ctx.beginPath();
                        for (var j = 0; j < samples.length; j++) {
                            var x = j * stepX;
                            var y = padY + graphH - ((samples[j].v - minVal) / range) * graphH;
                            if (j === 0) ctx.moveTo(x, y);
                            else ctx.lineTo(x, y);
                        }
                        ctx.stroke();

                        // Labels
                        ctx.fillStyle = "#80FFFFFF";
                        ctx.font = Dims.l(2) + "px sans-serif";
                        ctx.textAlign = "left";
                        ctx.fillText(maxVal.toFixed(1), 2, padY - 2);
                        ctx.fillText(minVal.toFixed(1), 2, height - 2);
                    }
                }

                // Close graph button
                Rectangle {
                    anchors { top: parent.top; right: parent.right; margins: 2 }
                    width: Dims.w(6); height: width; radius: width / 2
                    color: "#60000000"

                    Label {
                        anchors.centerIn: parent
                        text: "✕"
                        font.pixelSize: Dims.l(2.5)
                        color: "white"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: selectedSensor = -1
                    }
                }
            }

            // ── Sensor list ─────────────────────────────────────────
            Repeater {
                model: sensorList

                Rectangle {
                    width: Dims.w(86)
                    height: Dims.h(12)
                    anchors.horizontalCenter: parent.horizontalCenter
                    radius: Dims.l(2)
                    color: {
                        if (!modelData.available) return "#1A555555";
                        var active = false;
                        var actives = SensorsLab.activeSensors;
                        for (var k = 0; k < actives.length; k++) {
                            if (actives[k].type === modelData.type) { active = true; break; }
                        }
                        return active ? "#1AE91E63" : "#1A333333";
                    }
                    border.color: {
                        var active2 = false;
                        var actives2 = SensorsLab.activeSensors;
                        for (var k2 = 0; k2 < actives2.length; k2++) {
                            if (actives2[k2].type === modelData.type) { active2 = true; break; }
                        }
                        return active2 ? "#E91E63" : "transparent";
                    }
                    border.width: 1
                    opacity: modelData.available ? 1.0 : 0.4

                    Row {
                        anchors {
                            fill: parent
                            leftMargin: Dims.w(3)
                            rightMargin: Dims.w(3)
                        }
                        spacing: Dims.w(2)

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - startBtn.width - graphBtn.width - Dims.w(6)

                            Label {
                                text: modelData.name
                                font.pixelSize: Dims.l(3)
                                font.bold: true
                                color: "white"
                            }

                            Label {
                                text: {
                                    var latest = SensorsLab.latestValue(modelData.type);
                                    if (latest && latest.value !== undefined)
                                        return Number(latest.value).toFixed(2) + " " + modelData.unit;
                                    return modelData.unit + " • " + modelData.channels + "ch";
                                }
                                font.pixelSize: Dims.l(2.3)
                                color: "#80FFFFFF"
                            }
                        }

                        // Graph button
                        Rectangle {
                            id: graphBtn
                            width: Dims.w(9); height: Dims.h(7)
                            radius: height / 2
                            color: selectedSensor === modelData.type ? "#E91E63" : "#333"
                            anchors.verticalCenter: parent.verticalCenter
                            visible: modelData.available

                            Label {
                                anchors.centerIn: parent
                                text: "📈"
                                font.pixelSize: Dims.l(3)
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (selectedSensor === modelData.type)
                                        selectedSensor = -1;
                                    else
                                        selectedSensor = modelData.type;
                                }
                            }
                        }

                        // Start/stop button
                        Rectangle {
                            id: startBtn
                            width: Dims.w(12); height: Dims.h(7)
                            radius: height / 2
                            anchors.verticalCenter: parent.verticalCenter
                            visible: modelData.available

                            property bool active: {
                                var actives3 = SensorsLab.activeSensors;
                                for (var k3 = 0; k3 < actives3.length; k3++) {
                                    if (actives3[k3].type === modelData.type) return true;
                                }
                                return false;
                            }

                            color: active ? "#F44336" : "#4CAF50"

                            Label {
                                anchors.centerIn: parent
                                text: startBtn.active ? "Stop" : "Start"
                                font.pixelSize: Dims.l(2.5)
                                color: "white"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (startBtn.active)
                                        SensorsLab.stopStream(modelData.type);
                                    else
                                        SensorsLab.startStream(modelData.type, 100);
                                }
                            }
                        }
                    }
                }
            }

            Item { width: 1; height: Dims.h(8) }
        }
    }
}
