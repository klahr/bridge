import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.bridge 1.0
import "../cards/cards.js" as Cards
import "../components/Suits.js" as Suits

CoverBackground {
    readonly property color red: Theme.colorScheme === Theme.LightOnDark ? "#ff6e6e" : "#c62828"

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingSmall

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Bridge")
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
        }

        Repeater {
            model: 2
            Item {
                width: parent.width
                height: scoreLabel.height
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: game.sideName(index) + (game.vulnerable[index] ? " ·" : "")
                    color: Cards.playerColor(index)
                }
                Label {
                    id: scoreLabel
                    anchors.right: parent.right
                    text: game.scores[index]
                    color: Cards.playerColor(index)
                    font.pixelSize: Theme.fontSizeLarge
                    font.family: Theme.fontFamilyHeading
                }
            }
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            textFormat: Text.StyledText
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
            text: {
                switch (game.phase) {
                case Bridge.Bidding:
                    return qsTr("Bidding")
                case Bridge.Play:
                    return Suits.colored(game.contractText(game.contract), red) + " · "
                           + qsTr("%1 to %2").arg(game.tricks[0]).arg(game.tricks[1])
                case Bridge.Score:
                    return qsTr("Deal counted")
                }
                return game.winner === 0 ? qsTr("You won") : game.winner === 1 ? qsTr("They won") : qsTr("A tie")
            }
        }
    }

    CoverActionList {
        enabled: game.phase === Bridge.GameOver
        CoverAction {
            iconSource: "image://theme/icon-cover-new"
            onTriggered: game.newGame()
        }
    }
}
