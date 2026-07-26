import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts
import QtQuick.Dialogs
import App 1.0

Item {
    id: root
    implicitHeight: mainLayout.implicitHeight + 32

    // Tipado fuerte (mejor validación en carga)
    required property GeoManager geoMgr
    required property QgisProjectGenerator qgisMgr

    property string sourceGpkgPath: root.geoMgr.activeGpkgPath
    property string outputQgzPath: ""

    Connections {
        target: root.geoMgr
        function onActiveGpkgPathChanged() {
            root.sourceGpkgPath = root.geoMgr.activeGpkgPath
        }
    }

    FileDialog {
        id: gpkgSelectDialog
        title: qsTr("Seleccionar GeoPackage de origen")
        nameFilters: [qsTr("GeoPackage (*.gpkg)")]
        fileMode: FileDialog.OpenFile
        onAccepted: root.sourceGpkgPath = root.geoMgr.urlToPath(selectedFile.toString())
    }

    FileDialog {
        id: qgzSaveDialog
        title: qsTr("Guardar proyecto QGIS (.qgz) como…")
        nameFilters: [qsTr("Proyecto QGIS (*.qgz)")]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "qgz"
        onAccepted: root.outputQgzPath = root.geoMgr.urlToPath(selectedFile.toString())
    }

    ColumnLayout {
        id: mainLayout
        x: 16
        y: 16
        width: parent.width - 32
        spacing: 12

        Label {
            text: qsTr("Generador de Proyecto QGIS (.qgz)")
            font.pixelSize: 16
            font.weight: Font.Medium
        }

        // Indicador de progreso (visible durante generación)
        Rectangle {
            id: progressContainer
            Layout.fillWidth: true
            height: 50
            visible: root.qgisMgr.busy
            color: Material.backgroundColor
            border.color: Material.primary
            border.width: 1
            radius: 4

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                BusyIndicator {
                    id: busyIndicator
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    running: root.qgisMgr.busy
                }

                Label {
                    id: progressLabel
                    text: qsTr("Generando proyecto QGIS…")
                    Layout.fillWidth: true
                    color: Material.primary
                }
            }
        }

        // Panel de entrada
        GroupBox {
            id: inputGroup
            title: qsTr("Configuración")
            Layout.fillWidth: true

            ColumnLayout {
                width: parent.width
                spacing: 8

                RowLayout {
                    spacing: 8

                    Label {
                        text: qsTr("Archivo de origen:")
                        Layout.minimumWidth: 90
                        Layout.preferredWidth: Math.max(90, Math.min(160, root.width * 0.22))
                    }

                    TextField {
                        id: sourceField
                        text: root.sourceGpkgPath
                        readOnly: true
                        Layout.fillWidth: true
                        placeholderText: qsTr("Ninguno seleccionado")
                    }

                    Button {
                        text: qsTr("Seleccionar…")
                        enabled: !root.qgisMgr.busy
                        onClicked: gpkgSelectDialog.open()
                    }
                }

                RowLayout {
                    spacing: 8

                    Label {
                        text: qsTr("Ubicación de salida:")
                        Layout.minimumWidth: 90
                        Layout.preferredWidth: Math.max(90, Math.min(160, root.width * 0.22))
                    }

                    TextField {
                        id: outputField
                        text: root.outputQgzPath
                        readOnly: true
                        Layout.fillWidth: true
                        placeholderText: qsTr("Ninguno seleccionado")
                    }

                    Button {
                        text: qsTr("Seleccionar…")
                        enabled: !root.qgisMgr.busy
                        onClicked: qgzSaveDialog.open()
                    }
                }

                RowLayout {
                    spacing: 8

                    Label {
                        text: qsTr("Nombre del proyecto:")
                        Layout.minimumWidth: 90
                        Layout.preferredWidth: Math.max(90, Math.min(160, root.width * 0.22))
                    }

                    TextField {
                        id: projectNameField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Se usará el nombre del archivo si se deja vacío")
                        enabled: !root.qgisMgr.busy
                    }
                }
            }
        }

        // Botón de generación
        Button {
            id: generateBtn
            text: root.qgisMgr.busy ? qsTr("Generando…") : qsTr("Generar proyecto .qgz")
            enabled: root.sourceGpkgPath.length > 0 &&
                     root.outputQgzPath.length > 0 &&
                     !root.qgisMgr.busy
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            font.pixelSize: 14

            onClicked: {
                resultLabel.color = Material.foreground
                resultLabel.text = qsTr("Procesando…")
                root.qgisMgr.generate(
                    root.sourceGpkgPath,
                    root.outputQgzPath,
                    projectNameField.text.trim()
                )
            }
        }

        // Panel de resultados
        Rectangle {
            id: resultContainer
            Layout.fillWidth: true
            Layout.minimumHeight: 60
            visible: resultLabel.text.length > 0
            color: resultLabel.color === Material.color(Material.Red) ?
                   Material.backgroundColor : Material.primary.withAlpha(0.1)
            border.color: resultLabel.color
            border.width: 1
            radius: 4

            Label {
                id: resultLabel
                anchors.fill: parent
                anchors.margins: 12
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignVCenter
                color: Material.foreground
            }
        }

        // (sin espaciador: height lo controla el ScrollView)
    }

    // =========================================================================
    // Listeners para operaciones asincrónicas (v0.2.0+)
    // =========================================================================
    Connections {
        target: root.qgisMgr

        // Cuando la generación termina exitosamente
        function onGenerationSucceeded(path) {
            resultLabel.color = Material.color(Material.Green)
            resultLabel.text = qsTr("✓ Proyecto generado exitosamente:\n") + path
            // Opcional: limpiar campos después de un delay
            clearFieldsTimer.start()
        }

        // Cuando hay error
        function onLastErrorChanged() {
            if (root.qgisMgr.lastError.length > 0) {
                resultLabel.color = Material.color(Material.Red)
                resultLabel.text = qsTr("✗ Error: ") + root.qgisMgr.lastError
            }
        }

        // Cuando cambia el estado de ocupado (operación iniciada/finalizada)
        function onBusyChanged() {
            if (!root.qgisMgr.busy) {
                // Operación finalizada (éxito o error)
                // resultLabel ya debería estar actualizado vía signals
            }
        }
    }

    // Timer para limpiar campos después de éxito
    Timer {
        id: clearFieldsTimer
        interval: 3000 // 3 segundos
        onTriggered: {
            // Opcional: descomenta si quieres limpiar campos automáticamente
            // root.outputQgzPath = ""
            // projectNameField.text = ""
        }
    }
}