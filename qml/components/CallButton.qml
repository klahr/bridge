import QtQuick 2.6
import Sailfish.Silica 1.0

// A key of the bidding box.
BackgroundItem {
    id: root

    property alias text: label.text
    property bool selected
    // The call the bots suggest.
    property bool suggested

    height: Theme.itemSizeExtraSmall
    highlighted: down || selected
    opacity: enabled ? 1 : 0.35

    Rectangle {
        anchors { fill: parent; margins: Theme.paddingSmall / 2 }
        radius: Theme.paddingSmall
        color: root.suggested ? Theme.rgba(Theme.highlightColor, 0.2) : "transparent"
        border.color: root.selected || root.suggested ? Theme.highlightColor : Theme.rgba(Theme.primaryColor, 0.3)
        border.width: Math.max(1, Math.round(Theme.pixelRatio * (root.suggested ? 2 : 1)))
    }

    Label {
        id: label
        anchors.centerIn: parent
        textFormat: Text.StyledText
        color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
        font.pixelSize: Theme.fontSizeMedium
        font.family: Theme.fontFamilyHeading
    }
}
