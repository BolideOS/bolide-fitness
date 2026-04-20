import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: trendsPage

    Rectangle { anchors.fill: parent; color: "#000000" }

    // ── State ───────────────────────────────────────────────────────────
    property var metrics: TrendsManager.availableMetrics
    property int selectedIndex: 0
    property string selectedMetric: metrics.length > 0 ? metrics[selectedIndex].id : ""
    property string selectedName:   metrics.length > 0 ? metrics[selectedIndex].name : ""
    property string selectedUnit:   metrics.length > 0 ? metrics[selectedIndex].unit : ""
    property int viewDays: 30

    property var trendData: []
    property var baselines: TrendsManager.baselines

    function refresh() {
        if (selectedMetric !== "")
            trendData = TrendsManager.trendData(selectedMetric, viewDays)
    }

    Component.onCompleted: refresh()
    onSelectedMetricChanged: refresh()
    onViewDaysChanged: refresh()

    Connections {
        target: TrendsManager
        onBaselinesChanged: baselines = TrendsManager.baselines
    }

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
                //% "Trends"
                text: qsTrId("id-trends")
                font.pixelSize: Dims.l(5)
                font.bold: true
                color: "white"
            }

            // ── Metric selector (tap to cycle) ──────────────────────
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.w(70)
                height: Dims.h(8)
                radius: height / 2
                color: "#1A4CAF50"
                border.color: "#4CAF50"; border.width: 1

                Label {
                    anchors.centerIn: parent
                    text: selectedName + " (" + selectedUnit + ")"
                    font.pixelSize: Dims.l(3)
                    color: "#4CAF50"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        selectedIndex = (selectedIndex + 1) % metrics.length
                    }
                }
            }

            // ── Time range selector ─────────────────────────────────
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Dims.w(2)

                Repeater {
                    model: [
                        { label: "7d",  days: 7 },
                        { label: "30d", days: 30 },
                        { label: "90d", days: 90 }
                    ]

                    Rectangle {
                        width: Dims.w(16); height: Dims.h(6)
                        radius: height / 2
                        color: viewDays === modelData.days ? "#4CAF50" : "#333"

                        Label {
                            anchors.centerIn: parent
                            text: modelData.label
                            font.pixelSize: Dims.l(2.5)
                            color: viewDays === modelData.days ? "white" : "#80FFFFFF"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: viewDays = modelData.days
                        }
                    }
                }
            }

            // ── Trend graph ─────────────────────────────────────────
            Item {
                width: Dims.w(86)
                height: Dims.h(25)
                anchors.horizontalCenter: parent.horizontalCenter

                Canvas {
                    id: trendCanvas
                    anchors.fill: parent

                    property var data: trendData

                    onDataChanged: requestPaint()

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);

                        if (!data || data.length < 2) {
                            ctx.fillStyle = "#40FFFFFF";
                            ctx.font = Dims.l(3) + "px sans-serif";
                            ctx.textAlign = "center";
                            ctx.fillText("No data", width / 2, height / 2);
                            return;
                        }

                        // Find min/max
                        var minVal = data[0].value, maxVal = data[0].value;
                        for (var i = 1; i < data.length; i++) {
                            if (data[i].value < minVal) minVal = data[i].value;
                            if (data[i].value > maxVal) maxVal = data[i].value;
                        }

                        var range = maxVal - minVal;
                        if (range < 0.001) range = 1;

                        var padY = height * 0.1;
                        var graphH = height - 2 * padY;
                        var stepX = width / (data.length - 1);

                        // Grid lines
                        ctx.strokeStyle = "#20FFFFFF";
                        ctx.lineWidth = 1;
                        for (var g = 0; g <= 4; g++) {
                            var gy = padY + (g / 4.0) * graphH;
                            ctx.beginPath();
                            ctx.moveTo(0, gy);
                            ctx.lineTo(width, gy);
                            ctx.stroke();
                        }

                        // Line
                        ctx.strokeStyle = "#4CAF50";
                        ctx.lineWidth = 2;
                        ctx.beginPath();
                        for (var j = 0; j < data.length; j++) {
                            var x = j * stepX;
                            var y = padY + graphH - ((data[j].value - minVal) / range) * graphH;
                            if (j === 0) ctx.moveTo(x, y);
                            else ctx.lineTo(x, y);
                        }
                        ctx.stroke();

                        // Fill gradient
                        var grad = ctx.createLinearGradient(0, padY, 0, height);
                        grad.addColorStop(0, "#404CAF50");
                        grad.addColorStop(1, "#004CAF50");
                        ctx.fillStyle = grad;
                        ctx.lineTo(width, height);
                        ctx.lineTo(0, height);
                        ctx.closePath();
                        ctx.fill();

                        // Min/Max labels
                        ctx.fillStyle = "#80FFFFFF";
                        ctx.font = Dims.l(2) + "px sans-serif";
                        ctx.textAlign = "left";
                        ctx.fillText(maxVal.toFixed(1), 2, padY - 2);
                        ctx.fillText(minVal.toFixed(1), 2, height - 2);
                    }
                }
            }

            // ── Trend direction indicator ───────────────────────────
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Dims.w(2)

                property int dir: selectedMetric !== "" ? TrendsManager.trendDirection(selectedMetric) : 0
                property double pct: selectedMetric !== "" ? TrendsManager.trendChangePercent(selectedMetric) : 0

                Label {
                    text: parent.dir > 0 ? "▲" : parent.dir < 0 ? "▼" : "→"
                    font.pixelSize: Dims.l(4)
                    color: parent.dir > 0 ? "#4CAF50" : parent.dir < 0 ? "#F44336" : "#FFC107"
                    anchors.verticalCenter: parent.verticalCenter
                }

                Label {
                    text: (parent.pct >= 0 ? "+" : "") + parent.pct.toFixed(1) + "% vs 30d"
                    font.pixelSize: Dims.l(3)
                    color: "#B0FFFFFF"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // ── Current value ───────────────────────────────────────
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    if (trendData.length === 0) return "--";
                    var latest = trendData[trendData.length - 1].value;
                    return latest.toFixed(1) + " " + selectedUnit;
                }
                font.pixelSize: Dims.l(6)
                font.bold: true
                color: "white"
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "latest"
                font.pixelSize: Dims.l(2.5)
                color: "#60FFFFFF"
            }

            // ── Key baselines ───────────────────────────────────────
            Item { width: 1; height: Dims.h(1) }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Baselines"
                font.pixelSize: Dims.l(3.5)
                font.bold: true
                color: "#2196F3"
            }

            Grid {
                anchors.horizontalCenter: parent.horizontalCenter
                columns: 2
                columnSpacing: Dims.w(4)
                rowSpacing: Dims.h(1.5)

                Repeater {
                    model: [
                        { key: "resting_hr_7d",  label: "RHR 7d",    unit: "bpm", color: "#F44336" },
                        { key: "hrv_7d",          label: "HRV 7d",    unit: "ms",  color: "#9C27B0" },
                        { key: "sleep_quality_7d",label: "Sleep 7d",  unit: "/100",color: "#3F51B5" },
                        { key: "steps_7d",        label: "Steps 7d",  unit: "",    color: "#FF9800" },
                        { key: "readiness_7d",    label: "Ready 7d",  unit: "/100",color: "#4CAF50" },
                        { key: "stress_7d",       label: "Stress 7d", unit: "",    color: "#E91E63" }
                    ]

                    Row {
                        spacing: Dims.w(1)

                        Rectangle {
                            width: Dims.w(2); height: Dims.h(4)
                            radius: 2
                            color: modelData.color
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Column {
                            Label {
                                text: modelData.label
                                font.pixelSize: Dims.l(2.3)
                                color: "#80FFFFFF"
                            }
                            Label {
                                property var val: baselines[modelData.key]
                                text: val !== undefined && val !== 0 ? Number(val).toFixed(modelData.unit === "" ? 0 : 1) + modelData.unit : "--"
                                font.pixelSize: Dims.l(3)
                                font.bold: true
                                color: "white"
                            }
                        }
                    }
                }
            }

            // ── Refresh button ──────────────────────────────────────
            Item { width: 1; height: Dims.h(2) }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.w(40); height: Dims.h(8)
                radius: height / 2
                color: "#333"

                Label {
                    anchors.centerIn: parent
                    text: "⟳ Refresh"
                    font.pixelSize: Dims.l(3)
                    color: "white"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        TrendsManager.recomputeBaselines()
                        refresh()
                    }
                }
            }

            Item { width: 1; height: Dims.h(8) }
        }
    }
}
