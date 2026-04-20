import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0

/*
 * DataField – compact value + label pair used inside data screens.
 *
 * Properties:
 *   fieldValue  : string  – main text (large, bold)
 *   fieldLabel  : string  – small descriptor
 *   fieldUnit   : string  – optional unit suffix
 *   accentColor : color
 *   fontSize    : real    – base Dims.l factor for value
 */
Item {
    id: dataField

    property string fieldValue:  "--"
    property string fieldLabel:  ""
    property string fieldUnit:   ""
    property color  accentColor: "white"
    property real   fontSize:    7

    implicitWidth:  col.implicitWidth
    implicitHeight: col.implicitHeight

    Column {
        id: col
        anchors.centerIn: parent
        spacing: Dims.h(0.3)

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Dims.w(0.5)

            Label {
                text: fieldValue
                font.pixelSize: Dims.l(fontSize)
                font.bold: true
                color: accentColor
            }

            Label {
                visible: fieldUnit.length > 0
                text: fieldUnit
                font.pixelSize: Dims.l(Math.max(2.5, fontSize * 0.4))
                color: Qt.lighter(accentColor, 1.4)
                anchors.baseline: parent.children[0].baseline
            }
        }

        Label {
            visible: fieldLabel.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            text: fieldLabel
            font.pixelSize: Dims.l(2.5)
            color: "#80FFFFFF"
        }
    }
}
