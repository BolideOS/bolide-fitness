import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import Nemo.Configuration 1.0
import org.bolide.fitness 1.0

Item {
    id: settingsPage

    Flickable {
        anchors.fill: parent
        contentHeight: settingsColumn.height + Dims.h(12)
        flickableDirection: Flickable.VerticalFlick
        clip: true

        Column {
            id: settingsColumn
            width: parent.width
            topPadding: Dims.h(12)
            bottomPadding: Dims.h(8)
            spacing: Dims.h(3)

            // ── Section: Power Profile ──────────────────────────────────
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Power Profile"
                text: qsTrId("id-power-profile")
                font.pixelSize: Dims.l(4)
                font.bold: true
                color: "#2196F3"
            }

            ConfigurationValue {
                id: cfgProfile
                key: "/org/bolide/fitness/active-profile"
                defaultValue: "balanced"
            }

            Repeater {
                model: [
                    { id: "low_power",  name: qsTrId("id-low-power"),  desc: qsTrId("id-low-power-desc"),  color: "#4CAF50" },
                    { id: "balanced",   name: qsTrId("id-balanced"),   desc: qsTrId("id-balanced-desc"),   color: "#2196F3" },
                    { id: "high_perf",  name: qsTrId("id-high-perf"),  desc: qsTrId("id-high-perf-desc"),  color: "#FF9800" },
                    { id: "ultra",      name: qsTrId("id-ultra"),      desc: qsTrId("id-ultra-desc"),      color: "#F44336" }
                ]

                delegate: Rectangle {
                    width: parent.width - Dims.w(8)
                    anchors.horizontalCenter: parent.horizontalCenter
                    height: Dims.h(10)
                    radius: Dims.l(2)
                    color: cfgProfile.value === modelData.id ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                    border.width: cfgProfile.value === modelData.id ? 1 : 0
                    border.color: modelData.color

                    Row {
                        anchors.fill: parent
                        anchors.margins: Dims.w(3)
                        spacing: Dims.w(2)

                        Rectangle {
                            width: Dims.l(2)
                            height: Dims.l(2)
                            radius: width / 2
                            color: modelData.color
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter

                            Label {
                                text: modelData.name
                                font.pixelSize: Dims.l(3.5)
                                font.bold: true
                                color: "white"
                            }

                            Label {
                                text: modelData.desc
                                font.pixelSize: Dims.l(2.5)
                                color: "#80FFFFFF"
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            cfgProfile.value = modelData.id
                            WorkoutManager.activeProfileId = modelData.id
                        }
                    }
                }
            }

            // ── Section: HR Zones ───────────────────────────────────────
            Item { width: 1; height: Dims.h(4) } /* spacer */

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Heart-Rate Zones"
                text: qsTrId("id-hr-zones")
                font.pixelSize: Dims.l(4)
                font.bold: true
                color: "#F44336"
            }

            ConfigurationValue {
                id: cfgZoneCount
                key: "/org/bolide/fitness/hr-zone-count"
                defaultValue: 5
            }

            ConfigurationValue {
                id: cfgZone1
                key: "/org/bolide/fitness/hr-zone-1"
                defaultValue: 100
            }
            ConfigurationValue {
                id: cfgZone2
                key: "/org/bolide/fitness/hr-zone-2"
                defaultValue: 120
            }
            ConfigurationValue {
                id: cfgZone3
                key: "/org/bolide/fitness/hr-zone-3"
                defaultValue: 140
            }
            ConfigurationValue {
                id: cfgZone4
                key: "/org/bolide/fitness/hr-zone-4"
                defaultValue: 160
            }
            ConfigurationValue {
                id: cfgZone5
                key: "/org/bolide/fitness/hr-zone-5"
                defaultValue: 180
            }

            /* Zone threshold list */
            Repeater {
                model: cfgZoneCount.value

                delegate: Row {
                    width: parent.width - Dims.w(8)
                    anchors.horizontalCenter: parent.horizontalCenter
                    height: Dims.h(8)
                    spacing: Dims.w(2)

                    property var zoneColors: ["#4CAF50", "#8BC34A", "#FFC107", "#FF9800", "#F44336"]
                    property var zoneCfgs: [cfgZone1, cfgZone2, cfgZone3, cfgZone4, cfgZone5]
                    property var zoneNames: [
                        qsTrId("id-zone-warmup"), qsTrId("id-zone-easy"),
                        qsTrId("id-zone-aerobic"), qsTrId("id-zone-threshold"),
                        qsTrId("id-zone-max")
                    ]

                    Rectangle {
                        width: Dims.l(1.5)
                        height: parent.height * 0.6
                        radius: width / 2
                        color: zoneColors[index] || "#888"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Label {
                        width: Dims.w(30)
                        text: zoneNames[index] || ("Zone " + (index + 1))
                        font.pixelSize: Dims.l(3)
                        color: "white"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Label {
                        text: (zoneCfgs[index] ? zoneCfgs[index].value : "--") + " bpm"
                        font.pixelSize: Dims.l(3)
                        color: "#80FFFFFF"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // ── Section: GPS ────────────────────────────────────────────
            Item { width: 1; height: Dims.h(4) }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "GPS"
                text: qsTrId("id-gps-section")
                font.pixelSize: Dims.l(4)
                font.bold: true
                color: "#2196F3"
            }

            ConfigurationValue {
                id: cfgGpsEnabled
                key: "/org/bolide/fitness/gps-enabled"
                defaultValue: true
            }

            Row {
                width: parent.width - Dims.w(8)
                anchors.horizontalCenter: parent.horizontalCenter
                height: Dims.h(8)

                Label {
                    //% "Enable GPS"
                    text: qsTrId("id-enable-gps")
                    font.pixelSize: Dims.l(3.5)
                    color: "white"
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - toggleGps.width
                }

                Rectangle {
                    id: toggleGps
                    width: Dims.w(12)
                    height: Dims.h(6)
                    radius: height / 2
                    color: cfgGpsEnabled.value ? "#2196F3" : "#444"
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        width: parent.height - 4
                        height: width
                        radius: width / 2
                        color: "white"
                        anchors.verticalCenter: parent.verticalCenter
                        x: cfgGpsEnabled.value ? parent.width - width - 2 : 2

                        Behavior on x { NumberAnimation { duration: 150 } }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: cfgGpsEnabled.value = !cfgGpsEnabled.value
                    }
                }
            }

            // ── Section: Barometer ──────────────────────────────────────
            ConfigurationValue {
                id: cfgBaroEnabled
                key: "/org/bolide/fitness/baro-enabled"
                defaultValue: true
            }

            Row {
                width: parent.width - Dims.w(8)
                anchors.horizontalCenter: parent.horizontalCenter
                height: Dims.h(8)

                Label {
                    //% "Barometric Altimeter"
                    text: qsTrId("id-enable-baro")
                    font.pixelSize: Dims.l(3.5)
                    color: "white"
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - toggleBaro.width
                }

                Rectangle {
                    id: toggleBaro
                    width: Dims.w(12)
                    height: Dims.h(6)
                    radius: height / 2
                    color: cfgBaroEnabled.value ? "#4CAF50" : "#444"
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        width: parent.height - 4
                        height: width
                        radius: width / 2
                        color: "white"
                        anchors.verticalCenter: parent.verticalCenter
                        x: cfgBaroEnabled.value ? parent.width - width - 2 : 2

                        Behavior on x { NumberAnimation { duration: 150 } }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: cfgBaroEnabled.value = !cfgBaroEnabled.value
                    }
                }
            }

            /* Bottom padding for round screens */
            Item { width: 1; height: Dims.h(8) }
        }
    }
}
