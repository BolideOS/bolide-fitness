/*
 * FolderGlance.qml – Level 1 glance card for a non-workout folder.
 *
 * Displays the folder's primary metric summary: icon, name, key value,
 * and a mini sparkline or progress indicator. Tapping enters L2 detail.
 *
 * The parent delegate sets: property var folderData
 */
import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0
import org.bolide.fitness 1.0

Item {
    id: root

    property var folderData: parent ? parent.folderData : ({})
    property string folderId: folderData.id || ""
    property string folderName: folderData.name ? qsTrId(folderData.name) : ""
    property string folderIcon: folderData.icon || ""
    property color  accentColor: folderData.accentColor || "#FFFFFF"

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    Column {
        anchors.centerIn: parent
        width: parent.width * 0.85
        spacing: Dims.h(2)

        // ── Folder icon ─────────────────────────────────────────────────
        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Dims.l(12)
            height: width
            name: folderIcon
            color: accentColor
        }

        // ── Folder name ─────────────────────────────────────────────────
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: folderName
            font.pixelSize: Dims.l(5)
            font.bold: true
            color: "white"
        }

        // ── Glance content (dispatched by provider type) ────────────────
        Loader {
            id: glanceLoader
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            source: "components/ScreenGlance.qml"

            property string glanceProvider: {
                // Use the first screen's glance provider as the folder's primary glance
                var screens = folderData.screens || []
                if (screens.length > 0)
                    return screens[0].glanceProvider || ""
                return ""
            }
            property color glanceColor: accentColor
        }

        // ── Tap hint ────────────────────────────────────────────────────
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            //% "Tap to view"
            text: qsTrId("id-tap-to-view")
            font.pixelSize: Dims.l(2.5)
            color: "#60FFFFFF"
        }
    }

    // ── Up/down scroll arrows (subtle hints) ────────────────────────────
    Icon {
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top
            topMargin: Dims.h(2)
        }
        width: Dims.l(4)
        height: width
        name: "ios-arrow-up"
        color: "#40FFFFFF"

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            NumberAnimation { from: 0.3; to: 0.7; duration: 1000 }
            NumberAnimation { from: 0.7; to: 0.3; duration: 1000 }
        }
    }

    Icon {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: Dims.h(2)
        }
        width: Dims.l(4)
        height: width
        name: "ios-arrow-down"
        color: "#40FFFFFF"

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            NumberAnimation { from: 0.3; to: 0.7; duration: 1000 }
            NumberAnimation { from: 0.7; to: 0.3; duration: 1000 }
        }
    }
}
