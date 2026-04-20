import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0

/*
 * MetricCard – reusable rounded card that displays a single metric value
 * with optional icon, label, unit, and accent colour.
 *
 * Properties:
 *   metricValue : string   – the value to display large & bold
 *   metricLabel : string   – small caption below the value
 *   metricUnit  : string   – unit suffix (e.g. "bpm", "km")
 *   iconName    : string   – asteroid icon name (empty → no icon)
 *   accentColor : color    – tint for the value & icon
 *   compact     : bool     – when true, use a tighter layout
 */
Item {
    id: card

    property string metricValue: "--"
    property string metricLabel: ""
    property string metricUnit:  ""
    property string iconName:    ""
    property color  accentColor: "#2196F3"
    property bool   compact:     false

    implicitWidth:  Dims.w(42)
    implicitHeight: compact ? Dims.h(14) : Dims.h(18)

    Rectangle {
        anchors.fill: parent
        radius: Dims.l(2)
        color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 0.5
        border.color: Qt.rgba(1, 1, 1, 0.08)
    }

    Column {
        anchors.centerIn: parent
        spacing: compact ? 0 : Dims.h(0.5)

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Dims.w(1)

            Icon {
                visible: iconName.length > 0
                width: compact ? Dims.l(3) : Dims.l(4)
                height: width
                name: iconName
                color: accentColor
                anchors.verticalCenter: parent.verticalCenter
            }

            Label {
                text: metricValue
                font.pixelSize: compact ? Dims.l(7) : Dims.l(9)
                font.bold: true
                color: accentColor
                anchors.verticalCenter: parent.verticalCenter
            }

            Label {
                visible: metricUnit.length > 0
                text: metricUnit
                font.pixelSize: Dims.l(3)
                color: Qt.lighter(accentColor, 1.4)
                anchors.baseline: parent.children[1].baseline
            }
        }

        Label {
            visible: metricLabel.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            text: metricLabel
            font.pixelSize: Dims.l(2.5)
            color: "#80FFFFFF"
        }
    }
}
