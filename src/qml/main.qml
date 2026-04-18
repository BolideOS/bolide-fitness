import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.asteroid.workout 1.0

Application {
    id: app
    centerColor: "#1B5E20"
    outerColor: "#0A1F0B"

    property bool workoutInProgress: WorkoutController.workoutActive

    onWorkoutInProgressChanged: {
        if (workoutInProgress) {
            pageView.currentIndex = 1
        } else {
            pageView.currentIndex = 0
        }
    }

    ListView {
        id: pageView
        anchors.fill: parent
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        flickDeceleration: 5000
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        interactive: !workoutInProgress
        model: 3
        clip: true

        delegate: Item {
            width: pageView.width
            height: pageView.height

            // Page 0: Workout Type Selector
            Item {
                anchors.fill: parent
                visible: index === 0

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

                    property var workoutTypes: WorkoutController.workoutTypes()
                    property real cellSize: (width - spacing * (columns - 1)) / columns

                    Repeater {
                        model: grid.workoutTypes

                        Item {
                            width: grid.cellSize
                            height: grid.cellSize

                            Rectangle {
                                id: cellBg
                                anchors.fill: parent
                                color: cellMouseArea.pressed ? "#2E7D32" : "#1B5E20"
                                radius: Dims.w(2)
                                border.width: Dims.w(0.5)
                                border.color: "#4CAF50"

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            Column {
                                anchors.centerIn: parent
                                width: parent.width * 0.9
                                spacing: Dims.h(1)

                                Icon {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: Math.min(parent.width * 0.5, Dims.w(12))
                                    height: width
                                    name: modelData.icon
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
                                id: cellMouseArea
                                anchors.fill: parent

                                onClicked: {
                                    if (WorkoutController.startWorkout(modelData.id)) {
                                        pageView.currentIndex = 1
                                    }
                                }
                            }
                        }
                    }
                }
            }

            IconButton {
                anchors {
                    horizontalCenter: parent.horizontalCenter
                    bottom: parent.bottom
                    bottomMargin: Dims.iconButtonMargin
                }
                iconName: "ios-settings-outline"
                visible: !workoutInProgress
                onClicked: pageView.currentIndex = 2
            }
        }

            // Page 1: Active Workout
            WorkoutActivePage {
                anchors.fill: parent
                visible: index === 1
            }

            // Page 2: Settings
            WorkoutSettingsPage {
                anchors.fill: parent
                visible: index === 2
            }
        }
    }

    PageDot {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: Dims.h(2)
        }
        height: Dims.h(2)
        dotNumber: 3
        currentIndex: pageView.currentIndex
        visible: !workoutInProgress
    }
}
