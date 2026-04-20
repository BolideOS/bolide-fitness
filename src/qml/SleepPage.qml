import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: sleepPage

    Rectangle { anchors.fill: parent; color: "#000000" }

    Flickable {
        anchors.fill: parent
        contentHeight: content.height + Dims.h(14)
        flickableDirection: Flickable.VerticalFlick
        clip: true

        Column {
            id: content
            width: parent.width
            topPadding: Dims.h(12)
            bottomPadding: Dims.h(10)
            spacing: Dims.h(2)

            // ── Header ──────────────────────────────────────────────────
            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.l(8)
                height: width
                name: "ios-moon"
                color: "#9C27B0"
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Sleep"
                text: qsTrId("id-sleep")
                font.pixelSize: Dims.l(5)
                font.bold: true
                color: "white"
            }

            // ── Start/Stop button ───────────────────────────────────────
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.w(50)
                height: Dims.h(10)
                radius: height / 2
                color: SleepTracker.tracking ? "#F44336" : "#9C27B0"

                Label {
                    anchors.centerIn: parent
                    text: SleepTracker.tracking
                        ? qsTrId("id-stop-sleep")
                        : qsTrId("id-start-sleep")
                    font.pixelSize: Dims.l(3.5)
                    font.bold: true
                    color: "white"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (SleepTracker.tracking)
                            SleepTracker.stopTracking()
                        else
                            SleepTracker.startTracking()
                    }
                }
            }

            // ── Tracking indicator ──────────────────────────────────────
            Label {
                visible: SleepTracker.tracking
                anchors.horizontalCenter: parent.horizontalCenter
                text: "● " + qsTrId("id-tracking-sleep")
                font.pixelSize: Dims.l(3)
                color: "#9C27B0"
            }

            // ── Last night summary ──────────────────────────────────────
            Item { width: 1; height: Dims.h(2) }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Last Night"
                text: qsTrId("id-last-night")
                font.pixelSize: Dims.l(3.5)
                font.bold: true
                color: "#CE93D8"
                visible: SleepTracker.totalMinutes > 0
            }

            // Quality score arc
            Item {
                visible: SleepTracker.totalMinutes > 0
                width: parent.width
                height: Dims.h(20)

                Column {
                    anchors.centerIn: parent
                    spacing: Dims.h(0.5)

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: Math.round(SleepTracker.qualityScore).toString()
                        font.pixelSize: Dims.l(16)
                        font.bold: true
                        color: {
                            var q = SleepTracker.qualityScore
                            if (q >= 80) return "#4CAF50"
                            if (q >= 60) return "#FFC107"
                            if (q >= 40) return "#FF9800"
                            return "#F44336"
                        }
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        //% "quality"
                        text: qsTrId("id-quality")
                        font.pixelSize: Dims.l(2.5)
                        color: "#80FFFFFF"
                    }
                }
            }

            // ── Stage breakdown ─────────────────────────────────────────
            Row {
                visible: SleepTracker.totalMinutes > 0
                width: parent.width - Dims.w(8)
                anchors.horizontalCenter: parent.horizontalCenter
                height: Dims.h(8)
                spacing: 0

                // Stacked bar showing sleep stages proportionally
                Rectangle {
                    height: parent.height
                    width: parent.width *
                        (SleepTracker.deepMinutes / Math.max(1, SleepTracker.totalMinutes))
                    color: "#3F51B5"
                    radius: 2
                }
                Rectangle {
                    height: parent.height
                    width: parent.width *
                        (SleepTracker.lightMinutes / Math.max(1, SleepTracker.totalMinutes))
                    color: "#7986CB"
                }
                Rectangle {
                    height: parent.height
                    width: parent.width *
                        (SleepTracker.remMinutes / Math.max(1, SleepTracker.totalMinutes))
                    color: "#9C27B0"
                }
                Rectangle {
                    height: parent.height
                    width: parent.width *
                        (SleepTracker.awakeMinutes / Math.max(1, SleepTracker.totalMinutes))
                    color: "#F44336"
                }
            }

            // Stage labels
            Grid {
                visible: SleepTracker.totalMinutes > 0
                anchors.horizontalCenter: parent.horizontalCenter
                columns: 2
                rowSpacing: Dims.h(1)
                columnSpacing: Dims.w(6)

                Repeater {
                    model: [
                        { label: qsTrId("id-deep"),    min: SleepTracker.deepMinutes,  color: "#3F51B5" },
                        { label: qsTrId("id-light"),   min: SleepTracker.lightMinutes, color: "#7986CB" },
                        { label: qsTrId("id-rem"),     min: SleepTracker.remMinutes,   color: "#9C27B0" },
                        { label: qsTrId("id-awake"),   min: SleepTracker.awakeMinutes, color: "#F44336" }
                    ]

                    delegate: Row {
                        spacing: Dims.w(1.5)

                        Rectangle {
                            width: Dims.l(2)
                            height: Dims.l(2)
                            radius: width / 2
                            color: modelData.color
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Label {
                            text: modelData.label + ": "
                            font.pixelSize: Dims.l(2.5)
                            color: "#80FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Label {
                            text: {
                                var m = modelData.min
                                var h = Math.floor(m / 60)
                                var mm = m % 60
                                return h > 0 ? h + "h " + mm + "m" : mm + "m"
                            }
                            font.pixelSize: Dims.l(2.5)
                            font.bold: true
                            color: "white"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // ── Duration & HRV ──────────────────────────────────────────
            Row {
                visible: SleepTracker.totalMinutes > 0
                width: parent.width - Dims.w(8)
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Dims.w(4)

                Column {
                    width: (parent.width - parent.spacing) / 2
                    spacing: Dims.h(0.5)

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: {
                            var t = SleepTracker.totalMinutes
                            return Math.floor(t / 60) + "h " + (t % 60) + "m"
                        }
                        font.pixelSize: Dims.l(6)
                        font.bold: true
                        color: "#CE93D8"
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        //% "total sleep"
                        text: qsTrId("id-total-sleep")
                        font.pixelSize: Dims.l(2.5)
                        color: "#80FFFFFF"
                    }
                }

                Column {
                    width: (parent.width - parent.spacing) / 2
                    spacing: Dims.h(0.5)

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: SleepTracker.overnightRestingHr > 0
                            ? SleepTracker.overnightRestingHr.toString()
                            : "--"
                        font.pixelSize: Dims.l(6)
                        font.bold: true
                        color: "#F44336"
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        //% "resting HR"
                        text: qsTrId("id-resting-hr")
                        font.pixelSize: Dims.l(2.5)
                        color: "#80FFFFFF"
                    }
                }
            }

            // ── Sleep debt ──────────────────────────────────────────────
            Column {
                visible: SleepTracker.sleepDebtMinutes > 0
                width: parent.width
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: {
                        var d = SleepTracker.sleepDebtMinutes
                        return Math.floor(d / 60) + "h " + (d % 60) + "m"
                    }
                    font.pixelSize: Dims.l(5)
                    font.bold: true
                    color: "#FF9800"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "sleep debt (7 days)"
                    text: qsTrId("id-sleep-debt")
                    font.pixelSize: Dims.l(2.5)
                    color: "#80FFFFFF"
                }
            }

            // ── No data message ─────────────────────────────────────────
            Label {
                visible: SleepTracker.totalMinutes <= 0 && !SleepTracker.tracking
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.w(70)
                //% "No sleep data yet. Start tracking before bed."
                text: qsTrId("id-no-sleep-data")
                font.pixelSize: Dims.l(3)
                color: "#60FFFFFF"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Item { width: 1; height: Dims.h(8) }
        }
    }
}
