import QtQuick 2.6
import Sailfish.Silica 1.0
import "../cards/cards.js" as Cards
import "Suits.js" as Suits

// The calls so far, a column for each player from west round to south.
// Tap a call to hear what it means.
Item {
    id: root

    property var calls: []
    property int dealer: 0
    // Whose call it is, marked in the next free place; -1 for nobody.
    property int turn: -1
    property int selectedIndex: -1
    property color red: Theme.colorScheme === Theme.LightOnDark ? "#ff6e6e" : "#c62828"
    readonly property real cellWidth: width / 4
    readonly property real cellHeight: Math.round(Theme.itemSizeExtraSmall * 0.7)
    readonly property int offset: Suits.column(dealer)
    readonly property int cells: offset + calls.length + (turn >= 0 ? 1 : 0)

    signal callClicked(int index)

    Row {
        id: header
        width: parent.width
        Repeater {
            model: [1, 2, 3, 0]
            Label {
                width: root.cellWidth
                horizontalAlignment: Text.AlignHCenter
                text: game.playerName(modelData)
                color: Cards.playerColor(modelData)
                font.pixelSize: Theme.fontSizeExtraSmall
                truncationMode: TruncationMode.Fade
            }
        }
    }

    Rectangle {
        anchors.top: header.bottom
        width: parent.width
        height: Math.max(1, Math.round(Theme.pixelRatio))
        color: Theme.rgba(Theme.primaryColor, 0.2)
    }

    Flickable {
        id: flick
        anchors { top: header.bottom; topMargin: Theme.paddingSmall; left: parent.left; right: parent.right; bottom: parent.bottom }
        clip: true
        contentHeight: grid.height
        interactive: contentHeight > height
        // The latest calls stay in view.
        onContentHeightChanged: contentY = Math.max(0, contentHeight - height)
        onHeightChanged: contentY = Math.max(0, contentHeight - height)

        Grid {
            id: grid
            columns: 4

            Repeater {
                model: root.cells
                Item {
                    readonly property int callIndex: index - root.offset
                    readonly property bool isCall: callIndex >= 0 && callIndex < root.calls.length
                    readonly property var entry: isCall ? root.calls[callIndex] : null

                    width: root.cellWidth
                    height: root.cellHeight

                    Rectangle {
                        anchors { fill: parent; margins: Theme.paddingSmall / 2 }
                        radius: Theme.paddingSmall
                        color: Theme.rgba(Theme.highlightBackgroundColor, Theme.highlightBackgroundOpacity)
                        visible: parent.isCall ? callIndex === root.selectedIndex : callIndex >= 0 && root.turn >= 0
                    }
                    Label {
                        anchors.centerIn: parent
                        textFormat: Text.StyledText
                        text: parent.isCall ? Suits.colored(game.callText(parent.entry.call), root.red)
                                            : callIndex >= 0 ? "?" : ""
                        color: parent.isCall ? Cards.playerColor(parent.entry.player) : Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeMedium
                        font.family: Theme.fontFamilyHeading
                        font.bold: parent.isCall && parent.entry.alert
                    }
                    // Alerted: the call does not mean what it says.
                    Rectangle {
                        visible: parent.isCall && parent.entry.alert
                        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: Theme.paddingSmall / 2 }
                        width: parent.width * 0.4
                        height: Math.max(2, Math.round(Theme.pixelRatio * 2))
                        radius: height / 2
                        color: Theme.highlightColor
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: parent.isCall
                        onClicked: root.callClicked(parent.callIndex)
                    }
                }
            }
        }
    }
}
