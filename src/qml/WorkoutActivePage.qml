import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.asteroid.workout 1.0

Item {
    id: workoutActivePage

    property string workoutType: WorkoutController.workoutType
    property bool isPaused: false

    // Keep screen on during workout (use MCE if available)
    Component.onCompleted: {
        // Request display stay on - MCE D-Bus would go here
        // For now, we rely on the system's default behavior during active use
    }

    Component.onDestruction: {
        // Release display lock
    }

    // Main content: swipeable data screens
    ListView {
        id: dataScreensView
        anchors.fill: parent
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        clip: true
        interactive: true
        currentIndex: 0

        model: ListModel {
            ListElement { screenType: "timer" }
            ListElement { screenType: "hr" }
        }

        delegate: Item {
            width: dataScreensView.width
            height: dataScreensView.height

            Loader {
                anchors.fill: parent
                source: {
                    if (screenType === "timer") {
                        return "DataScreenTimer.qml"
                    } else if (screenType === "hr") {
                        return "DataScreenHR.qml"
                    }
                    return ""
                }
            }
        }
    }

    // Page indicator for data screens
    Row {
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top
            topMargin: Dims.h(3)
        }
        spacing: Dims.w(2)

        Repeater {
            model: dataScreensView.count

            Rectangle {
                width: Dims.w(2)
                height: width
                radius: width / 2
                color: index === dataScreensView.currentIndex ? "white" : "#80FFFFFF"

                Behavior on color {
                    ColorAnimation { duration: 200 }
                }
            }
        }
    }

    // Control buttons overlay at bottom
    Item {
        id: controlsOverlay
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: Dims.h(20)

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.6
        }

        Row {
            anchors.centerIn: parent
            spacing: Dims.w(8)

            // Pause/Resume button
            Column {
                spacing: Dims.h(1)

                IconButton {
                    id: pauseButton
                    anchors.horizontalCenter: parent.horizontalCenter
                    iconName: isPaused ? "ios-play-outline" : "ios-pause-outline"
                    iconColor: "white"

                    onClicked: {
                        isPaused = !isPaused
                        // In a real implementation, this would pause sensor logging
                    }
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: isPaused ? 
                        //% "Resume"
                        qsTrId("id-resume") : 
                        //% "Pause"
                        qsTrId("id-pause")
                    font.pixelSize: Dims.l(3)
                    color: "white"
                }
            }

            // Stop button
            Column {
                spacing: Dims.h(1)

                IconButton {
                    id: stopButton
                    anchors.horizontalCenter: parent.horizontalCenter
                    iconName: "ios-square-outline"
                    iconColor: "#FF5252"

                    onClicked: {
                        stopRemorse.start()
                    }
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //% "Stop"
                    text: qsTrId("id-stop")
                    font.pixelSize: Dims.l(3)
                    color: "#FF5252"
                }
            }
        }
    }

    RemorseTimer {
        id: stopRemorse
        //% "Stopping..."
        action: qsTrId("id-stopping")
        onTriggered: {
            isPaused = false
            WorkoutController.stopWorkout()
        }
    }

    // Long press anywhere to open settings
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        z: -1

        onPressAndHold: {
            settingsRequested()
        }

        onPressed: mouse.accepted = false
        onReleased: mouse.accepted = false
        onClicked: mouse.accepted = false
    }

    signal settingsRequested()

    // Handle settings request by switching to settings page
    onSettingsRequested: {
        // Find parent SwipeView
        var swipeView = parent
        while (swipeView && !swipeView.hasOwnProperty("currentIndex")) {
            swipeView = swipeView.parent
        }
        if (swipeView) {
            swipeView.currentIndex = 2
        }
    }
}
