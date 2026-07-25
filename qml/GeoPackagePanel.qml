import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import App 1.0

/**
 * GeoPackagePanel.qml
 *
 * Proporciona la interfaz completa de CRUD para GeoPackage:
 *   • Crear un nuevo .gpkg desde uno o más Shapefiles.
 *   • Abrir un .gpkg existente y ver sus capas.
 *   • Agregar más Shapefiles como nuevas capas al .gpkg activo.
 *   • Eliminar capas individuales del .gpkg activo.
 *
 * NOTA (v0.2.0+): Todas las operaciones ahora son asincrónicas.
 * Los cambios en layerNames, lastError, etc. llegan vía signals.
 * Use la propiedad 'busy' para indicadores visuales.
 */
Item {
    id: root

    // The GeoManager singleton injected from main.qml
    required property var geoMgr

    // -----------------------------------------------------------------------
    // File dialogs
    // -----------------------------------------------------------------------

    // Select multiple .shp files for creating / adding layers
    FileDialog {
        id: shpOpenDialog
        title:       qsTr("Seleccionar Shapefiles")
        nameFilters: [ qsTr("Shapefiles (*.shp)"), qsTr("Todos los archivos (*)") ]
        fileMode:    FileDialog.OpenFiles

        onAccepted: {
            const paths = selectedFiles.map(u => root.geoMgr.urlToPath(u.toString()))
            shpListModel.clear()
            paths.forEach(p => shpListModel.append({ path: p }))
        }
    }

    // Save-as dialog for naming the new GeoPackage
    FileDialog {
        id: gpkgSaveDialog
        title:       qsTr("Guardar GeoPackage como…")
        nameFilters: [ qsTr("GeoPackage (*.gpkg)") ]
        fileMode:    FileDialog.SaveFile
        defaultSuffix: "gpkg"

        onAccepted: {
            const path = root.geoMgr.urlToPath(selectedFile.toString())
            if (shpListModel.count === 0) {
                statusLabel.text = qsTr("⚠ Primero selecciona al menos un Shapefile.")
                return
            }
            const shps = []
            for (let i = 0; i < shpListModel.count; i++)
                shps.push(shpListModel.get(i).path)
            root.geoMgr.createGeoPackage(shps, path)
        }
    }

    // Open an existing GeoPackage
    FileDialog {
        id: gpkgOpenDialog
        title:       qsTr("Abrir GeoPackage existente")
        nameFilters: [ qsTr("GeoPackage (*.gpkg)") ]
        fileMode:    FileDialog.OpenFile

        onAccepted: {
            root.geoMgr.openGeoPackage(
                root.geoMgr.urlToPath(selectedFile.toString()))
        }
    }

    // Select more .shp files to add into the active GPKG
    FileDialog {
        id: shpAddDialog
        title:       qsTr("Agregar Shapefiles al GeoPackage activo")
        nameFilters: [ qsTr("Shapefiles (*.shp)") ]
        fileMode:    FileDialog.OpenFiles

        onAccepted: {
            const paths = selectedFiles.map(u => root.geoMgr.urlToPath(u.toString()))
            root.geoMgr.addLayers(paths)
        }
    }

    // -----------------------------------------------------------------------
    // Confirmation dialog for layer removal
    // -----------------------------------------------------------------------
    Dialog {
        id: confirmDeleteDialog
        title: qsTr("Confirmar eliminación")
        modal: true
        anchors.centerIn: Overlay.overlay

        property string layerName: ""

        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            text: qsTr("¿Eliminar la capa «%1»?").arg(confirmDeleteDialog.layerName)
            wrapMode: Text.WordWrap
        }

        onAccepted: root.geoMgr.removeLayer(confirmDeleteDialog.layerName)
    }

    // GPKG viewer dialog
    GpkgViewerDialog {
        id: viewerDialog
        geoMgr: root.geoMgr
    }

    // -----------------------------------------------------------------------
    // Internal model: selected Shapefiles for the next create operation
    // -----------------------------------------------------------------------
    ListModel { id: shpListModel }

    // -----------------------------------------------------------------------
    // UI layout
    // -----------------------------------------------------------------------
    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: 16
        spacing:         12

        // ── Section title ────────────────────────────────────────────────
        Label {
            text: qsTr("Gestión de GeoPackage")
            font.pixelSize: 16
            font.weight: Font.Medium
        }

        // ── Row 1: Create new GPKG ────────────────────────────────────────
        GroupBox {
            title: qsTr("Crear nuevo GeoPackage")
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                // Shapefile list
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        text: qsTr("Seleccionar .shp…")
                        icon.name: "document-open"
                        enabled: !root.geoMgr.busy
                        onClicked: shpOpenDialog.open()
                    }

                    Label {
                        text: shpListModel.count > 0
                              ? qsTr("%1 archivo(s) seleccionado(s)").arg(shpListModel.count)
                              : qsTr("Ningún archivo seleccionado")
                        font.italic: shpListModel.count === 0
                        color: shpListModel.count === 0 ? Material.color(Material.Grey) : Material.foreground
                    }
                }

                // Tiny list of chosen SHPs
                ListView {
                    visible:         shpListModel.count > 0
                    Layout.fillWidth: true
                    implicitHeight:   Math.min(shpListModel.count * 28, 100)
                    model:            shpListModel
                    clip:             true

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        height: 28
                        contentItem: Label {
                            text:  "• " + model.path
                            elide: Text.ElideLeft
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Button {
                    text: qsTr("Guardar GeoPackage como…")
                    Material.background: Material.Teal
                    Material.foreground: "white"
                    enabled: shpListModel.count > 0 && !root.geoMgr.busy
                    onClicked: gpkgSaveDialog.open()
                }
            }
        }

        // ── Row 2: Open existing GPKG ─────────────────────────────────────
        GroupBox {
            title: qsTr("Abrir GeoPackage existente")
            Layout.fillWidth: true

            RowLayout {
                spacing: 12

                Button {
                    text: qsTr("Abrir .gpkg…")
                    enabled: !root.geoMgr.busy
                    onClicked: gpkgOpenDialog.open()
                }

                Label {
                    text: root.geoMgr.activeGpkgPath.length > 0
                          ? root.geoMgr.activeGpkgPath
                          : qsTr("Ningún GeoPackage abierto")
                    font.italic:   root.geoMgr.activeGpkgPath.length === 0
                    color: root.geoMgr.activeGpkgPath.length === 0
                           ? Material.color(Material.Grey)
                           : Material.foreground
                    elide: Text.ElideLeft
                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Actualizar")
                    flat: true
                    enabled: root.geoMgr.activeGpkgPath.length > 0 && !root.geoMgr.busy
                    onClicked: root.geoMgr.refreshLayers()
                }

                Button {
                    text: qsTr("Visualizar")
                    enabled: root.geoMgr.activeGpkgPath.length > 0 && !root.geoMgr.busy
                    Material.background: Material.Teal
                    Material.foreground: "white"
                    onClicked: viewerDialog.open()
                }
            }
        }

        // ── Row 3: Layer management ───────────────────────────────────────
        GroupBox {
            title: qsTr("Capas en el GeoPackage activo")
            Layout.fillWidth:  true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                // Add-layers button
                Button {
                    text: qsTr("Agregar capas (.shp)…")
                    enabled: root.geoMgr.activeGpkgPath.length > 0 && !root.geoMgr.busy
                    onClicked: shpAddDialog.open()
                }

                // Layer list
                Rectangle {
                    Layout.fillWidth:  true
                    Layout.fillHeight: true
                    border.color: Material.color(Material.Grey, Material.Shade300)
                    border.width: 1
                    radius: 4
                    color: "transparent"

                    Label {
                        visible:           root.geoMgr.layerNames.length === 0
                        anchors.centerIn:  parent
                        text: root.geoMgr.activeGpkgPath.length === 0
                              ? qsTr("Abre un GeoPackage para ver sus capas.")
                              : qsTr("Este GeoPackage no tiene capas.")
                        color: Material.color(Material.Grey)
                        font.italic: true
                    }

                    ListView {
                        id: layerListView
                        anchors.fill:    parent
                        anchors.margins: 4
                        clip:            true
                        model:           root.geoMgr.layerNames

                        ScrollBar.vertical: ScrollBar {}

                        delegate: ItemDelegate {
                            id: layerDelegate
                            required property string modelData
                            required property int    index

                            width: layerListView.width
                            height: 40

                            contentItem: RowLayout {
                                spacing: 8

                                // Geometry icon placeholder
                                Label {
                                    text: "⬡"
                                    font.pixelSize: 16
                                    color: Material.accent
                                }

                                Label {
                                    text: layerDelegate.modelData
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                    verticalAlignment: Text.AlignVCenter
                                }

                                // Delete button
                                RoundButton {
                                    implicitWidth:  28
                                    implicitHeight: 28
                                    flat: true
                                    text: "✕"
                                    font.pixelSize: 12
                                    Material.foreground: Material.color(Material.Red)
                                    ToolTip.text: qsTr("Eliminar capa")
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 500

                                    onClicked: {
                                        confirmDeleteDialog.layerName = layerDelegate.modelData
                                        confirmDeleteDialog.open()
                                    }
                                }
                            }

                            // Divider
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.left:   parent.left
                                anchors.right:  parent.right
                                height: 1
                                color: Material.color(Material.Grey, Material.Shade200)
                                visible: layerDelegate.index < layerListView.count - 1
                            }
                        }
                    }
                }

                // Layer count label
                Label {
                    text: qsTr("%1 capa(s)").arg(root.geoMgr.layerNames.length)
                    font.pixelSize: 11
                    color: Material.color(Material.Grey)
                }
            }
        }

        // ── Internal status (shown within the panel) ──────────────────────

        // Indicador de progreso (visible durante operaciones async)
        Rectangle {
            id: progressContainer
            Layout.fillWidth: true
            height: 50
            visible: root.geoMgr.busy
            color: Material.backgroundColor
            border.color: Material.primary
            border.width: 1
            radius: 4

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                BusyIndicator {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    running: root.geoMgr.busy
                }

                Label {
                    text: qsTr("Procesando operación GeoPackage…")
                    Layout.fillWidth: true
                    color: Material.primary
                }
            }
        }

        // Panel de estado/resultados
        Rectangle {
            id: statusContainer
            Layout.fillWidth: true
            Layout.minimumHeight: 50
            visible: statusLabel.text.length > 0 && !root.geoMgr.busy
            color: statusLabel.color === Material.color(Material.Red) ? 
                   Material.backgroundColor : Material.primary.withAlpha(0.1)
            border.color: statusLabel.color
            border.width: 1
            radius: 4

            Label {
                id: statusLabel
                anchors.fill: parent
                anchors.margins: 12
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // =========================================================================
    // Listeners para operaciones asincrónicas (v0.2.0+)
    // =========================================================================
    Connections {
        target: root.geoMgr

        // Cuando una operación se completa exitosamente
        function onOperationSucceeded(message) {
            statusLabel.color = Material.color(Material.Green)
            statusLabel.text = qsTr("✓ ") + message
        }

        // Cuando hay error en cualquier operación
        function onLastErrorChanged() {
            if (root.geoMgr.lastError.length > 0) {
                statusLabel.color = Material.color(Material.Red)
                statusLabel.text = qsTr("✗ Error: ") + root.geoMgr.lastError
            }
        }

        // Cuando cambia el estado de ocupado
        function onBusyChanged() {
            if (!root.geoMgr.busy) {
                // Operación finalizada (éxito o error)
                // statusLabel ya debería estar actualizado vía signals
            }
        }

        // Cuando cambian las capas disponibles
        function onLayerNamesChanged() {
            // La ListView se actualiza automáticamente vía binding
        }
    }
