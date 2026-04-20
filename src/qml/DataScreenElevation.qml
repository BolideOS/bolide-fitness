import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: elevScreen

    Rectangle { anchors.fill: parent; color: "#000000" }

    Column {
        anchors.fill: parent
        anchors.margins: Dims.w(4)
        spacing: Dims.h(2)

        // ── Header ──────────────────────────────────────────────────────
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            //% "Elevation"
            text: qsTrId("id-elevation-screen")
            font.pixelSize: Dims.l(4)
            color: "#4CAF50"
            font.bold: true
        }

        // ── Current altitude ────────────────────────────────────────────
        Item {
            width: parent.width
            height: Dims.h(20)

            Column {
                anchors.centerIn: parent
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Math.round(WorkoutManager.currentAltitude).toString()
                    font.pixelSize: Dims.l(18)
                    font.bold: true
                    color: "white"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "m altitude"
                    text: qsTrId("id-altitude-m")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                }
            }
        }

        // ── Elevation gain + grade row ──────────────────────────────────
        Row {
            width: parent.width
            height: Dims.h(16)
            spacing: Dims.w(4)

            Column {
                width: (parent.width - parent.spacing) / 2
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Math.round(WorkoutManager.elevationGain).toString()
                    font.pixelSize: Dims.l(9)
                    font.bold: true
                    color: "#4CAF50"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "m gained"
                    text: qsTrId("id-gain")
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
                        /* Compute approximate grade from recent elevation change
                           and horizontal distance */
                        var d = WorkoutManager.distance
                        var g = WorkoutManager.elevationGain
                        if (d > 50) {
                            var grade = (g / d) * 100
                            return grade.toFixed(1) + "%"
                        }
                        return "--%"
                    }
                    font.pixelSize: Dims.l(9)
                    font.bold: true
                    color: "#FFC107"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "avg grade"
                    text: qsTrId("id-avg-grade")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                }
            }
        }

        // ── Elevation profile sparkline ─────────────────────────────────
        Canvas {
            id: elevCanvas
            width: parent.width
            height: Dims.h(25)

            property var elevHistory: WorkoutManager.elevationHistory
            property real minElev: 0
            property real maxElev: 100
            property real range: 0

            onElevHistoryChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                var data = elevHistory
                if (!data || data.length < 2) return

                /* compute bounds */
                var minV = data[0], maxV = data[0]
                for (var i = 1; i < data.length; i++) {
                    if (data[i] < minV) minV = data[i]
                    if (data[i] > maxV) maxV = data[i]
                }
                if (maxV - minV < 10) {
                    minV -= 5
                    maxV += 5
                }
                minElev = minV
                maxElev = maxV
                range = maxV - minV

                /* fill area under curve */
                var xStep = width / (data.length - 1)
                ctx.beginPath()
                ctx.moveTo(0, height)
                for (var j = 0; j < data.length; j++) {
                    var x = j * xStep
                    var y = height - ((data[j] - minV) / range) * height * 0.9
                    ctx.lineTo(x, y)
                }
                ctx.lineTo(width, height)
                ctx.closePath()

                var gradient = ctx.createLinearGradient(0, 0, 0, height)
                gradient.addColorStop(0, "#604CAF50")
                gradient.addColorStop(1, "#104CAF50")
                ctx.fillStyle = gradient
                ctx.fill()

                /* stroke outline */
                ctx.beginPath()
                for (var k = 0; k < data.length; k++) {
                    var px = k * xStep
                    var py = height - ((data[k] - minV) / range) * height * 0.9
                    if (k === 0) ctx.moveTo(px, py)
                    else ctx.lineTo(px, py)
                }
                ctx.lineWidth = 1.5
                ctx.strokeStyle = "#4CAF50"
                ctx.stroke()
            }
        }

        /* Min/max labels under graph */
        Row {
            width: parent.width

            Label {
                width: parent.width / 2
                text: Math.round(elevCanvas.minElev) + " m"
                font.pixelSize: Dims.l(2.5)
                color: "#60FFFFFF"
                horizontalAlignment: Text.AlignLeft
            }

            Label {
                width: parent.width / 2
                text: Math.round(elevCanvas.maxElev) + " m"
                font.pixelSize: Dims.l(2.5)
                color: "#60FFFFFF"
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
