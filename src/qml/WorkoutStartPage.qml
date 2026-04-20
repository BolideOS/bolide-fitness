/*
 * WorkoutStartPage.qml – Pre-workout screen for a specific workout type.
 *
 * Layout:
 *   - Workout name + icon (center)
 *   - Start button (top-right corner)
 *   - Sensor status icons (active sensors for this workout)
 *   - GPS readiness indicator (for GPS workouts)
 *   - Power profile badge
 *   - Settings gear icon → WorkoutSettingsPage
 *   - Pin/unpin toggle
 *
 * Swipe right to go back.
 */
import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0
import "components"

Item {
    id: root

    property string workoutTypeId: app.activeWorkoutType || ""
    property var workoutData: {
        var types = WorkoutManager.workoutTypes
        for (var i = 0; i < types.length; i++) {
            if (types[i].id === workoutTypeId)
                return types[i]
        }
        return {}
    }
    property bool isPinned: ScreenRegistry.isWorkoutPinned(workoutTypeId)
    property bool showSettings: false

    Rectangle { anchors.fill: parent; color: "#000000" }

    // ── Settings page (overlay) ─────────────────────────────────────────
    Loader {
        id: settingsLoader
        anchors.fill: parent
        active: showSettings
        source: "WorkoutSettingsPage.qml"
        z: 10

        Connections {
            target: settingsLoader.item
            ignoreUnknownSignals: true
            onBackRequested: showSettings = false
        }
    }

    // ── Main content (visible when settings not shown) ──────────────────
    Item {
        anchors.fill: parent
        visible: !showSettings

        // ── Start button (top-right) ────────────────────────────────────
        Rectangle {
            id: startButton
            anchors {
                top: parent.top
                right: parent.right
                topMargin: Dims.h(4)
                rightMargin: Dims.w(4)
            }
            width: Dims.w(18)
            height: Dims.h(8)
            radius: height / 2
            color: startMouse.pressed ? "#2E7D32" : "#4CAF50"

            Behavior on color { ColorAnimation { duration: 100 } }

            Label {
                anchors.centerIn: parent
                //% "Start"
                text: qsTrId("id-start")
                font.pixelSize: Dims.l(3.5)
                font.bold: true
                color: "white"
            }

            MouseArea {
                id: startMouse
                anchors.fill: parent
                onClicked: {
                    WorkoutManager.startWorkout(workoutTypeId)
                }
            }
        }

        // ── Center content ──────────────────────────────────────────────
        Column {
            anchors.centerIn: parent
            width: parent.width * 0.85
            spacing: Dims.h(2)

            // Workout icon
            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Dims.l(16)
                height: width
                name: workoutData.icon || "ios-fitness-outline"
                color: "#4CAF50"
            }

            // Workout name
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: workoutData.name || ""
                font.pixelSize: Dims.l(5)
                font.bold: true
                color: "white"
            }

            // ── Sensor status icons ─────────────────────────────────────
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Dims.w(3)

                property var sensors: workoutData.sensors || []

                Repeater {
                    model: parent.sensors

                    Icon {
                        width: Dims.l(4)
                        height: width
                        name: {
                            switch (modelData) {
                                case "heart_rate":   return "ios-heart-outline"
                                case "gps":          return "ios-navigate-outline"
                                case "barometer":    return "ios-speedometer-outline"
                                case "accelerometer": return "ios-move"
                                case "step_count":   return "ios-walk"
                                default:             return "ios-radio-button-on"
                            }
                        }
                        color: {
                            // GPS gets special coloring for readiness
                            if (modelData === "gps")
                                return "#FFC107" // Yellow = acquiring
                            return "#80FFFFFF"
                        }
                    }
                }
            }

            // ── Power profile badge ─────────────────────────────────────
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: profileLabel.width + Dims.w(6)
                height: Dims.h(5)
                radius: height / 2
                color: "#20FFFFFF"

                Label {
                    id: profileLabel
                    anchors.centerIn: parent
                    text: {
                        var pid = WorkoutManager.profileForWorkout(workoutTypeId)
                        switch (pid) {
                            case "low_power": return qsTrId("id-low-power")
                            case "balanced":  return qsTrId("id-balanced")
                            case "high_perf": return qsTrId("id-high-perf")
                            case "ultra":     return qsTrId("id-ultra")
                            default:          return pid || qsTrId("id-balanced")
                        }
                    }
                    font.pixelSize: Dims.l(2.5)
                    color: "#B0FFFFFF"
                }
            }
        }

        // ── Bottom toolbar: Settings + Pin ──────────────────────────────
        Row {
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: Dims.h(6)
            }
            spacing: Dims.w(8)

            // Settings button
            Item {
                width: Dims.w(12)
                height: width

                Icon {
                    anchors.centerIn: parent
                    width: Dims.l(6)
                    height: width
                    name: "ios-settings-outline"
                    color: "#80FFFFFF"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: showSettings = true
                }
            }

            // Pin/Unpin button
            Item {
                width: Dims.w(12)
                height: width

                Icon {
                    anchors.centerIn: parent
                    width: Dims.l(6)
                    height: width
                    name: isPinned ? "ios-star" : "ios-star-outline"
                    color: isPinned ? "#FFC107" : "#80FFFFFF"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (isPinned)
                            ScreenRegistry.unpinWorkout(workoutTypeId)
                        else
                            ScreenRegistry.pinWorkout(workoutTypeId)
                        isPinned = ScreenRegistry.isWorkoutPinned(workoutTypeId)
                    }
                }
            }
        }
    }
}
