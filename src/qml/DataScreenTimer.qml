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
        
        function pad(n) {
            return n < 10 ? "0" + n : "" + n
        }
        
        if (h > 0) {
            return pad(h) + ":" + pad(m) + ":" + pad(s)
        }
        return pad(m) + ":" + pad(s)
    }

    function getWorkoutIcon(type) {
        var types = WorkoutController.workoutTypes()
        for (var i = 0; i < types.length; i++) {
            if (types[i].id === type) {
                return types[i].icon
            }
        }
        return "ios-fitness-outline"
    }

    function getWorkoutName(type) {
        var types = WorkoutController.workoutTypes()
        for (var i = 0; i < types.length; i++) {
            if (types[i].id === type) {
                return types[i].name
            }
        }
        return type
    }

    // Background
    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    // Circular layout
    Item {
        id: circularLayout
        anchors.fill: parent

        // Workout type icon at top
        Column {
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: parent.top
                topMargin: Dims.h(8)
            }
            spacing: Dims.h(1)

            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.w(12)
                height: width
                name: getWorkoutIcon(WorkoutController.workoutType)
                color: "#4CAF50"
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: getWorkoutName(WorkoutController.workoutType)
                font.pixelSize: Dims.l(4)
                color: "#4CAF50"
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // Large elapsed time in center
        Column {
            anchors.centerIn: parent
            spacing: Dims.h(0.5)

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: formatTime(WorkoutController.elapsedSeconds)
                font.pixelSize: Dims.l(18)
                font.bold: true
                color: "white"
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Elapsed Time"
                text: qsTrId("id-elapsed-time")
                font.pixelSize: Dims.l(3)
                color: "#80FFFFFF"
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // Bottom metrics row
        Row {
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: Dims.h(25)
            }
            spacing: Dims.w(8)

            // Calories (left)
            Column {
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Math.floor(WorkoutController.elapsedSeconds / 60 * 8.5).toString()
                    font.pixelSize: Dims.l(8)
                    font.bold: true
                    color: "#FF9800"
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "Cal"
                    text: qsTrId("id-calories")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // Distance (right)
            Column {
                spacing: Dims.h(0.5)

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: (WorkoutController.elapsedSeconds / 600).toFixed(2)
                    font.pixelSize: Dims.l(8)
                    font.bold: true
                    color: "#2196F3"
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "km"
                    text: qsTrId("id-distance-km")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // Pace at bottom center
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
                    var distance = WorkoutController.elapsedSeconds / 600
                    if (distance > 0) {
                        var paceMinPerKm = WorkoutController.elapsedSeconds / 60 / distance
                        var paceMin = Math.floor(paceMinPerKm)
                        var paceSec = Math.floor((paceMinPerKm - paceMin) * 60)
                        return paceMin + ":" + (paceSec < 10 ? "0" : "") + paceSec
                    }
                    return "--:--"
                }
                font.pixelSize: Dims.l(6)
                font.bold: true
                color: "#9C27B0"
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "min/km"
                text: qsTrId("id-pace")
                font.pixelSize: Dims.l(3)
                color: "#80FFFFFF"
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // Active profile indicator (subtle, top-right corner)
        Label {
            anchors {
                right: parent.right
                top: parent.top
                margins: Dims.w(2)
            }
            text: WorkoutController.activeProfileId
            font.pixelSize: Dims.l(2.5)
            color: "#40FFFFFF"
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideMiddle
            width: Dims.w(30)
        }
    }
}
