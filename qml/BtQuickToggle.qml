// Bluetooth on/off toggle injected into the quick settings panel next to Airplane mode.
// Context properties: bt (QQmlPropertyMap). windowNavigator is copied from a sibling toggle.
import QtQuick
import xofm.libs.settings

QuickSettingsToggle {
    selected: !!bt.powered
    enabled: !!bt.available
    iconSource: "qrc:/rmppbt/icons/bluetooth.svg"
    Component.onCompleted: bt.command = "active:1#" + Date.now()
    Component.onDestruction: bt.command = "active:0#" + Date.now()
    onClicked: bt.command = (bt.powered ? "power:0" : "power:1") + "#" + Date.now()
    onPressAndHold: {
        windowNavigator?.open("settings/window/main", { index: 1000 });
        windowNavigator?.close("settings/window/quicksettings");
    }
}
