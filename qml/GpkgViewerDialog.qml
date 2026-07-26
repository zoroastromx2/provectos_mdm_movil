import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import App 1.0

Dialog {
    id: root

    required property GeoManager geoMgr

    title: qsTr("Visor de GeoPackage")
    modal: true
    closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay

    // Tamaño inicial responsivo; se actualiza al abrir y al redimensionar
    property real dlgWidth:  Overlay.overlay ? Math.max(520, Overlay.overlay.width  * 0.85) : 640
    property real dlgHeight: Overlay.overlay ? Math.max(380, Overlay.overlay.height * 0.85) : 500

    width:  dlgWidth
    height: dlgHeight

    // Sincronizar tamaño inicial y cargar datos cada vez que se abre el diálogo
    onOpened: {
        dlgWidth   = Overlay.overlay ? Math.max(520, Overlay.overlay.width  * 0.85) : 640
        dlgHeight  = Overlay.overlay ? Math.max(380, Overlay.overlay.height * 0.85) : 500
        fileInfo   = root.geoMgr.getGpkgFileInfo()
        layerList  = root.geoMgr.getAllLayerInfo()
    }

    // Pesos de columna (valor relativo; RowLayout los normaliza)
    // Extensión recibe más espacio, Features menos
    readonly property real colNombre:   3.0
    readonly property real colTipo:     1.8
    readonly property real colCRS:      1.8
    readonly property real colFeatures: 1.0
    readonly property real colExtent:   2.4

    // Tamaño mínimo del diálogo al redimensionar
    readonly property real minDlgW: 420
    readonly property real minDlgH: 300

    property var fileInfo:  ({})
    property var layerList: []

    // ── Handles de redimensionado ─────────────────────────────────────
    // Borde derecho
    MouseArea {
        z: 10
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: 6
        cursorShape: Qt.SizeHorCursor
        property real startX: 0
        property real startW: 0
        onPressed: (mouse) => { startX = mouse.x; startW = root.dlgWidth }
        onPositionChanged: (mouse) => {
            const delta = mouse.x - startX
            root.dlgWidth = Math.max(root.minDlgW, startW + delta)
        }
    }
    // Borde izquierdo
    MouseArea {
        z: 10
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: 6
        cursorShape: Qt.SizeHorCursor
        property real startX: 0
        property real startW: 0
        onPressed: (mouse) => { startX = mouse.x; startW = root.dlgWidth }
        onPositionChanged: (mouse) => {
            const delta = startX - mouse.x
            root.dlgWidth = Math.max(root.minDlgW, startW + delta)
        }
    }
    // Borde inferior
    MouseArea {
        z: 10
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 6
        cursorShape: Qt.SizeVerCursor
        property real startY: 0
        property real startH: 0
        onPressed: (mouse) => { startY = mouse.y; startH = root.dlgHeight }
        onPositionChanged: (mouse) => {
            const delta = mouse.y - startY
            root.dlgHeight = Math.max(root.minDlgH, startH + delta)
        }
    }
    // Borde superior
    MouseArea {
        z: 10
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 6
        cursorShape: Qt.SizeVerCursor
        property real startY: 0
        property real startH: 0
        onPressed: (mouse) => { startY = mouse.y; startH = root.dlgHeight }
        onPositionChanged: (mouse) => {
            const delta = startY - mouse.y
            root.dlgHeight = Math.max(root.minDlgH, startH + delta)
        }
    }
    // Esquina inferior-derecha
    MouseArea {
        z: 11
        anchors { right: parent.right; bottom: parent.bottom }
        width: 12; height: 12
        cursorShape: Qt.SizeFDiagCursor
        property real startX: 0; property real startY: 0
        property real startW: 0; property real startH: 0
        onPressed: (mouse) => { startX = mouse.x; startY = mouse.y; startW = root.dlgWidth; startH = root.dlgHeight }
        onPositionChanged: (mouse) => {
            root.dlgWidth  = Math.max(root.minDlgW, startW + mouse.x - startX)
            root.dlgHeight = Math.max(root.minDlgH, startH + mouse.y - startY)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        // ── File info ────────────────────────────────────────────────
        GroupBox {
            title: qsTr("Información del archivo")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 6
                width: parent.width

                Label { text: qsTr("Ruta:"); font.weight: Font.Medium }
                Label {
                    text: root.fileInfo.path || ""
                    Layout.fillWidth: true
                    elide: Text.ElideLeft
                }

                Label { text: qsTr("Tamaño:"); font.weight: Font.Medium }
                Label {
                    text: root.fileInfo.sizeBytes
                          ? formatBytes(root.fileInfo.sizeBytes) : ""
                }

                Label { text: qsTr("Modificado:"); font.weight: Font.Medium }
                Label { text: root.fileInfo.lastModified || "" }

                Label { text: qsTr("Capas:"); font.weight: Font.Medium }
                Label { text: root.fileInfo.totalLayers ?? "" }
            }
        }

        // ── Layer table ──────────────────────────────────────────────
        GroupBox {
            title: qsTr("Capas (%1)").arg(root.layerList.length)
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                width: parent.width
                height: parent.height
                spacing: 4

                // Header row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredWidth: root.colNombre
                        height: 28
                        color: Material.color(Material.Grey, Material.Shade200)
                        radius: 2
                        Label { anchors.fill: parent; anchors.leftMargin: 6; text: qsTr("Nombre"); font.pixelSize: 12; font.weight: Font.Medium; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredWidth: root.colTipo
                        height: 28
                        color: Material.color(Material.Grey, Material.Shade200)
                        radius: 2
                        Label { anchors.fill: parent; anchors.leftMargin: 6; text: qsTr("Tipo"); font.pixelSize: 12; font.weight: Font.Medium; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredWidth: root.colCRS
                        height: 28
                        color: Material.color(Material.Grey, Material.Shade200)
                        radius: 2
                        Label { anchors.fill: parent; anchors.leftMargin: 6; text: qsTr("CRS"); font.pixelSize: 12; font.weight: Font.Medium; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredWidth: root.colFeatures
                        height: 28
                        color: Material.color(Material.Grey, Material.Shade200)
                        radius: 2
                        Label { anchors.fill: parent; anchors.leftMargin: 6; text: qsTr("Features"); font.pixelSize: 12; font.weight: Font.Medium; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredWidth: root.colExtent
                        height: 28
                        color: Material.color(Material.Grey, Material.Shade200)
                        radius: 2
                        Label { anchors.fill: parent; anchors.leftMargin: 6; text: qsTr("Extensión"); font.pixelSize: 12; font.weight: Font.Medium; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    }
                }

                // Data rows
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.layerList

                    ScrollBar.vertical: ScrollBar {}

                    delegate: ItemDelegate {
                        id: layerRow
                        required property var modelData
                        required property int index

                        width: ListView.view.width
                        height: 32

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 4
                            spacing: 2

                            Label {
                                text: layerRow.modelData.name || ""
                                Layout.fillWidth: true
                                Layout.preferredWidth: root.colNombre
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Label {
                                text: layerRow.modelData.geomType || ""
                                Layout.fillWidth: true
                                Layout.preferredWidth: root.colTipo
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Label {
                                text: layerRow.modelData.crsAuth || ""
                                Layout.fillWidth: true
                                Layout.preferredWidth: root.colCRS
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Label {
                                text: layerRow.modelData.featureCount
                                      ? Number(layerRow.modelData.featureCount).toLocaleString() : "0"
                                Layout.fillWidth: true
                                Layout.preferredWidth: root.colFeatures
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Label {
                                text: layerRow.modelData.minX !== undefined
                                      ? qsTr("%1, %2").arg(
                                            Number(layerRow.modelData.minX).toLocaleString(undefined, 'fixed', 2)).arg(
                                            Number(layerRow.modelData.minY).toLocaleString(undefined, 'fixed', 2))
                                      : ""
                                Layout.fillWidth: true
                                Layout.preferredWidth: root.colExtent
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 1
                            color: Material.color(Material.Grey, Material.Shade200)
                            visible: layerRow.index < root.layerList.length - 1
                        }
                    }
                }
            }
        }

        // ── Close button ─────────────────────────────────────────────
        Button {
            text: qsTr("Cerrar")
            Layout.alignment: Qt.AlignHCenter
            onClicked: root.close()
        }
    }

    // Helper
    function formatBytes(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + " MB"
        return (bytes / 1073741824).toFixed(2) + " GB"
    }
}