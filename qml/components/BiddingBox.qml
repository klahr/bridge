import QtQuick 2.6
import Sailfish.Silica 1.0
import "../cards"
import "Suits.js" as Suits

// Choose a level, then a strain; or pass, double or redouble. Press and
// hold a call to see what it would mean.
Column {
    id: root

    property bool active
    property int minLevel: 1
    property bool canDouble
    property bool canRedouble
    property int level: 1
    // The call the bots suggest, -1 for none.
    property int suggestion: -1
    property color red: Theme.colorScheme === Theme.LightOnDark ? "#ff6e6e" : "#c62828"
    // Changes with every call, so legality is checked again.
    property int calls: 0

    signal call(int callId)
    signal explain(int callId)
    signal hint()

    function reset() { level = Math.max(1, Math.min(7, minLevel)) }

    onMinLevelChanged: reset()
    onSuggestionChanged: if (suggestion >= 3) level = Suits.levelOf(suggestion)

    Row {
        Repeater {
            model: 7
            CallButton {
                width: root.width / 7
                text: index + 1
                enabled: root.active && index + 1 >= root.minLevel
                selected: root.level === index + 1
                onClicked: root.level = index + 1
            }
        }
    }

    Row {
        Repeater {
            model: 5
            CallButton {
                readonly property int callId: Suits.bidId(root.level, index)

                width: root.width / 5
                text: Suits.colored(game.callText(callId), root.red)
                enabled: root.active && root.calls >= 0 && game.canCall(callId)
                suggested: root.suggestion === callId
                onClicked: root.call(callId)
                onPressAndHold: root.explain(callId)
            }
        }
    }

    Row {
        CallButton {
            width: root.width * 0.35
            text: game.callText(0)
            enabled: root.active
            suggested: root.suggestion === 0
            onClicked: root.call(0)
            onPressAndHold: root.explain(0)
        }
        CallButton {
            width: root.width * 0.2
            text: game.callText(1)
            enabled: root.active && root.canDouble
            suggested: root.suggestion === 1
            onClicked: root.call(1)
            onPressAndHold: root.explain(1)
        }
        CallButton {
            width: root.width * 0.2
            text: game.callText(2)
            enabled: root.active && root.canRedouble
            suggested: root.suggestion === 2
            onClicked: root.call(2)
            onPressAndHold: root.explain(2)
        }
        CallButton {
            width: root.width * 0.25
            text: "?"
            enabled: root.active
            onClicked: root.hint()
        }
    }
}
