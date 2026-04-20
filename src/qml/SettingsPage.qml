import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import Nemo.Configuration 1.0
import org.bolide.fitness 1.0

Item {
    id: settingsPage

    Flickable {
        anchors.fill: parent
        contentHeight: col.height + Dims.h(12)
        flickableDirection: Flickable.VerticalFlick
        clip: true

        Column {
            id: col
            width: parent.width
            topPadding: Dims.h(14)
            bottomPadding: Dims.h(10)
            spacing: Dims.h(3)

            // ── Title ───────────────────────────────────────────────────
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Settings"
                text: qsTrId("id-settings")
                font.pixelSize: Dims.l(5)
                font.bold: true
                color: "white"
            }

            // ── User Profile ────────────────────────────────────────────
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "User Profile"
                text: qsTrId("id-user-profile")
                font.pixelSize: Dims.l(3.5)
                font.bold: true
                color: "#2196F3"
            }

            ConfigurationValue { id: cfgAge;    key: "/org/bolide/fitness/user-age";    defaultValue: 30 }
            ConfigurationValue { id: cfgWeight; key: "/org/bolide/fitness/user-weight"; defaultValue: 70 }
            ConfigurationValue { id: cfgHeight; key: "/org/bolide/fitness/user-height"; defaultValue: 175 }
            ConfigurationValue { id: cfgGender; key: "/org/bolide/fitness/user-gender"; defaultValue: "male" }
            ConfigurationValue { id: cfgHRMax;  key: "/org/bolide/fitness/user-hrmax";  defaultValue: 190 }

            Column {
                width: parent.width - Dims.w(8)
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Dims.h(2)

                /* Age */
                Row {
                    width: parent.width; height: Dims.h(7)
                    Label { text: qsTrId("id-age"); font.pixelSize: Dims.l(3); color: "white"; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.5 }
                    Label { text: cfgAge.value.toString(); font.pixelSize: Dims.l(3.5); font.bold: true; color: "#80FFFFFF"; anchors.verticalCenter: parent.verticalCenter }
                }

                /* Weight */
                Row {
                    width: parent.width; height: Dims.h(7)
                    Label { text: qsTrId("id-weight"); font.pixelSize: Dims.l(3); color: "white"; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.5 }
                    Label { text: cfgWeight.value + " kg"; font.pixelSize: Dims.l(3.5); font.bold: true; color: "#80FFFFFF"; anchors.verticalCenter: parent.verticalCenter }
                }

                /* Height */
                Row {
                    width: parent.width; height: Dims.h(7)
                    Label { text: qsTrId("id-height"); font.pixelSize: Dims.l(3); color: "white"; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.5 }
                    Label { text: cfgHeight.value + " cm"; font.pixelSize: Dims.l(3.5); font.bold: true; color: "#80FFFFFF"; anchors.verticalCenter: parent.verticalCenter }
                }

                /* Gender toggle */
                Row {
                    width: parent.width; height: Dims.h(7)
                    Label { text: qsTrId("id-gender"); font.pixelSize: Dims.l(3); color: "white"; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.5 }
                    Label {
                        text: cfgGender.value === "male" ? qsTrId("id-male") : qsTrId("id-female")
                        font.pixelSize: Dims.l(3.5); font.bold: true; color: "#80FFFFFF"
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea {
                            anchors.fill: parent
                            onClicked: cfgGender.value = (cfgGender.value === "male" ? "female" : "male")
                        }
                    }
                }

                /* Max HR */
                Row {
                    width: parent.width; height: Dims.h(7)
                    Label { text: qsTrId("id-max-hr"); font.pixelSize: Dims.l(3); color: "white"; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.5 }
                    Label { text: cfgHRMax.value + " bpm"; font.pixelSize: Dims.l(3.5); font.bold: true; color: "#F44336"; anchors.verticalCenter: parent.verticalCenter }
                }
            }

            // ── Data ────────────────────────────────────────────────────
            Item { width: 1; height: Dims.h(2) }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Data"
                text: qsTrId("id-data")
                font.pixelSize: Dims.l(3.5)
                font.bold: true
                color: "#4CAF50"
            }

            ConfigurationValue { id: cfgAutoExport; key: "/org/bolide/fitness/auto-export"; defaultValue: false }

            Row {
                width: parent.width - Dims.w(8)
                anchors.horizontalCenter: parent.horizontalCenter
                height: Dims.h(8)

                Label {
                    //% "Auto-export workouts"
                    text: qsTrId("id-auto-export")
                    font.pixelSize: Dims.l(3)
                    color: "white"
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - toggleExport.width
                }

                Rectangle {
                    id: toggleExport
                    width: Dims.w(12); height: Dims.h(6)
                    radius: height / 2
                    color: cfgAutoExport.value ? "#4CAF50" : "#444"
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        width: parent.height - 4; height: width; radius: width / 2
                        color: "white"; anchors.verticalCenter: parent.verticalCenter
                        x: cfgAutoExport.value ? parent.width - width - 2 : 2
                        Behavior on x { NumberAnimation { duration: 150 } }
                    }

                    MouseArea { anchors.fill: parent; onClicked: cfgAutoExport.value = !cfgAutoExport.value }
                }
            }

            // ── About ───────────────────────────────────────────────────
            Item { width: 1; height: Dims.h(2) }

            Column {
                width: parent.width
                spacing: Dims.h(1)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Bolide Fitness"
                    font.pixelSize: Dims.l(3)
                    color: "#60FFFFFF"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "v1.0.0 • GPLv3"
                    font.pixelSize: Dims.l(2.5)
                    color: "#40FFFFFF"
                }
            }

            Item { width: 1; height: Dims.h(10) }
        }
    }
}
