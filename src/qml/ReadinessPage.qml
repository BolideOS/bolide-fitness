import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: readinessPage

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
            spacing: Dims.h(3)

            // ── Header ──────────────────────────────────────────────────
            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.l(8)
                height: width
                name: "ios-pulse"
                color: readinessColor
            }

            property color readinessColor: {
                var s = ReadinessScore.score
                if (s >= 85) return "#4CAF50"
                if (s >= 70) return "#8BC34A"
                if (s >= 50) return "#FFC107"
                if (s >= 30) return "#FF9800"
                return "#F44336"
            }

            // ── Score display ───────────────────────────────────────────
            Item {
                width: parent.width
                height: Dims.h(25)

                Column {
                    anchors.centerIn: parent
                    spacing: Dims.h(0.5)

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: ReadinessScore.score > 0
                            ? ReadinessScore.score.toString()
                            : "--"
                        font.pixelSize: Dims.l(22)
                        font.bold: true
                        color: content.readinessColor
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: ReadinessScore.score > 0
                            ? ReadinessScore.label
                            : qsTrId("id-no-data")
                        font.pixelSize: Dims.l(3.5)
                        font.bold: true
                        color: content.readinessColor
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        //% "Readiness"
                        text: qsTrId("id-readiness")
                        font.pixelSize: Dims.l(2.5)
                        color: "#80FFFFFF"
                    }
                }
            }

            // ── Recompute button ────────────────────────────────────────
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.w(45)
                height: Dims.h(8)
                radius: height / 2
                color: Qt.rgba(1, 1, 1, 0.1)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.2)

                Label {
                    anchors.centerIn: parent
                    //% "Refresh"
                    text: qsTrId("id-refresh")
                    font.pixelSize: Dims.l(3)
                    color: "white"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: ReadinessScore.recompute()
                }
            }

            // ── Component breakdown ─────────────────────────────────────
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Breakdown"
                text: qsTrId("id-breakdown")
                font.pixelSize: Dims.l(3.5)
                font.bold: true
                color: "#90FFFFFF"
                visible: ReadinessScore.score > 0
            }

            Column {
                visible: ReadinessScore.score > 0
                width: parent.width - Dims.w(8)
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Dims.h(2)

                Repeater {
                    model: [
                        { key: "hrv",            label: qsTrId("id-hrv"),          icon: "ios-pulse",        weight: 30 },
                        { key: "resting_hr",     label: qsTrId("id-resting-hr"),   icon: "ios-heart",        weight: 20 },
                        { key: "sleep_quality",  label: qsTrId("id-sleep-qual"),   icon: "ios-moon",         weight: 25 },
                        { key: "sleep_duration", label: qsTrId("id-sleep-dur"),    icon: "ios-time",         weight: 10 },
                        { key: "training_load",  label: qsTrId("id-training"),     icon: "ios-fitness",      weight: 15 }
                    ]

                    delegate: Column {
                        width: parent.width
                        spacing: Dims.h(0.5)

                        property var comp: ReadinessScore.breakdown[modelData.key] || {}
                        property int compScore: comp.score || 0

                        Row {
                            width: parent.width
                            spacing: Dims.w(2)

                            Icon {
                                width: Dims.l(3)
                                height: width
                                name: modelData.icon
                                color: {
                                    if (compScore >= 80) return "#4CAF50"
                                    if (compScore >= 60) return "#FFC107"
                                    if (compScore >= 40) return "#FF9800"
                                    return "#F44336"
                                }
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Label {
                                text: modelData.label
                                font.pixelSize: Dims.l(2.8)
                                color: "white"
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - Dims.l(3) - Dims.w(12) - parent.spacing * 2
                            }

                            Label {
                                text: compScore + "/" + modelData.weight + "%"
                                font.pixelSize: Dims.l(2.8)
                                font.bold: true
                                color: "#80FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignRight
                                width: Dims.w(12)
                            }
                        }

                        // Progress bar
                        Rectangle {
                            width: parent.width
                            height: Dims.h(1)
                            radius: height / 2
                            color: Qt.rgba(1, 1, 1, 0.08)

                            Rectangle {
                                width: parent.width * (compScore / 100)
                                height: parent.height
                                radius: parent.radius
                                color: {
                                    if (compScore >= 80) return "#4CAF50"
                                    if (compScore >= 60) return "#FFC107"
                                    if (compScore >= 40) return "#FF9800"
                                    return "#F44336"
                                }
                            }
                        }
                    }
                }
            }

            // ── Guide text ──────────────────────────────────────────────
            Label {
                visible: ReadinessScore.score > 0
                width: Dims.w(70)
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    var s = ReadinessScore.score
                    if (s >= 85) return qsTrId("id-ready-excellent")
                    if (s >= 70) return qsTrId("id-ready-good")
                    if (s >= 50) return qsTrId("id-ready-moderate")
                    if (s >= 30) return qsTrId("id-ready-low")
                    return qsTrId("id-ready-verylow")
                }
                font.pixelSize: Dims.l(2.5)
                color: "#60FFFFFF"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            // ── No data ─────────────────────────────────────────────────
            Label {
                visible: ReadinessScore.score <= 0
                width: Dims.w(70)
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Track sleep to get your readiness score."
                text: qsTrId("id-readiness-no-data")
                font.pixelSize: Dims.l(3)
                color: "#60FFFFFF"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Item { width: 1; height: Dims.h(8) }
        }
    }
}
