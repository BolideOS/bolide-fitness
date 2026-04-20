import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: timerScreen

    function formatTime(seconds) {
        var h = Math.floor(seconds / 3600)
        var m = Math.floor((seconds % 3600) / 60)
        var s = seconds % 60
        function pad(n) { return n < 10 ? "0" + n : "" + n }
        return h > 0 ? pad(h) + ":" + pad(m) + ":" + pad(s) : pad(m) + ":" + pad(s)
    }

    Rectangle { anchors.fill: parent; color: "#000000" }

    Item {
        anchors.fill: parent

        // Workout type icon + name
        Column {
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: parent.top
                topMargin: Dims.h(8)
            }
            spacing: Dims.h(1)

            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.w(10)
                height: width
                name: WorkoutManager.workoutIcon
                color: "#4CAF50"
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: WorkoutManager.workoutName
                font.pixelSize: Dims.l(3.5)
                color: "#4CAF50"
            }
        }

        // Large elapsed time
        Column {
            anchors.centerIn: parent
            spacing: Dims.h(0.5)

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: formatTime(WorkoutManager.elapsedSeconds)
                font.pixelSize: Dims.l(18)
                font.bold: true
                color: "white"
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Elapsed Time"
                text: qsTrId("id-elapsed-time")
                font.pixelSize: Dims.l(3)
                color: "#80FFFFFF"
            }
        }

        // Bottom metrics
        Row {
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: Dims.h(25)
            }
            spacing: Dims.w(6)

            // Distance
            Column {
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: (WorkoutManager.distance / 1000).toFixed(2)
                    font.pixelSize: Dims.l(7)
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

            // Calories
            Column {
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: WorkoutManager.calories.toString()
                    font.pixelSize: Dims.l(7)
                    font.bold: true
                    color: "#FF9800"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "cal"
                    text: qsTrId("id-calories")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                }
            }

            // Steps
            Column {
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: WorkoutManager.steps.toString()
                    font.pixelSize: Dims.l(7)
                    font.bold: true
                    color: "#9C27B0"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "steps"
                    text: qsTrId("id-steps")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                }
            }
        }

        // Pace
        Column {
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: Dims.h(12)
            }
            spacing: Dims.h(0.5)

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    var pace = WorkoutManager.currentPace
                    if (pace > 0 && pace < 3600) {
                        var paceMin = Math.floor(pace / 60)
                        var paceSec = Math.floor(pace % 60)
                        return paceMin + ":" + (paceSec < 10 ? "0" : "") + paceSec
                    }
                    return "--:--"
                }
                font.pixelSize: Dims.l(6)
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

        // Profile indicator
        Label {
            anchors {
                right: parent.right
                top: parent.top
                margins: Dims.w(2)
            }
            text: WorkoutManager.activeProfileId
            font.pixelSize: Dims.l(2.5)
            color: "#40FFFFFF"
            elide: Text.ElideMiddle
            width: Dims.w(30)
            horizontalAlignment: Text.AlignRight
        }
    }
}
