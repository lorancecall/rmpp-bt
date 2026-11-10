// Sidebar row injected under "Wi-Fi" in the Settings page.
// Context properties: bt (QQmlPropertyMap), settingsRoot (Settings.qml root item).
import QtQuick
import QtQuick.Layouts
import ark.tokens as ArkTokens
import ark.controls as ArkControls

ArkControls.SidebarItem {
    readonly property int btPage: 1000

    type: ArkTokens.Sidebar.primary
    text: settingsRoot.showLabels ? "Bluetooth" : ""
    iconSource: "qrc:/rmppbt/icons/bluetooth.svg"
    highlighted: settingsRoot._selectedPage === btPage
    focusPolicy: Qt.StrongFocus
    Layout.fillWidth: true

    onTriggered: settingsRoot._selectedPage = btPage
}
