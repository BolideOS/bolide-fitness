import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Application {
    id: app
    centerColor: "#1B5E20"
    outerColor: "#0A1F0B"

    property bool workoutActive: WorkoutManager.active

    // ── Navigation state machine ────────────────────────────────────────
    // States: "l1" (folder browse), "l2" (detail screens), "workout" (active workout)
    property string navState: "l1"
    property string activeFolderId: ""
    property string activeWorkoutType: ""

    onWorkoutActiveChanged: {
        if (workoutActive) {
            navState = "workout"
        } else if (navState === "workout") {
            navState = "l1"
        }
    }

    // ── L1: Vertical folder navigation (PathView, infinite loop) ────────
    PathView {
        id: folderView
        anchors.fill: parent
        visible: navState === "l1"
        interactive: navState === "l1"

        model: ScreenRegistry.folders
        pathItemCount: 3
        flickDeceleration: 3000
        snapMode: PathView.SnapOneItem
        highlightRangeMode: PathView.StrictlyEnforceRange
        preferredHighlightBegin: 0.5
        preferredHighlightEnd: 0.5

        path: Path {
            startX: app.width / 2
            startY: -app.height * 0.5

            PathLine {
                x: app.width / 2
                y: app.height * 1.5
            }
        }

        delegate: Item {
            width: app.width
            height: app.height
            opacity: PathView.isCurrentItem ? 1.0 : 0.4
            scale: PathView.isCurrentItem ? 1.0 : 0.85

            Behavior on opacity { NumberAnimation { duration: 200 } }
            Behavior on scale { NumberAnimation { duration: 200 } }

            Loader {
                id: folderLoader
                anchors.fill: parent
                source: {
                    if (modelData.type === "workout")
                        return "WorkoutFolderPage.qml"
                    else
                        return "FolderGlance.qml"
                }
                asynchronous: !PathView.isCurrentItem

                property var folderData: modelData
            }

            MouseArea {
                anchors.fill: parent
                enabled: PathView.isCurrentItem
                onClicked: {
                    activeFolderId = modelData.id
                    if (modelData.type === "workout") {
                        // Workout folder handles its own tap (pinned items / All Workouts)
                        // Forwarded via folderLoader
                    } else {
                        navState = "l2"
                    }
                }
            }
        }
    }

    // ── L1 vertical scroll indicators ───────────────────────────────────
    Column {
        anchors {
            right: parent.right
            rightMargin: Dims.w(2)
            verticalCenter: parent.verticalCenter
        }
        spacing: Dims.h(1)
        visible: navState === "l1"

        Repeater {
            model: ScreenRegistry.folderCount

            Rectangle {
                width: Dims.w(1.5)
                height: index === folderView.currentIndex ? Dims.h(4) : Dims.h(2)
                radius: width / 2
                color: index === folderView.currentIndex ? "white" : "#40FFFFFF"

                Behavior on height {
                    NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
                }
                Behavior on color {
                    ColorAnimation { duration: 150 }
                }
            }
        }
    }

    // ── L2: Detail view (screens within a folder) ───────────────────────
    DetailView {
        id: detailView
        anchors.fill: parent
        visible: navState === "l2"
        folderId: activeFolderId

        onBackRequested: {
            navState = "l1"
        }
    }

    // ── Active workout: full-screen data screens ────────────────────────
    Loader {
        id: workoutActiveLoader
        anchors.fill: parent
        active: navState === "workout"
        source: "WorkoutActivePage.qml"
    }

    // ── Transition animations ───────────────────────────────────────────
    states: [
        State {
            name: "l1"
            when: navState === "l1"
            PropertyChanges { target: folderView; opacity: 1.0; visible: true }
            PropertyChanges { target: detailView; opacity: 0.0 }
        },
        State {
            name: "l2"
            when: navState === "l2"
            PropertyChanges { target: folderView; opacity: 0.0 }
            PropertyChanges { target: detailView; opacity: 1.0; visible: true }
        },
        State {
            name: "workout"
            when: navState === "workout"
            PropertyChanges { target: folderView; opacity: 0.0 }
            PropertyChanges { target: detailView; opacity: 0.0 }
        }
    ]

    transitions: [
        Transition {
            from: "l1"; to: "l2"
            SequentialAnimation {
                NumberAnimation { target: folderView; property: "opacity"; duration: 200 }
                PropertyAction { target: folderView; property: "visible" }
                NumberAnimation { target: detailView; property: "opacity"; duration: 200 }
            }
        },
        Transition {
            from: "l2"; to: "l1"
            SequentialAnimation {
                NumberAnimation { target: detailView; property: "opacity"; duration: 200 }
                PropertyAction { target: detailView; property: "visible" }
                NumberAnimation { target: folderView; property: "opacity"; duration: 200 }
            }
        },
        Transition {
            from: "*"; to: "workout"
            NumberAnimation { properties: "opacity"; duration: 150 }
        },
        Transition {
            from: "workout"; to: "l1"
            SequentialAnimation {
                NumberAnimation { target: workoutActiveLoader; property: "opacity"; duration: 200 }
                NumberAnimation { target: folderView; property: "opacity"; duration: 200 }
            }
        }
    ]

    // ── Functions called by child components ─────────────────────────────
    function enterFolder(folderId) {
        activeFolderId = folderId
        navState = "l2"
    }

    function enterWorkoutStart(workoutTypeId) {
        activeWorkoutType = workoutTypeId
        activeFolderId = "workouts"
        navState = "l2"
    }

    function backToL1() {
        navState = "l1"
    }
}
