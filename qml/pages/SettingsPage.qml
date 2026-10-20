import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page

    allowedOrientations: Orientation.Portrait

    RemorsePopup { id: remorse }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            PageHeader { title: qsTr("Settings") }

            ComboBox {
                label: qsTr("Bots")
                currentIndex: game.difficulty
                description: [
                    qsTr("Play by rules of thumb: second hand low, third hand high, leads at random."),
                    qsTr("Draw trumps, cash winners, ruff in dummy and lead partner's suit."),
                    qsTr("Read the bidding and the play, and think every card through over many guessed deals.")
                ][currentIndex] + " " + qsTr("Everyone bids by the system the same way. Takes effect in the next game.")
                menu: ContextMenu {
                    MenuItem { text: qsTr("Easy") }
                    MenuItem { text: qsTr("Medium") }
                    MenuItem { text: qsTr("Hard") }
                }
                onCurrentIndexChanged: game.difficulty = currentIndex
            }

            ComboBox {
                label: qsTr("Scoring")
                currentIndex: game.scoring
                description: [
                    qsTr("Rubber bridge: the first side to win two games wins the rubber and a bonus."),
                    qsTr("Chicago: four deals, the vulnerability set for each, and a bonus for every game.")
                ][currentIndex] + " " + qsTr("Takes effect in the next game.")
                menu: ContextMenu {
                    MenuItem { text: qsTr("Rubber") }
                    MenuItem { text: qsTr("Chicago") }
                }
                onCurrentIndexChanged: game.scoring = currentIndex
            }

            ComboBox {
                label: qsTr("Our system")
                currentIndex: game.system
                description: game.systemDescription(currentIndex) + " " + qsTr("You and North bid it. Takes effect in the next game.")
                menu: ContextMenu {
                    MenuItem { text: game.systemName(0) }
                    MenuItem { text: game.systemName(1) }
                    MenuItem { text: game.systemName(2) }
                    MenuItem { text: game.systemName(3) }
                    MenuItem { text: game.systemName(4) }
                }
                onCurrentIndexChanged: game.system = currentIndex
            }

            ComboBox {
                label: qsTr("Their system")
                currentIndex: game.theirSystem
                description: game.systemDescription(currentIndex) + " " + qsTr("West and East bid it. Takes effect in the next game.")
                menu: ContextMenu {
                    MenuItem { text: game.systemName(0) }
                    MenuItem { text: game.systemName(1) }
                    MenuItem { text: game.systemName(2) }
                    MenuItem { text: game.systemName(3) }
                    MenuItem { text: game.systemName(4) }
                }
                onCurrentIndexChanged: game.theirSystem = currentIndex
            }

            ComboBox {
                label: qsTr("Play speed")
                currentIndex: game.speed
                description: qsTr("How long the bots take to call and play, and a finished trick stays on the table.")
                menu: ContextMenu {
                    MenuItem { text: qsTr("Slow") }
                    MenuItem { text: qsTr("Normal") }
                    MenuItem { text: qsTr("Fast") }
                    MenuItem { text: qsTr("Very fast") }
                }
                onCurrentIndexChanged: game.speed = currentIndex
            }

            SectionHeader { text: qsTr("Statistics") }

            Repeater {
                model: game.stats
                DetailItem {
                    label: [qsTr("Easy"), qsTr("Medium"), qsTr("Hard")][index]
                    value: qsTr("%1 won, %2 lost").arg(modelData.won).arg(modelData.lost)
                           + (modelData.made + modelData.down > 0
                              ? " · " + qsTr("%1 of %2 contracts made").arg(modelData.made).arg(modelData.made + modelData.down)
                              : "")
                }
            }

            Item { width: 1; height: Theme.paddingLarge }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Reset statistics")
                onClicked: remorse.execute(qsTr("Resetting statistics"), function() { game.resetStats() })
            }
        }
        VerticalScrollDecorator { }
    }
}
