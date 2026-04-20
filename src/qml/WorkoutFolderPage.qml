/*
 * WorkoutFolderPage.qml – Level 1 glance for the Workout folder.
 *
 * Fenix 8 style layout:
 *   - Row of 3-5 pinned workout icons (favorites)
 *   - Divider line
 *   - "All Workouts" button
 *
 * Tapping a pinned workout → WorkoutStartPage
 * Tapping "All Workouts"  → AllWorkoutsPage
 */
import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: root

    property var folderData: parent ? parent.folderData : ({})
    property var pinnedIds: ScreenRegistry.pinnedWorkouts
    property var allTypes: WorkoutManager.workoutTypes

    // Resolve pinned ids to workout type objects
    property var pinnedTypes: {
        var result = []
        for (var i = 0; i < pinnedIds.length; i++) {
            for (var j = 0; j < allTypes.length; j++) {
                if (allTypes[j].id === pinnedIds[i]) {
                    result.push(allTypes[j])
                    break
                }
            }
        }
        return result
    }

    Rectangle { anchors.fill: parent; color: "transparent" }

    Column {
        anchors.centerIn: parent
        width: parent.width * 0.9
        spacing: Dims.h(3)

        // ── Folder title ────────────────────────────────────────────────
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            //% "Workouts"
            text: qsTrId("id-workouts")
            font.pixelSize: Dims.l(5)
            font.bold: true
            color: "white"
        }

        // ── Pinned workout icons ────────────────────────────────────────
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Dims.w(3)

            Repeater {
                model: pinnedTypes

                Item {
                    width: Dims.w(14)
                    height: Dims.w(14)

                    Rectangle {
                        anchors.fill: parent
                        radius: Dims.w(2)
                        color: pinMouse.pressed ? "#2E7D32" : "#1B5E20"
                        border.width: Dims.w(0.5)
                        border.color: "#4CAF50"

                        Behavior on color { ColorAnimation { duration: 100 } }
                    }

                    Icon {
                        anchors.centerIn: parent
                        width: parent.width * 0.55
                        height: width
                        name: modelData.icon || "ios-fitness-outline"
                        color: "white"
                    }

                    MouseArea {
                        id: pinMouse
                        anchors.fill: parent
                        onClicked: {
                            app.enterWorkoutStart(modelData.id)
                        }
                    }
                }
            }
        }

        // ── Divider ─────────────────────────────────────────────────────
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.7
            height: 1
            color: "#40FFFFFF"
        }

        // ── All Workouts button ─────────────────────────────────────────
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.7
            height: Dims.h(8)

            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: allMouse.pressed ? "#1B5E20" : "transparent"
                border.width: 1
                border.color: "#40FFFFFF"

                Behavior on color { ColorAnimation { duration: 100 } }
            }

            Row {
                anchors.centerIn: parent
                spacing: Dims.w(2)

                Icon {
                    width: Dims.l(4)
                    height: width
                    name: "ios-list"
                    color: "#80FFFFFF"
                    anchors.verticalCenter: parent.verticalCenter
                }

                Label {
                    //% "All Workouts"
                    text: qsTrId("id-all-workouts")
                    font.pixelSize: Dims.l(3.5)
                    color: "#C0FFFFFF"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: allMouse
                anchors.fill: parent
                onClicked: {
                    // Navigate to AllWorkoutsPage via L2
                    app.activeFolderId = "workouts"
                    app.navState = "l2"
                }
            }
        }

        // ── Last workout glance ─────────────────────────────────────────
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Dims.h(0.5)
            visible: WorkoutManager.recentWorkouts.length > 0

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                //% "Last workout"
                text: qsTrId("id-last-workout")
                font.pixelSize: Dims.l(2.5)
                color: "#60FFFFFF"
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    var recent = WorkoutManager.recentWorkouts
                    if (recent.length > 0) {
                        var w = recent[0]
                        return (w.name || "") + " · " + (w.duration || "")
                    }
                    return ""
                }
                font.pixelSize: Dims.l(3)
                color: "#4CAF50"
            }
        }
    }
}
