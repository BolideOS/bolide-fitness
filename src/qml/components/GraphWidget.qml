import QtQuick 2.9
import org.asteroid.controls 1.0
import org.asteroid.utils 1.0

/*
 * GraphWidget – reusable Canvas-based line/area graph for sensor data.
 *
 * Properties:
 *   graphData   : var (array of numbers)
 *   lineColor   : color
 *   fillColor   : color  (area fill, supports alpha)
 *   minValue    : real   (-1 = auto)
 *   maxValue    : real   (-1 = auto)
 *   showGrid    : bool
 *   gridLines   : int    (horizontal grid count)
 *   showMinMax  : bool   (render min/max labels)
 *   lineWidth   : real
 */
Item {
    id: graphWidget

    property var   graphData:  []
    property color lineColor:  "#2196F3"
    property color fillColor:  "#302196F3"
    property real  minValue:   -1
    property real  maxValue:   -1
    property bool  showGrid:   false
    property int   gridLines:  3
    property bool  showMinMax: false
    property real  lineWidth:  1.5

    property real computedMin: 0
    property real computedMax: 100

    implicitWidth:  Dims.w(90)
    implicitHeight: Dims.h(25)

    onGraphDataChanged: canvas.requestPaint()
    onLineColorChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            var w = width, h = height
            ctx.clearRect(0, 0, w, h)

            var data = graphData
            if (!data || data.length < 2) return

            /* Compute range */
            var lo = data[0], hi = data[0]
            for (var i = 1; i < data.length; i++) {
                if (data[i] < lo) lo = data[i]
                if (data[i] > hi) hi = data[i]
            }
            lo = (minValue >= 0) ? minValue : lo
            hi = (maxValue >= 0) ? maxValue : hi
            if (hi - lo < 1) { lo -= 0.5; hi += 0.5 }
            var range = hi - lo

            computedMin = lo
            computedMax = hi

            /* Grid */
            if (showGrid && gridLines > 0) {
                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.06)
                ctx.lineWidth = 0.5
                for (var g = 1; g <= gridLines; g++) {
                    var gy = h - (g / (gridLines + 1)) * h
                    ctx.beginPath()
                    ctx.moveTo(0, gy)
                    ctx.lineTo(w, gy)
                    ctx.stroke()
                }
            }

            var xStep = w / (data.length - 1)

            /* Area fill */
            ctx.beginPath()
            ctx.moveTo(0, h)
            for (var j = 0; j < data.length; j++) {
                var val = Math.max(lo, Math.min(hi, data[j]))
                var x = j * xStep
                var y = h - ((val - lo) / range) * h * 0.92
                ctx.lineTo(x, y)
            }
            ctx.lineTo(w, h)
            ctx.closePath()
            ctx.fillStyle = fillColor
            ctx.fill()

            /* Line */
            ctx.beginPath()
            for (var k = 0; k < data.length; k++) {
                var v = Math.max(lo, Math.min(hi, data[k]))
                var px = k * xStep
                var py = h - ((v - lo) / range) * h * 0.92
                if (k === 0) ctx.moveTo(px, py)
                else ctx.lineTo(px, py)
            }
            ctx.lineWidth = lineWidth
            ctx.strokeStyle = lineColor
            ctx.stroke()
        }
    }

    /* Optional min/max labels */
    Label {
        visible: showMinMax
        anchors { left: parent.left; bottom: parent.bottom; leftMargin: 2 }
        text: computedMin.toFixed(0)
        font.pixelSize: Dims.l(2)
        color: "#50FFFFFF"
    }

    Label {
        visible: showMinMax
        anchors { right: parent.right; top: parent.top; rightMargin: 2 }
        text: computedMax.toFixed(0)
        font.pixelSize: Dims.l(2)
        color: "#50FFFFFF"
    }
}
