import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: bodyPage

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
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Body Metrics"
                text: qsTrId("id-body-metrics")
                font.pixelSize: Dims.l(4.5)
                font.bold: true
                color: "#00BCD4"
            }

            // ── Monitoring toggle ───────────────────────────────────────
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.w(50)
                height: Dims.h(8)
                radius: height / 2
                color: BodyMetrics.monitoring ? "#F44336" : "#00BCD4"

                Label {
                    anchors.centerIn: parent
                    text: BodyMetrics.monitoring
                        ? qsTrId("id-stop-monitoring")
                        : qsTrId("id-start-monitoring")
                    font.pixelSize: Dims.l(3)
                    font.bold: true
                    color: "white"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (BodyMetrics.monitoring)
                            BodyMetrics.stopMonitoring()
                        else
                            BodyMetrics.startMonitoring()
                    }
                }
            }

            // ── Heart Rate ──────────────────────────────────────────────
            Item {
                width: parent.width
                height: Dims.h(16)

                Row {
                    anchors.centerIn: parent
                    spacing: Dims.w(3)

                    Icon {
                        width: Dims.l(5)
                        height: width
                        name: "ios-heart"
                        color: "#F44336"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter

                        Label {
                            text: BodyMetrics.heartRate > 0
                                ? BodyMetrics.heartRate.toString()
                                : "--"
                            font.pixelSize: Dims.l(12)
                            font.bold: true
                            color: "#F44336"
                        }
                        Label {
                            //% "bpm"
                            text: qsTrId("id-bpm")
                            font.pixelSize: Dims.l(2.5)
                            color: "#80FFFFFF"
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: BodyMetrics.restingHr > 0

                        Label {
                            text: BodyMetrics.restingHr.toString()
                            font.pixelSize: Dims.l(6)
                            font.bold: true
                            color: "#EF9A9A"
                        }
                        Label {
                            //% "resting"
                            text: qsTrId("id-resting")
                            font.pixelSize: Dims.l(2.5)
                            color: "#80FFFFFF"
                        }
                    }
                }
            }

            // ── Metric grid ─────────────────────────────────────────────
            Grid {
                anchors.horizontalCenter: parent.horizontalCenter
                columns: 2
                rowSpacing: Dims.h(2)
                columnSpacing: Dims.w(4)

                // SpO2
                Rectangle {
                    width: Dims.w(40)
                    height: Dims.h(14)
                    radius: Dims.l(2)
                    color: Qt.rgba(1, 1, 1, 0.06)

                    Column {
                        anchors.centerIn: parent
                        spacing: Dims.h(0.3)

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: BodyMetrics.spo2 > 0
                                ? BodyMetrics.spo2 + "%"
                                : "--"
                            font.pixelSize: Dims.l(8)
                            font.bold: true
                            color: {
                                var s = BodyMetrics.spo2
                                if (s >= 95) return "#4CAF50"
                                if (s >= 90) return "#FFC107"
                                return "#F44336"
                            }
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            //% "SpO₂"
                            text: qsTrId("id-spo2")
                            font.pixelSize: Dims.l(2.5)
                            color: "#80FFFFFF"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: BodyMetrics.spotCheckSpO2()
                    }

                    // Measuring indicator
                    Rectangle {
                        visible: BodyMetrics.spo2Measuring
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 4
                        width: Dims.l(2)
                        height: width
                        radius: width / 2
                        color: "#F44336"

                        SequentialAnimation on opacity {
                            running: BodyMetrics.spo2Measuring
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 500 }
                            NumberAnimation { to: 1.0; duration: 500 }
                        }
                    }
                }

                // Stress
                Rectangle {
                    width: Dims.w(40)
                    height: Dims.h(14)
                    radius: Dims.l(2)
                    color: Qt.rgba(1, 1, 1, 0.06)

                    Column {
                        anchors.centerIn: parent
                        spacing: Dims.h(0.3)

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: BodyMetrics.stressIndex > 0
                                ? BodyMetrics.stressIndex.toString()
                                : "--"
                            font.pixelSize: Dims.l(8)
                            font.bold: true
                            color: {
                                var s = BodyMetrics.stressIndex
                                if (s <= 25) return "#4CAF50"
                                if (s <= 50) return "#FFC107"
                                if (s <= 75) return "#FF9800"
                                return "#F44336"
                            }
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            //% "Stress"
                            text: qsTrId("id-stress")
                            font.pixelSize: Dims.l(2.5)
                            color: "#80FFFFFF"
                        }
                    }
                }

                // HRV
                Rectangle {
                    width: Dims.w(40)
                    height: Dims.h(14)
                    radius: Dims.l(2)
                    color: Qt.rgba(1, 1, 1, 0.06)

                    Column {
                        anchors.centerIn: parent
                        spacing: Dims.h(0.3)

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: BodyMetrics.hrvRmssd > 0
                                ? Math.round(BodyMetrics.hrvRmssd).toString()
                                : "--"
                            font.pixelSize: Dims.l(8)
                            font.bold: true
                            color: "#9C27B0"
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            //% "HRV ms"
                            text: qsTrId("id-hrv-ms")
                            font.pixelSize: Dims.l(2.5)
                            color: "#80FFFFFF"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: BodyMetrics.spotCheckHRV()
                    }
                }

                // Temperature
                Rectangle {
                    width: Dims.w(40)
                    height: Dims.h(14)
                    radius: Dims.l(2)
                    color: Qt.rgba(1, 1, 1, 0.06)

                    Column {
                        anchors.centerIn: parent
                        spacing: Dims.h(0.3)

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: BodyMetrics.temperature > 0
                                ? BodyMetrics.temperature.toFixed(1) + "°"
                                : "--"
                            font.pixelSize: Dims.l(8)
                            font.bold: true
                            color: "#FF9800"
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            //% "Temp °C"
                            text: qsTrId("id-temp")
                            font.pixelSize: Dims.l(2.5)
                            color: "#80FFFFFF"
                        }
                    }
                }

                // Breathing rate
                Rectangle {
                    width: Dims.w(40)
                    height: Dims.h(14)
                    radius: Dims.l(2)
                    color: Qt.rgba(1, 1, 1, 0.06)

                    Column {
                        anchors.centerIn: parent
                        spacing: Dims.h(0.3)

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: BodyMetrics.breathingRate > 0
                                ? BodyMetrics.breathingRate.toString()
                                : "--"
                            font.pixelSize: Dims.l(8)
                            font.bold: true
                            color: "#00BCD4"
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            //% "br/min"
                            text: qsTrId("id-breathing")
                            font.pixelSize: Dims.l(2.5)
                            color: "#80FFFFFF"
                        }
                    }
                }

                // Steps
                Rectangle {
                    width: Dims.w(40)
                    height: Dims.h(14)
                    radius: Dims.l(2)
                    color: Qt.rgba(1, 1, 1, 0.06)

                    Column {
                        anchors.centerIn: parent
                        spacing: Dims.h(0.3)

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: BodyMetrics.dailySteps > 0
                                ? (BodyMetrics.dailySteps / 1000).toFixed(1) + "k"
                                : "--"
                            font.pixelSize: Dims.l(8)
                            font.bold: true
                            color: "#8BC34A"
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            //% "Steps"
                            text: qsTrId("id-steps")
                            font.pixelSize: Dims.l(2.5)
                            color: "#80FFFFFF"
                        }
                    }
                }
            }

            // ── Tap hints ───────────────────────────────────────────────
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Tap SpO₂ or HRV to measure"
                text: qsTrId("id-tap-to-measure")
                font.pixelSize: Dims.l(2.5)
                color: "#40FFFFFF"
            }

            Item { width: 1; height: Dims.h(8) }
        }
    }
}
