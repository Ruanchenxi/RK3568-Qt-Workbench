import QtQuick 2.12
import QtQuick.VirtualKeyboard 2.15

Item {
    id: root
    property bool manualEnabled: false
    property bool keyboardVisible: manualEnabled && Qt.inputMethod.visible
    property int panelHeight: keyboardVisible
                              ? Math.max(Math.round(Qt.inputMethod.keyboardRectangle.height), 280)
                              : 0

    width: parent ? parent.width : 1280
    height: panelHeight

    InputPanel {
        id: inputPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.manualEnabled
    }
}
