/*
 * ScreenGlance.qml – Generic glance content for L1 folder display.
 *
 * Dispatches to the appropriate glance visualization based on
 * the glanceProvider property set by the parent FolderGlance.
 *
 * Each provider type reads from the corresponding domain singleton
 * to show a compact summary: value + label + optional sparkline.
 *
 * This component is also used by the GlanceProvider C++ class
 * to compute data for the launcher D-Bus API.
 */
import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: root

    property string glanceProvider: parent ? parent.glanceProvider : ""
    property color  glanceColor: parent ? parent.glanceColor : "white"

    width: parent ? parent.width : 0
    height: glanceColumn.height

    Column {
        id: glanceColumn
        width: parent.width
        spacing: Dims.h(1)

        // ── Sleep glance ────────────────────────────────────────────────
        Column {
            visible: glanceProvider === "sleep"
            width: parent.width
            spacing: Dims.h(0.5)

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    if (SleepTracker.tracking)
                        return "● " + qsTrId("id-tracking-sleep")
                    var total = SleepTracker.totalMinutes
                    if (total <= 0) return "—"
                    var h = Math.floor(total / 60)
                    var m = total % 60
                    return h + "h " + m + "m"
                }
                font.pixelSize: Dims.l(6)
                font.bold: true
                color: glanceColor
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    if (SleepTracker.tracking)
                        return ""
                    var q = SleepTracker.qualityScore
                    if (q > 0) return qsTrId("id-quality") + ": " + Math.round(q)
                    return ""
                }
                font.pixelSize: Dims.l(3)
                color: "#80FFFFFF"
            }

            // Mini stage bar
            Row {
                visible: SleepTracker.totalMinutes > 0 && !SleepTracker.tracking
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width * 0.6
                height: Dims.h(2)

                Rectangle {
                    height: parent.height
                    width: parent.width * (SleepTracker.deepMinutes / Math.max(1, SleepTracker.totalMinutes))
                    color: "#3F51B5"
                }
                Rectangle {
                    height: parent.height
                    width: parent.width * (SleepTracker.lightMinutes / Math.max(1, SleepTracker.totalMinutes))
                    color: "#7986CB"
                }
                Rectangle {
                    height: parent.height
                    width: parent.width * (SleepTracker.remMinutes / Math.max(1, SleepTracker.totalMinutes))
                    color: "#9C27B0"
                }
                Rectangle {
                    height: parent.height
                    width: parent.width * (SleepTracker.awakeMinutes / Math.max(1, SleepTracker.totalMinutes))
                    color: "#F44336"
                }
            }
        }

        // ── Body metrics glance ─────────────────────────────────────────
        Column {
            visible: glanceProvider === "body"
            width: parent.width
            spacing: Dims.h(0.5)

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: BodyMetrics.heartRate > 0 ? BodyMetrics.heartRate.toString() : "—"
                font.pixelSize: Dims.l(6)
                font.bold: true
                color: "#F44336"
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: BodyMetrics.heartRate > 0
                    ? qsTrId("id-bpm") + " · " + qsTrId("id-stress") + " " + BodyMetrics.stressIndex
                    : ""
                font.pixelSize: Dims.l(3)
                color: "#80FFFFFF"
            }
        }

        // ── Readiness glance ────────────────────────────────────────────
        Column {
            visible: glanceProvider === "readiness"
            width: parent.width
            spacing: Dims.h(0.5)

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: ReadinessScore.score > 0 ? Math.round(ReadinessScore.score).toString() : "—"
                font.pixelSize: Dims.l(6)
                font.bold: true
                color: {
                    var s = ReadinessScore.score
                    if (s >= 85) return "#4CAF50"
                    if (s >= 70) return "#8BC34A"
                    if (s >= 50) return "#FF9800"
                    if (s >= 30) return "#FF5722"
                    return "#F44336"
                }
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: ReadinessScore.label || ""
                font.pixelSize: Dims.l(3)
                color: "#80FFFFFF"
            }
        }

        // ── Trends glance ───────────────────────────────────────────────
        Column {
            visible: glanceProvider === "trends"
            width: parent.width
            spacing: Dims.h(0.5)

            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.l(6)
                height: width
                name: "ios-trending-up"
                color: glanceColor
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "View trends"
                text: qsTrId("id-view-trends")
                font.pixelSize: Dims.l(3)
                color: "#80FFFFFF"
            }
        }

        // ── Sensors Lab glance ──────────────────────────────────────────
        Column {
            visible: glanceProvider === "sensors"
            width: parent.width
            spacing: Dims.h(0.5)

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    if (SensorsLab.recording)
                        return "● " + qsTrId("id-recording")
                    return qsTrId("id-sensors")
                }
                font.pixelSize: Dims.l(4)
                font.bold: true
                color: SensorsLab.recording ? "#F44336" : glanceColor
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: SensorsLab.recording
                    ? SensorsLab.sampleCount + " " + qsTrId("id-samples")
                    : ""
                font.pixelSize: Dims.l(3)
                color: "#80FFFFFF"
            }
        }

        // ── Fallback (unknown provider) ─────────────────────────────────
        Label {
            visible: glanceProvider !== "sleep" && glanceProvider !== "body"
                     && glanceProvider !== "readiness" && glanceProvider !== "trends"
                     && glanceProvider !== "sensors" && glanceProvider !== ""
            anchors.horizontalCenter: parent.horizontalCenter
            text: "—"
            font.pixelSize: Dims.l(5)
            color: "#60FFFFFF"
        }
    }

    // ── Update glance provider D-Bus data ───────────────────────────────
    Timer {
        interval: 5000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: updateGlanceData()
    }

    function updateGlanceData() {
        if (!glanceProvider) return

        var data = {}
        switch (glanceProvider) {
        case "sleep":
            data = {
                "title": qsTrId("id-sleep"),
                "value": SleepTracker.totalMinutes > 0
                    ? Math.floor(SleepTracker.totalMinutes / 60) + "h " + (SleepTracker.totalMinutes % 60) + "m"
                    : "—",
                "label": SleepTracker.qualityScore > 0
                    ? qsTrId("id-quality") + ": " + Math.round(SleepTracker.qualityScore)
                    : "",
                "progress": SleepTracker.qualityScore / 100.0,
                "icon": "ios-moon",
                "color": "#9C27B0"
            }
            break
        case "body":
            data = {
                "title": qsTrId("id-body-metrics"),
                "value": BodyMetrics.heartRate > 0 ? BodyMetrics.heartRate + " bpm" : "—",
                "label": "Stress: " + BodyMetrics.stressIndex,
                "progress": BodyMetrics.heartRate / 200.0,
                "icon": "ios-body-outline",
                "color": "#E91E63"
            }
            break
        case "readiness":
            data = {
                "title": qsTrId("id-readiness"),
                "value": ReadinessScore.score > 0 ? Math.round(ReadinessScore.score).toString() : "—",
                "label": ReadinessScore.label || "",
                "progress": ReadinessScore.score / 100.0,
                "icon": "ios-pulse-outline",
                "color": "#2196F3"
            }
            break
        case "trends":
            data = {
                "title": qsTrId("id-trends"),
                "value": "",
                "label": qsTrId("id-view-trends"),
                "progress": 0,
                "icon": "ios-trending-up",
                "color": "#00BCD4"
            }
            break
        case "sensors":
            data = {
                "title": qsTrId("id-sensors-lab"),
                "value": SensorsLab.recording ? SensorsLab.sampleCount + " samples" : "—",
                "label": SensorsLab.recording ? qsTrId("id-recording") : "",
                "progress": 0,
                "icon": "ios-flask-outline",
                "color": "#FF9800"
            }
            break
        }

        if (Object.keys(data).length > 0) {
            GlanceProvider.updateGlance(glanceProvider, data)
        }
    }
}
