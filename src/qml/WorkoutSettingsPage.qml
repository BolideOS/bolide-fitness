import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0
import Nemo.Configuration 1.0

Item {
    id: settingsPage

    ConfigurationGroup {
        id: workoutSettings
        path: "/bolide-fitness/settings"

        property int zone1Max: 100
        property int zone2Max: 120
        property int zone3Max: 140
        property int zone4Max: 160
        property bool autoPause: true
        property bool gpsTracking: true
        property bool screenAlwaysOn: true
    }

    PageHeader {
        id: header
        //% "Workout Settings"
        text: qsTrId("id-workout-settings")
    }

    Flickable {
        id: flickable
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        contentHeight: contentColumn.height + Dims.h(4)
        clip: true

        Column {
            id: contentColumn
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: Dims.w(2)
            }
            spacing: Dims.h(2)

            // Current workout info (if active)
            Item {
                width: parent.width
                height: workoutInfo.height
                visible: WorkoutController.workoutActive

                Column {
                    id: workoutInfo
                    width: parent.width
                    spacing: Dims.h(1)

                    Label {
                        width: parent.width
                        //% "Current Workout"
                        text: qsTrId("id-current-workout")
                        font.pixelSize: Dims.l(4)
                        color: "#4CAF50"
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: {
                            var types = WorkoutController.workoutTypes()
                            for (var i = 0; i < types.length; i++) {
                                if (types[i].id === WorkoutController.workoutType) {
                                    return types[i].name
                                }
                            }
                            return WorkoutController.workoutType
                        }
                        font.pixelSize: Dims.l(5)
                        color: "white"
                        wrapMode: Text.WordWrap
                    }

                    RowSeparator {
                        width: parent.width
                    }
                }
            }

            // Profile for current workout type
            Column {
                width: parent.width
                spacing: Dims.h(1)

                Label {
                    width: parent.width
                    //% "Power Profile"
                    text: qsTrId("id-power-profile")
                    font.pixelSize: Dims.l(4)
                    color: "#4CAF50"
                    font.bold: true
                }

                Label {
                    width: parent.width
                    //% "Select profile for this workout type"
                    text: qsTrId("id-profile-description")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: WorkoutController.availableProfiles()

                    Item {
                        width: parent.width
                        height: Dims.h(12)

                        property string profileId: modelData.id
                        property string profileName: modelData.name
                        property bool isSelected: WorkoutController.profileForWorkout(WorkoutController.workoutType) === profileId

                        Row {
                            anchors.fill: parent
                            anchors.margins: Dims.w(2)
                            spacing: Dims.w(2)

                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Dims.w(8)
                                height: width
                                name: isSelected ? "ios-checkmark-circle" : "ios-radio-button-off-outline"
                                color: isSelected ? "#4CAF50" : "#80FFFFFF"
                            }

                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - parent.spacing - Dims.w(8)
                                text: profileName
                                font.pixelSize: Dims.l(4)
                                color: isSelected ? "white" : "#CCFFFFFF"
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                WorkoutController.setProfileForWorkout(WorkoutController.workoutType, profileId)
                            }
                        }
                    }
                }

                RowSeparator {
                    width: parent.width
                }
            }

            // HR Zone Thresholds
            Column {
                width: parent.width
                spacing: Dims.h(1)

                Label {
                    width: parent.width
                    //% "Heart Rate Zones"
                    text: qsTrId("id-hr-zones")
                    font.pixelSize: Dims.l(4)
                    color: "#4CAF50"
                    font.bold: true
                }

                Label {
                    width: parent.width
                    //% "Adjust zone thresholds (bpm)"
                    text: qsTrId("id-hr-zones-description")
                    font.pixelSize: Dims.l(3)
                    color: "#80FFFFFF"
                    wrapMode: Text.WordWrap
                }

                // Zone 1: Warm-up (< zone1Max)
                Column {
                    width: parent.width
                    spacing: Dims.h(0.5)

                    Row {
                        width: parent.width
                        spacing: Dims.w(2)

                        Rectangle {
                            width: Dims.w(4)
                            height: Dims.h(4)
                            radius: Dims.w(1)
                            color: "#9E9E9E"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Label {
                            width: parent.width - Dims.w(6) - Dims.w(12)
                            anchors.verticalCenter: parent.verticalCenter
                            //% "Warm-up"
                            text: qsTrId("id-zone-warmup")
                            font.pixelSize: Dims.l(4)
                        }

                        Label {
                            width: Dims.w(12)
                            anchors.verticalCenter: parent.verticalCenter
                            text: "< " + workoutSettings.zone1Max
                            font.pixelSize: Dims.l(4)
                            color: "#80FFFFFF"
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    IntSelector {
                        width: parent.width
                        height: Dims.h(8)
                        min: 80
                        max: 120
                        stepSize: 1
                        value: workoutSettings.zone1Max
                        onValueChanged: workoutSettings.zone1Max = value
                    }
                }

                // Zone 2: Fat Burn (zone1Max - zone2Max)
                Column {
                    width: parent.width
                    spacing: Dims.h(0.5)

                    Row {
                        width: parent.width
                        spacing: Dims.w(2)

                        Rectangle {
                            width: Dims.w(4)
                            height: Dims.h(4)
                            radius: Dims.w(1)
                            color: "#4CAF50"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Label {
                            width: parent.width - Dims.w(6) - Dims.w(12)
                            anchors.verticalCenter: parent.verticalCenter
                            //% "Fat Burn"
                            text: qsTrId("id-zone-fat-burn")
                            font.pixelSize: Dims.l(4)
                        }

                        Label {
                            width: Dims.w(12)
                            anchors.verticalCenter: parent.verticalCenter
                            text: workoutSettings.zone1Max + "-" + workoutSettings.zone2Max
                            font.pixelSize: Dims.l(4)
                            color: "#80FFFFFF"
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    IntSelector {
                        width: parent.width
                        height: Dims.h(8)
                        min: workoutSettings.zone1Max + 5
                        max: 140
                        stepSize: 1
                        value: workoutSettings.zone2Max
                        onValueChanged: workoutSettings.zone2Max = value
                    }
                }

                // Zone 3: Cardio (zone2Max - zone3Max)
                Column {
                    width: parent.width
                    spacing: Dims.h(0.5)

                    Row {
                        width: parent.width
                        spacing: Dims.w(2)

                        Rectangle {
                            width: Dims.w(4)
                            height: Dims.h(4)
                            radius: Dims.w(1)
                            color: "#FF9800"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Label {
                            width: parent.width - Dims.w(6) - Dims.w(12)
                            anchors.verticalCenter: parent.verticalCenter
                            //% "Cardio"
                            text: qsTrId("id-zone-cardio")
                            font.pixelSize: Dims.l(4)
                        }

                        Label {
                            width: Dims.w(12)
                            anchors.verticalCenter: parent.verticalCenter
                            text: workoutSettings.zone2Max + "-" + workoutSettings.zone3Max
                            font.pixelSize: Dims.l(4)
                            color: "#80FFFFFF"
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    IntSelector {
                        width: parent.width
                        height: Dims.h(8)
                        min: workoutSettings.zone2Max + 5
                        max: 160
                        stepSize: 1
                        value: workoutSettings.zone3Max
                        onValueChanged: workoutSettings.zone3Max = value
                    }
                }

                // Zone 4: Peak (zone3Max - zone4Max)
                Column {
                    width: parent.width
                    spacing: Dims.h(0.5)

                    Row {
                        width: parent.width
                        spacing: Dims.w(2)

                        Rectangle {
                            width: Dims.w(4)
                            height: Dims.h(4)
                            radius: Dims.w(1)
                            color: "#F44336"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Label {
                            width: parent.width - Dims.w(6) - Dims.w(12)
                            anchors.verticalCenter: parent.verticalCenter
                            //% "Peak"
                            text: qsTrId("id-zone-peak")
                            font.pixelSize: Dims.l(4)
                        }

                        Label {
                            width: Dims.w(12)
                            anchors.verticalCenter: parent.verticalCenter
                            text: workoutSettings.zone3Max + "-" + workoutSettings.zone4Max
                            font.pixelSize: Dims.l(4)
                            color: "#80FFFFFF"
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    IntSelector {
                        width: parent.width
                        height: Dims.h(8)
                        min: workoutSettings.zone3Max + 5
                        max: 180
                        stepSize: 1
                        value: workoutSettings.zone4Max
                        onValueChanged: workoutSettings.zone4Max = value
                    }
                }

                // Zone 5: Maximum (> zone4Max)
                Row {
                    width: parent.width
                    spacing: Dims.w(2)

                    Rectangle {
                        width: Dims.w(4)
                        height: Dims.h(4)
                        radius: Dims.w(1)
                        color: "#C62828"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Label {
                        width: parent.width - Dims.w(6) - Dims.w(12)
                        anchors.verticalCenter: parent.verticalCenter
                        //% "Maximum"
                        text: qsTrId("id-zone-maximum")
                        font.pixelSize: Dims.l(4)
                    }

                    Label {
                        width: Dims.w(12)
                        anchors.verticalCenter: parent.verticalCenter
                        text: "> " + workoutSettings.zone4Max
                        font.pixelSize: Dims.l(4)
                        color: "#80FFFFFF"
                        horizontalAlignment: Text.AlignRight
                    }
                }

                RowSeparator {
                    width: parent.width
                }
            }

            // Workout Options
            Column {
                width: parent.width
                spacing: Dims.h(1)

                Label {
                    width: parent.width
                    //% "Workout Options"
                    text: qsTrId("id-workout-options")
                    font.pixelSize: Dims.l(4)
                    color: "#4CAF50"
                    font.bold: true
                }

                // Auto-pause toggle
                LabeledSwitch {
                    width: parent.width
                    height: Dims.h(12)
                    //% "Auto-pause"
                    text: qsTrId("id-auto-pause")
                    checked: workoutSettings.autoPause
                    onCheckedChanged: workoutSettings.autoPause = checked
                }

                // GPS tracking toggle
                LabeledSwitch {
                    width: parent.width
                    height: Dims.h(12)
                    //% "GPS Tracking"
                    text: qsTrId("id-gps-tracking")
                    checked: workoutSettings.gpsTracking
                    onCheckedChanged: workoutSettings.gpsTracking = checked
                }

                // Screen always-on toggle
                LabeledSwitch {
                    width: parent.width
                    height: Dims.h(12)
                    //% "Screen Always On"
                    text: qsTrId("id-screen-always-on")
                    checked: workoutSettings.screenAlwaysOn
                    onCheckedChanged: workoutSettings.screenAlwaysOn = checked
                }
            }
        }
    }

    // Back button
    IconButton {
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: Dims.iconButtonMargin
        }
        iconName: "ios-arrow-back-outline"
        onClicked: {
            var view = parent
            while (view && !view.hasOwnProperty("currentIndex")) {
                view = view.parent
            }
            if (view && WorkoutController.workoutActive) {
                view.currentIndex = 1
            } else if (view) {
                view.currentIndex = 0
            }
        }
    }
}
