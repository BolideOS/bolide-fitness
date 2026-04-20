import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0
import Nemo.Configuration 1.0

Item {
    id: hrScreen

    ConfigurationGroup {
        id: zoneSettings
        path: "/bolide-fitness/settings"
        property int zone1Max: 100
        property int zone2Max: 120
        property int zone3Max: 140
        property int zone4Max: 160
    }

    function getHRZone(bpm) {
        if (bpm < zoneSettings.zone1Max) return { name: qsTrId("id-zone-warmup"),   color: "#9E9E9E" }
        if (bpm < zoneSettings.zone2Max) return { name: qsTrId("id-zone-fat-burn"), color: "#4CAF50" }
        if (bpm < zoneSettings.zone3Max) return { name: qsTrId("id-zone-cardio"),   color: "#FF9800" }
        if (bpm < zoneSettings.zone4Max) return { name: qsTrId("id-zone-peak"),     color: "#F44336" }
        return { name: qsTrId("id-zone-maximum"), color: "#C62828" }
    }

    property var currentZone: getHRZone(WorkoutManager.heartRate)

    Rectangle { anchors.fill: parent; color: "#000000" }

    Column {
        anchors.fill: parent
        anchors.margins: Dims.w(4)
        spacing: Dims.h(2)

        // ── Current HR display ──────────────────────────────────────────
        Item {
            width: parent.width
            height: Dims.h(30)

            Column {
                anchors.centerIn: parent
                spacing: Dims.h(1)

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Dims.w(2)

                    Label {
                        text: WorkoutManager.heartRate > 0 ? WorkoutManager.heartRate.toString() : "--"
                        font.pixelSize: Dims.l(22)
                        font.bold: true
                        color: currentZone.color
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Label {
                        //% "bpm"
                        text: qsTrId("id-bpm")
                        font.pixelSize: Dims.l(6)
                        color: "#80FFFFFF"
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: Dims.h(2)
                    }
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Dims.w(40)
                    height: Dims.h(5)
                    radius: height / 2
                    color: currentZone.color

                    Label {
                        anchors.centerIn: parent
                        text: currentZone.name
                        font.pixelSize: Dims.l(3)
                        font.bold: true
                        color: "white"
                    }
                }
            }
        }

        // ── HR Graph ────────────────────────────────────────────────────
        Item {
            width: parent.width
            height: Dims.h(42)

            Rectangle {
                anchors.fill: parent
                color: "#1A1A1A"
                radius: Dims.w(2)
                border.width: 1
                border.color: "#333333"
            }

            Canvas {
                id: hrCanvas
                anchors.fill: parent
                anchors.margins: Dims.w(2)

                property var hrData: WorkoutManager.hrHistory
                onHrDataChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()

                    if (!hrData || hrData.length < 2) {
                        ctx.fillStyle = "#80FFFFFF"
                        ctx.font = "12px sans-serif"
                        ctx.textAlign = "center"
                        ctx.fillText("Waiting for HR data...", width / 2, height / 2)
                        return
                    }

                    var minBpm = 999, maxBpm = 0
                    for (var i = 0; i < hrData.length; i++) {
                        var b = hrData[i].bpm
                        if (b > 0) { minBpm = Math.min(minBpm, b); maxBpm = Math.max(maxBpm, b) }
                    }
                    if (maxBpm === 0) return

                    minBpm = Math.max(40, minBpm - 10)
                    maxBpm = Math.min(200, maxBpm + 10)
                    var range = maxBpm - minBpm
                    if (range < 20) { range = 20; minBpm = Math.max(40, (minBpm+maxBpm)/2 - 10); maxBpm = minBpm + 20 }

                    var w = width, h = height

                    // Zone backgrounds
                    var zones = [
                        [40, zoneSettings.zone1Max, "#9E9E9E"],
                        [zoneSettings.zone1Max, zoneSettings.zone2Max, "#4CAF50"],
                        [zoneSettings.zone2Max, zoneSettings.zone3Max, "#FF9800"],
                        [zoneSettings.zone3Max, zoneSettings.zone4Max, "#F44336"],
                        [zoneSettings.zone4Max, 200, "#C62828"]
                    ]
                    for (var z = 0; z < zones.length; z++) {
                        var y1 = h - ((zones[z][1] - minBpm) / range) * h
                        var y2 = h - ((zones[z][0] - minBpm) / range) * h
                        ctx.fillStyle = zones[z][2]
                        ctx.globalAlpha = 0.08
                        ctx.fillRect(0, Math.max(0,y1), w, Math.min(h,y2) - Math.max(0,y1))
                    }
                    ctx.globalAlpha = 1.0

                    // Grid
                    ctx.strokeStyle = "#333333"
                    ctx.lineWidth = 1
                    for (var g = Math.ceil(minBpm/20)*20; g <= maxBpm; g += 20) {
                        var gy = h - ((g - minBpm) / range) * h
                        ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(w, gy); ctx.stroke()
                    }

                    // HR line
                    ctx.strokeStyle = "#FF5252"
                    ctx.lineWidth = 2
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    var first = true, n = hrData.length - 1
                    for (var j = 0; j < hrData.length; j++) {
                        if (hrData[j].bpm <= 0) continue
                        var x = (j / Math.max(1, n)) * w
                        var y = h - ((hrData[j].bpm - minBpm) / range) * h
                        y = Math.max(0, Math.min(h, y))
                        if (first) { ctx.moveTo(x, y); first = false } else { ctx.lineTo(x, y) }
                    }
                    if (!first) {
                        ctx.stroke()
                        ctx.lineTo(w, h); ctx.lineTo(0, h); ctx.closePath()
                        ctx.fillStyle = "#FF5252"; ctx.globalAlpha = 0.15; ctx.fill()
                    }
                }
            }
        }

        // ── Stats row ───────────────────────────────────────────────────
        Row {
            width: parent.width
            height: Dims.h(12)
            spacing: Dims.w(4)

            Column {
                width: (parent.width - parent.spacing) / 2
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: {
                        var d = WorkoutManager.hrHistory
                        if (!d || d.length === 0) return "--"
                        var sum = 0, cnt = 0
                        for (var i = 0; i < d.length; i++) {
                            if (d[i].bpm > 0) { sum += d[i].bpm; cnt++ }
                        }
                        return cnt > 0 ? Math.round(sum / cnt).toString() : "--"
                    }
                    font.pixelSize: Dims.l(7)
                    font.bold: true
                    color: "#4CAF50"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "Avg HR"
                    text: qsTrId("id-avg-hr")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                }
            }

            Column {
                width: (parent.width - parent.spacing) / 2
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: {
                        var d = WorkoutManager.hrHistory
                        if (!d || d.length === 0) return "--"
                        var mx = 0
                        for (var i = 0; i < d.length; i++) {
                            mx = Math.max(mx, d[i].bpm)
                        }
                        return mx > 0 ? mx.toString() : "--"
                    }
                    font.pixelSize: Dims.l(7)
                    font.bold: true
                    color: "#F44336"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "Max HR"
                    text: qsTrId("id-max-hr")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                }
            }
        }
    }
}
