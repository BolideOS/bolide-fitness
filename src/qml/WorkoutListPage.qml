import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0
import "components"

Item {
    id: workoutListPage

    property bool workoutActive: WorkoutManager.active

    // ── When workout starts, show active page ───────────────────────────
    Item {
        id: selectorView
        anchors.fill: parent
        visible: !workoutActive

        PageHeader {
            id: header
            //% "Workout"
            text: qsTrId("id-workout-title")
        }

        Flickable {
            id: flickable
            anchors {
                top: header.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                bottomMargin: Dims.h(4)
            }
            contentHeight: grid.height + Dims.h(4)
            clip: true

            Grid {
                id: grid
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    leftMargin: Dims.w(4)
                    rightMargin: Dims.w(4)
                    topMargin: Dims.h(2)
                }
                columns: 3
                spacing: Dims.w(2)
                rowSpacing: Dims.h(2)

                property var types: WorkoutManager.workoutTypes
                property real cellSize: (width - spacing * (columns - 1)) / columns

                Repeater {
                    model: grid.types

                    Item {
                        width: grid.cellSize
                        height: grid.cellSize

                        Rectangle {
                            id: cellBg
                            anchors.fill: parent
                            color: cellMouse.pressed ? "#2E7D32" : "#1B5E20"
                            radius: Dims.w(2)
                            border.width: Dims.w(0.5)
                            border.color: "#4CAF50"
                            Behavior on color { ColorAnimation { duration: 100 } }
                        }

                        Column {
                            anchors.centerIn: parent
                            width: parent.width * 0.9
                            spacing: Dims.h(1)

                            Icon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: Math.min(parent.width * 0.5, Dims.w(12))
                                height: width
                                name: modelData.icon || "ios-fitness-outline"
                                color: "white"
                            }

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width
                                text: modelData.name
                                font.pixelSize: Dims.l(3.5)
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            id: cellMouse
                            anchors.fill: parent
                            onClicked: {
                                WorkoutManager.startWorkout(modelData.id)
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Active workout view ─────────────────────────────────────────────
    WorkoutActivePage {
        anchors.fill: parent
        visible: workoutActive
    }
}
