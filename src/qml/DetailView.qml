/*
 * DetailView.qml – Level 2 container for screens within a folder.
 *
 * Vertical PathView for the screens in the active folder.
 * Swipe right (horizontal flick) to go back to L1.
 *
 * For the workout folder, this loads WorkoutStartPage instead.
 */
import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: root

    property string folderId: ""
    property var screenList: folderId ? ScreenRegistry.screens(folderId) : []
    property bool isWorkoutFolder: {
        var f = ScreenRegistry.folder(folderId)
        return f.type === "workout"
    }

    signal backRequested()

    // ── Swipe-right gesture detection for back navigation ───────────────
    MouseArea {
        id: swipeBack
        anchors.fill: parent
        z: 1 // Below screen content but captures edge swipes

        property real startX: 0
        property bool tracking: false

        onPressed: {
            startX = mouse.x
            tracking = (mouse.x < Dims.w(15)) // Only from left edge
        }

        onPositionChanged: {
            if (tracking && (mouse.x - startX) > Dims.w(25)) {
                tracking = false
                root.backRequested()
            }
        }

        onReleased: tracking = false

        // Pass through to children when not swiping
        propagateComposedEvents: true
        onClicked: mouse.accepted = false
        onDoubleClicked: mouse.accepted = false
        onPressAndHold: mouse.accepted = false
    }

    // ── Back arrow indicator (left edge) ────────────────────────────────
    Icon {
        anchors {
            left: parent.left
            leftMargin: Dims.w(2)
            verticalCenter: parent.verticalCenter
        }
        width: Dims.l(4)
        height: width
        name: "ios-arrow-back"
        color: "#40FFFFFF"
        z: 2

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            NumberAnimation { from: 0.2; to: 0.5; duration: 1500 }
            NumberAnimation { from: 0.5; to: 0.2; duration: 1500 }
        }
    }

    // ── Workout folder: show AllWorkoutsPage or WorkoutStartPage ───────
    Loader {
        id: workoutLoader
        anchors.fill: parent
        active: isWorkoutFolder
        visible: isWorkoutFolder
        source: {
            if (!isWorkoutFolder) return ""
            if (app.activeWorkoutType)
                return "WorkoutStartPage.qml"
            return "AllWorkoutsPage.qml"
        }
        z: 0
    }

    // ── Screen list (vertical scroll) — for non-workout folders ─────────
    PathView {
        id: screenView
        anchors.fill: parent
        z: 0
        visible: !isWorkoutFolder

        model: screenList
        pathItemCount: 3
        flickDeceleration: 3000
        snapMode: PathView.SnapOneItem
        highlightRangeMode: PathView.StrictlyEnforceRange
        preferredHighlightBegin: 0.5
        preferredHighlightEnd: 0.5
        interactive: screenList.length > 1

        path: Path {
            startX: root.width / 2
            startY: -root.height * 0.5

            PathLine {
                x: root.width / 2
                y: root.height * 1.5
            }
        }

        delegate: Item {
            width: root.width
            height: root.height
            opacity: PathView.isCurrentItem ? 1.0 : 0.3
            scale: PathView.isCurrentItem ? 1.0 : 0.85

            Behavior on opacity { NumberAnimation { duration: 200 } }
            Behavior on scale { NumberAnimation { duration: 200 } }

            Loader {
                anchors.fill: parent
                source: modelData.qmlSource || ""
                asynchronous: !PathView.isCurrentItem
                active: Math.abs(index - screenView.currentIndex) <= 1
                         || screenList.length <= 3
            }
        }
    }

    // ── Vertical scroll indicators (right side) ─────────────────────────
    Column {
        anchors {
            right: parent.right
            rightMargin: Dims.w(2)
            verticalCenter: parent.verticalCenter
        }
        spacing: Dims.h(1)
        visible: screenList.length > 1

        Repeater {
            model: screenList.length

            Rectangle {
                width: Dims.w(1.5)
                height: index === screenView.currentIndex ? Dims.h(4) : Dims.h(2)
                radius: width / 2
                color: index === screenView.currentIndex ? "white" : "#40FFFFFF"

                Behavior on height {
                    NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
                }
            }
        }
    }

    // ── Folder name header (shows briefly on entry) ─────────────────────
    Label {
        id: folderLabel
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top
            topMargin: Dims.h(3)
        }
        text: {
            var f = ScreenRegistry.folder(folderId)
            return f.name ? qsTrId(f.name) : ""
        }
        font.pixelSize: Dims.l(3.5)
        color: "white"
        opacity: folderLabelTimer.running ? 0.8 : 0.0
        z: 3

        Behavior on opacity { NumberAnimation { duration: 300 } }
    }

    Timer {
        id: folderLabelTimer
        interval: 2000
    }

    onFolderIdChanged: {
        if (folderId) {
            screenView.currentIndex = 0
            folderLabelTimer.restart()
        }
    }
}
