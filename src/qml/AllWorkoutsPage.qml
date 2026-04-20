/*
 * AllWorkoutsPage.qml – Full list of all workout types.
 *
 * Scrollable list showing all available workout types. Tap to go to
 * WorkoutStartPage for that type. Used as the L2 content for the
 * Workout folder.
 *
 * Swipe right to go back to L1.
 */
import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: root

    property var workoutTypes: WorkoutManager.workoutTypes

    Rectangle { anchors.fill: parent; color: "#000000" }

    // ── Header ──────────────────────────────────────────────────────────
    Label {
        id: header
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top
            topMargin: Dims.h(4)
        }
        //% "All Workouts"
        text: qsTrId("id-all-workouts")
        font.pixelSize: Dims.l(4.5)
        font.bold: true
        color: "white"
        z: 2
    }

    // ── Scrollable workout list ─────────────────────────────────────────
    Flickable {
        anchors {
            top: header.bottom
            topMargin: Dims.h(2)
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            bottomMargin: Dims.h(2)
        }
        contentHeight: listColumn.height + Dims.h(4)
        clip: true

        Column {
            id: listColumn
            width: parent.width
            spacing: Dims.h(1)

            Repeater {
                model: workoutTypes

                Item {
                    width: parent.width
                    height: Dims.h(12)

                    Rectangle {
                        anchors {
                            fill: parent
                            leftMargin: Dims.w(4)
                            rightMargin: Dims.w(4)
                        }
                        radius: Dims.w(2)
                        color: rowMouse.pressed ? "#1B5E20" : "#10FFFFFF"
                        border.width: ScreenRegistry.isWorkoutPinned(modelData.id) ? 1 : 0
                        border.color: "#4CAF50"

                        Behavior on color { ColorAnimation { duration: 100 } }

                        Row {
                            anchors {
                                verticalCenter: parent.verticalCenter
                                left: parent.left
                                leftMargin: Dims.w(3)
                                right: parent.right
                                rightMargin: Dims.w(3)
                            }
                            spacing: Dims.w(3)

                            Icon {
                                width: Dims.l(6)
                                height: width
                                name: modelData.icon || "ios-fitness-outline"
                                color: "white"
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: Dims.h(0.3)

                                Label {
                                    text: modelData.name || ""
                                    font.pixelSize: Dims.l(3.5)
                                    color: "white"
                                }

                                Label {
                                    text: modelData.category || ""
                                    font.pixelSize: Dims.l(2.5)
                                    color: "#60FFFFFF"
                                }
                            }

                            // Pin indicator
                            Icon {
                                visible: ScreenRegistry.isWorkoutPinned(modelData.id)
                                width: Dims.l(3)
                                height: width
                                name: "ios-star"
                                color: "#FFC107"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        onClicked: {
                            app.enterWorkoutStart(modelData.id)
                        }
                    }
                }
            }
        }
    }
}
