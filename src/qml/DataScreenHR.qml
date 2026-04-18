import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: hrScreen

    function getHRZone(bpm) {
        if (bpm < 100) return { name: qsTrId("id-zone-warmup"), color: "#9E9E9E" }
        if (bpm < 120) return { name: qsTrId("id-zone-fat-burn"), color: "#4CAF50" }
        if (bpm < 140) return { name: qsTrId("id-zone-cardio"), color: "#FF9800" }
        if (bpm < 160) return { name: qsTrId("id-zone-peak"), color: "#F44336" }
        return { name: qsTrId("id-zone-maximum"), color: "#C62828" }
    }

    property var currentZone: getHRZone(WorkoutController.heartRate)

    // Background
    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    Column {
        anchors.fill: parent
        anchors.margins: Dims.w(4)
        spacing: Dims.h(2)

        // Top section: Current HR
        Item {
            width: parent.width
            height: Dims.h(30)

            Column {
                anchors.centerIn: parent
                spacing: Dims.h(1)

                // Large HR number
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Dims.w(2)

                    Label {
                        text: WorkoutController.heartRate > 0 ? WorkoutController.heartRate.toString() : "--"
                        font.pixelSize: Dims.l(24)
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

                // HR zone indicator
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Dims.w(40)
                    height: Dims.h(6)
                    radius: height / 2
                    color: currentZone.color

                    Label {
                        anchors.centerIn: parent
                        text: currentZone.name
                        font.pixelSize: Dims.l(3.5)
                        font.bold: true
                        color: "white"
                    }
                }

                // Heart icon
                Icon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Dims.w(8)
                    height: width
                    name: "ios-heart"
                    color: currentZone.color
                    opacity: 0.8
                }
            }
        }

        // HR Graph
        Item {
            width: parent.width
            height: Dims.h(45)

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

                property var hrData: WorkoutController.hrHistory

                onHrDataChanged: {
                    requestPaint()
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()

                    // Handle empty data case
                    if (!hrData || hrData.length === 0) {
                        ctx.fillStyle = "#80FFFFFF"
                        ctx.font = "12px sans-serif"
                        ctx.textAlign = "center"
                        ctx.fillText("No HR data yet", width / 2, height / 2)
                        return
                    }

                    if (hrData.length < 2) {
                        return
                    }

                    // Calculate min/max for auto-scaling
                    var minBpm = 999
                    var maxBpm = 0
                    var hasValidData = false
                    
                    for (var i = 0; i < hrData.length; i++) {
                        var entry = hrData[i]
                        if (entry && entry.bpm && entry.bpm > 0) {
                            minBpm = Math.min(minBpm, entry.bpm)
                            maxBpm = Math.max(maxBpm, entry.bpm)
                            hasValidData = true
                        }
                    }

                    if (!hasValidData) {
                        return
                    }

                    // Add padding to Y axis
                    var padding = 10
                    minBpm = Math.max(40, minBpm - padding)
                    maxBpm = Math.min(200, maxBpm + padding)
                    var bpmRange = maxBpm - minBpm
                    
                    // Ensure minimum range to avoid division by zero
                    if (bpmRange < 20) {
                        bpmRange = 20
                        var midBpm = (minBpm + maxBpm) / 2
                        minBpm = Math.max(40, midBpm - 10)
                        maxBpm = minBpm + 20
                    }

                    var w = width
                    var h = height

                    // Draw zone backgrounds
                    function drawZoneRect(zoneMinBpm, zoneMaxBpm, color) {
                        var y1 = h - ((zoneMaxBpm - minBpm) / bpmRange) * h
                        var y2 = h - ((zoneMinBpm - minBpm) / bpmRange) * h
                        ctx.fillStyle = color
                        ctx.globalAlpha = 0.1
                        ctx.fillRect(0, y1, w, y2 - y1)
                        ctx.globalAlpha = 1.0
                    }

                    drawZoneRect(40, 100, "#9E9E9E")
                    drawZoneRect(100, 120, "#4CAF50")
                    drawZoneRect(120, 140, "#FF9800")
                    drawZoneRect(140, 160, "#F44336")
                    drawZoneRect(160, 200, "#C62828")

                    // Draw grid lines
                    ctx.strokeStyle = "#333333"
                    ctx.lineWidth = 1
                    for (var bpm = Math.ceil(minBpm / 20) * 20; bpm <= maxBpm; bpm += 20) {
                        var y = h - ((bpm - minBpm) / bpmRange) * h
                        ctx.beginPath()
                        ctx.moveTo(0, y)
                        ctx.lineTo(w, y)
                        ctx.stroke()
                    }

                    // Draw HR line graph
                    ctx.strokeStyle = "#FF5252"
                    ctx.lineWidth = 2
                    ctx.lineJoin = "round"
                    ctx.lineCap = "round"

                    ctx.beginPath()
                    var firstPoint = true
                    var dataPointCount = Math.max(1, hrData.length - 1)
                    
                    for (var idx = 0; idx < hrData.length; idx++) {
                        var dataPoint = hrData[idx]
                        if (!dataPoint || !dataPoint.bpm || dataPoint.bpm <= 0) continue

                        var x = (idx / dataPointCount) * w
                        var yVal = h - ((dataPoint.bpm - minBpm) / bpmRange) * h

                        // Clamp to canvas bounds
                        yVal = Math.max(0, Math.min(h, yVal))

                        if (firstPoint) {
                            ctx.moveTo(x, yVal)
                            firstPoint = false
                        } else {
                            ctx.lineTo(x, yVal)
                        }
                    }
                    
                    if (!firstPoint) {
                        ctx.stroke()

                        // Fill area under line
                        ctx.lineTo(w, h)
                        ctx.lineTo(0, h)
                        ctx.closePath()
                        ctx.fillStyle = "#FF5252"
                        ctx.globalAlpha = 0.2
                        ctx.fill()
                        ctx.globalAlpha = 1.0
                    }
                }
            }

            // Y-axis labels
            Column {
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                    leftMargin: Dims.w(1)
                    topMargin: Dims.h(1)
                    bottomMargin: Dims.h(1)
                }
                spacing: 0

                Repeater {
                    model: 3

                    Label {
                        text: {
                            var hrData = WorkoutController.hrHistory
                            if (!hrData || hrData.length < 2) return ""
                            
                            var minBpm = 999
                            var maxBpm = 0
                            var hasValidData = false
                            
                            for (var i = 0; i < hrData.length; i++) {
                                if (hrData[i] && hrData[i].bpm && hrData[i].bpm > 0) {
                                    minBpm = Math.min(minBpm, hrData[i].bpm)
                                    maxBpm = Math.max(maxBpm, hrData[i].bpm)
                                    hasValidData = true
                                }
                            }
                            
                            if (!hasValidData) return ""
                            
                            var padding = 10
                            minBpm = Math.max(40, minBpm - padding)
                            maxBpm = Math.min(200, maxBpm + padding)
                            var bpmRange = maxBpm - minBpm
                            if (bpmRange < 20) {
                                bpmRange = 20
                                var midBpm = (minBpm + maxBpm) / 2
                                minBpm = Math.max(40, midBpm - 10)
                                maxBpm = minBpm + 20
                            }
                            
                            if (index === 0) return Math.round(maxBpm).toString()
                            if (index === 1) return Math.round((minBpm + maxBpm) / 2).toString()
                            return Math.round(minBpm).toString()
                        }
                        font.pixelSize: Dims.l(2.5)
                        color: "#80FFFFFF"
                    }
                }
            }

            // Time axis label
            Label {
                anchors {
                    bottom: parent.bottom
                    right: parent.right
                    margins: Dims.w(2)
                }
                //% "Last 30 min"
                text: qsTrId("id-hr-graph-timespan")
                font.pixelSize: Dims.l(2.5)
                color: "#80FFFFFF"
            }
        }

        // Bottom stats row
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
                        var hrData = WorkoutController.hrHistory
                        if (!hrData || hrData.length === 0) return "--"
                        var sum = 0
                        var count = 0
                        for (var i = 0; i < hrData.length; i++) {
                            if (hrData[i] && hrData[i].bpm && hrData[i].bpm > 0) {
                                sum += hrData[i].bpm
                                count++
                            }
                        }
                        return count > 0 ? Math.round(sum / count).toString() : "--"
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
                        var hrData = WorkoutController.hrHistory
                        if (!hrData || hrData.length === 0) return "--"
                        var maxHr = 0
                        for (var i = 0; i < hrData.length; i++) {
                            if (hrData[i] && hrData[i].bpm && hrData[i].bpm > 0) {
                                maxHr = Math.max(maxHr, hrData[i].bpm)
                            }
                        }
                        return maxHr > 0 ? maxHr.toString() : "--"
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
