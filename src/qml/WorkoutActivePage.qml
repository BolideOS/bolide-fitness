import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: workoutActivePage

    property bool isPaused: WorkoutManager.paused
    property var dataScreens: WorkoutManager.dataScreensForWorkout(WorkoutManager.workoutType)

    // ── Swipeable data screens (vertical scroll) ─────────────────────────
    PathView {
        id: dataScreensView
        anchors.fill: parent
        snapMode: PathView.SnapOneItem
        highlightRangeMode: PathView.StrictlyEnforceRange
        preferredHighlightBegin: 0.5
        preferredHighlightEnd: 0.5
        flickDeceleration: 3000
        pathItemCount: 3
        interactive: true
        currentIndex: 0

        path: Path {
            startX: workoutActivePage.width / 2
            startY: -workoutActivePage.height * 0.5

            PathLine {
                x: workoutActivePage.width / 2
                y: workoutActivePage.height * 1.5
            }
        }

        model: ListModel {
            id: screenModel
            Component.onCompleted: {
                // Always have at least timer + HR
                append({ screenFile: "DataScreenTimer.qml" })
                append({ screenFile: "DataScreenHR.qml" })

                // Add GPS screen if workout uses GPS
                var wt = WorkoutManager.workoutType
                if (wt === "outdoor_run" || wt === "outdoor_walk" ||
                    wt === "hike" || wt === "ultra_hike" || wt === "cycling") {
                    append({ screenFile: "DataScreenGPS.qml" })
                }

                // Add elevation screen for hikes
                if (wt === "hike" || wt === "ultra_hike") {
                    append({ screenFile: "DataScreenElevation.qml" })
                }
            }
        }

        delegate: Item {
            width: dataScreensView.width
            height: dataScreensView.height
            opacity: PathView.isCurrentItem ? 1.0 : 0.3
            scale: PathView.isCurrentItem ? 1.0 : 0.85

            Behavior on opacity { NumberAnimation { duration: 200 } }
            Behavior on scale { NumberAnimation { duration: 200 } }

            Loader {
                anchors.fill: parent
                source: screenFile
            }
        }
    }

    // ── Screen indicator dots (right side, vertical) ────────────────────
    Column {
        anchors {
            right: parent.right
            rightMargin: Dims.w(2)
            verticalCenter: parent.verticalCenter
        }
        spacing: Dims.h(1)
        z: 10

        Repeater {
            model: dataScreensView.count

            Rectangle {
                width: Dims.w(1.5)
                height: index === dataScreensView.currentIndex ? Dims.h(4) : Dims.h(2)
                radius: width / 2
                color: index === dataScreensView.currentIndex ? "white" : "#40FFFFFF"
                Behavior on height {
                    NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
                }
            }
        }
    }

    // ── Bottom controls overlay ─────────────────────────────────────────
    Item {
        id: controlsOverlay
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: Dims.h(20)
        z: 10

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.6
        }

        Row {
            anchors.centerIn: parent
            spacing: Dims.w(8)

            // Pause / Resume
            Column {
                spacing: Dims.h(1)

                IconButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    iconName: isPaused ? "ios-play-outline" : "ios-pause-outline"
                    iconColor: "white"
                    onClicked: {
                        if (isPaused) WorkoutManager.resumeWorkout()
                        else WorkoutManager.pauseWorkout()
                    }
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: isPaused ? qsTrId("id-resume") : qsTrId("id-pause")
                    font.pixelSize: Dims.l(3)
                    color: "white"
                }
            }

            // Stop
            Column {
                spacing: Dims.h(1)

                IconButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    iconName: "ios-square-outline"
                    iconColor: "#FF5252"
                    onClicked: stopRemorse.start()
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
        onTriggered: WorkoutManager.stopWorkout()
    }
}
