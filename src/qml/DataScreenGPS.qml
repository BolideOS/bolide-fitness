import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: gpsScreen

    Rectangle { anchors.fill: parent; color: "#000000" }

    Column {
        anchors.fill: parent
        anchors.margins: Dims.w(4)
        spacing: Dims.h(2)

        // ── Header ──────────────────────────────────────────────────────
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            //% "GPS"
            text: qsTrId("id-gps-screen")
            font.pixelSize: Dims.l(4)
            color: "#2196F3"
            font.bold: true
        }

        // ── Speed ───────────────────────────────────────────────────────
        Item {
            width: parent.width
            height: Dims.h(25)

            Column {
                anchors.centerIn: parent
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: (WorkoutManager.currentSpeed * 3.6).toFixed(1)
                    font.pixelSize: Dims.l(20)
                    font.bold: true
                    color: "white"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "km/h"
                    text: qsTrId("id-speed-kmh")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                }
            }
        }

        // ── Distance + Elevation row ────────────────────────────────────
        Row {
            width: parent.width
            height: Dims.h(20)
            spacing: Dims.w(4)

            Column {
                width: (parent.width - parent.spacing) / 2
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: (WorkoutManager.distance / 1000).toFixed(2)
                    font.pixelSize: Dims.l(8)
                    font.bold: true
                    color: "#2196F3"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "km"
                    text: qsTrId("id-distance-km")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                }
            }

            Column {
                width: (parent.width - parent.spacing) / 2
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Math.round(WorkoutManager.elevationGain).toString()
                    font.pixelSize: Dims.l(8)
                    font.bold: true
                    color: "#4CAF50"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "m ↑"
                    text: qsTrId("id-elevation-gain")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                }
            }
        }

        // ── Coordinates ─────────────────────────────────────────────────
        Column {
            width: parent.width
            spacing: Dims.h(0.5)

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: WorkoutManager.latitude.toFixed(6) + "°, " + WorkoutManager.longitude.toFixed(6) + "°"
                font.pixelSize: Dims.l(3)
                color: "#60FFFFFF"
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Alt: " + Math.round(WorkoutManager.currentAltitude) + " m"
                font.pixelSize: Dims.l(3)
                color: "#60FFFFFF"
            }
        }

        // ── Pace ────────────────────────────────────────────────────────
        Row {
            width: parent.width
            height: Dims.h(15)
            spacing: Dims.w(4)

            Column {
                width: (parent.width - parent.spacing) / 2
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: {
                        var p = WorkoutManager.currentPace
                        if (p > 0 && p < 3600) {
                            var m = Math.floor(p / 60)
                            var s = Math.floor(p % 60)
                            return m + ":" + (s < 10 ? "0" : "") + s
                        }
                        return "--:--"
                    }
                    font.pixelSize: Dims.l(7)
                    font.bold: true
                    color: "#00BCD4"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "min/km"
                    text: qsTrId("id-pace")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                }
            }

            Column {
                width: (parent.width - parent.spacing) / 2
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: WorkoutManager.heartRate > 0 ? WorkoutManager.heartRate.toString() : "--"
                    font.pixelSize: Dims.l(7)
                    font.bold: true
                    color: "#F44336"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "bpm"
                    text: qsTrId("id-bpm")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                }
            }
        }
    }
}
