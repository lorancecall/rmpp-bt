// Bluetooth settings page, shown over the Settings payload area when its sidebar row is selected.
// Context properties: bt (QQmlPropertyMap), settingsRoot, payloadLoaderRef.
import QtQuick
import QtQuick.Layouts
import ark.tokens as ArkTokens
import ark.controls as ArkControls

Rectangle {
    id: page
    readonly property int btPage: 1000
    readonly property var devices: {
        try { return JSON.parse(bt.devices || "[]"); } catch (e) { return []; }
    }
    readonly property var paired: devices.filter(d => d.paired)
    readonly property var available: devices.filter(d => !d.paired && d.name)

    function send(cmd) { bt.command = cmd + "#" + Date.now(); }

    x: payloadLoaderRef.x
    y: payloadLoaderRef.y
    width: payloadLoaderRef.width
    height: payloadLoaderRef.height
    z: 10
    color: "white"
    visible: settingsRoot._selectedPage === btPage
    onVisibleChanged: send(visible ? "active:1" : "active:0")

    ArkControls.PlaceholderBar {
        id: placeholder
        anchors { top: parent.top; left: parent.left; right: parent.right }
    }

    Flickable {
        anchors {
            top: placeholder.bottom; left: parent.left; right: parent.right; bottom: parent.bottom
            leftMargin: ArkControls.Values.margin.horizontal.large
            rightMargin: ArkControls.Values.margin.horizontal.large
        }
        contentHeight: column.implicitHeight + ArkControls.Values.margin.vertical.large
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: column
            width: parent.width
            spacing: ArkControls.Values.spacing.large

            ArkControls.Title {
                type: ArkControls.Title.Large
                text: "Bluetooth settings"
                Layout.fillWidth: true
            }

            ArkControls.Panel {
                label: "Bluetooth"
                description: bt.available ? (bt.powered ? "On" : "Off") : "Bluetooth adapter not found"
                iconSource: "qrc:/rmppbt/icons/bluetooth.svg"
                Layout.fillWidth: true
                action: ArkControls.Toggle {
                    checked: !!bt.powered
                    enabled: !!bt.available
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -20
                        onClicked: page.send(bt.powered ? "power:0" : "power:1")
                    }
                }
            }

            ArkControls.Panel {
                visible: !!bt.passkey
                label: "Type " + bt.passkey + " on " + (bt.pairingName || "the keyboard") + ", then press Enter"
                Layout.fillWidth: true
            }

            ArkControls.Panel {
                visible: !!bt.status && !bt.passkey
                label: bt.status || ""
                Layout.fillWidth: true
            }

            ArkControls.Title {
                visible: !!bt.powered
                type: ArkControls.Title.Medium
                text: "My devices"
                Layout.fillWidth: true
            }

            Repeater {
                model: bt.powered ? page.paired : []
                delegate: ArkControls.Panel {
                    required property var modelData
                    label: modelData.name || modelData.address
                    description: modelData.connected ? "Connected" : "Not connected"
                    Layout.fillWidth: true
                    action: RowLayout {
                        spacing: ArkControls.Values.spacing.medium
                        ArkControls.Button {
                            text: modelData.connected ? "Disconnect" : "Connect"
                            type: ArkTokens.Button.secondary
                            onClicked: page.send((modelData.connected ? "disconnect:" : "connect:") + modelData.path)
                        }
                        ArkControls.Button {
                            text: "Forget"
                            type: ArkTokens.Button.secondary
                            onClicked: page.send("remove:" + modelData.path)
                        }
                    }
                }
            }

            RowLayout {
                visible: !!bt.powered
                Layout.fillWidth: true
                ArkControls.Title {
                    type: ArkControls.Title.Medium
                    text: bt.discovering ? "Searching…" : "Other devices"
                    Layout.fillWidth: true
                }
                ArkControls.Button {
                    text: bt.discovering ? "Stop" : "Search"
                    type: ArkTokens.Button.secondary
                    onClicked: page.send(bt.discovering ? "scan:0" : "scan:1")
                }
            }

            Repeater {
                model: bt.powered ? page.available : []
                delegate: ArkControls.Panel {
                    required property var modelData
                    label: modelData.name
                    description: modelData.address
                    Layout.fillWidth: true
                    action: ArkControls.Button {
                        text: "Pair"
                        type: ArkTokens.Button.secondary
                        onClicked: page.send("pair:" + modelData.path)
                    }
                }
            }
        }
    }
}
