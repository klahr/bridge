import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components/Suits.js" as Suits
import "../cards/cards.js" as Cards

// Every deal of the rubber or the Chicago so far, and what it scored.
Page {
    id: page

    readonly property color red: Theme.colorScheme === Theme.LightOnDark ? "#ff6e6e" : "#c62828"
    readonly property real numberWidth: Theme.itemSizeExtraSmall * 0.7
    readonly property real pointsWidth: (width - 2 * Theme.horizontalPageMargin) * 0.18

    function resultText(deal) {
        if (deal.passedOut)
            return qsTr("passed out")
        if (deal.result === 0)
            return qsTr("made")
        return deal.result > 0 ? "+" + deal.result : "−" + (-deal.result)
    }

    allowedOrientations: Orientation.Portrait

    SilicaListView {
        id: list
        anchors.fill: parent
        model: game.deals

        header: Column {
            width: list.width

            PageHeader { title: qsTr("Score sheet") }

            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                Repeater {
                    model: 2
                    Column {
                        width: parent.width / 2
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: game.sideName(index)
                            color: Cards.playerColor(index)
                        }
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: game.scores[index]
                            color: Cards.playerColor(index)
                            font.pixelSize: Theme.fontSizeHuge
                            font.family: Theme.fontFamilyHeading
                        }
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: (game.gameScoring === 0 ? qsTr("%n game(s)", "", game.games[index]) + " · " : "")
                                  + qsTr("%1 towards game").arg(game.partScores[index])
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                        }
                    }
                }
            }

            Item { width: 1; height: Theme.paddingLarge }

            Item {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                height: Theme.itemSizeExtraSmall * 0.6
                Label {
                    text: qsTr("Contract")
                    x: page.numberWidth
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: page.pointsWidth
                    width: page.pointsWidth
                    horizontalAlignment: Text.AlignRight
                    text: game.sideName(0)
                    color: Cards.playerColor(0)
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
                Label {
                    anchors.right: parent.right
                    width: page.pointsWidth
                    horizontalAlignment: Text.AlignRight
                    text: game.sideName(1)
                    color: Cards.playerColor(1)
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
            }
        }

        delegate: Item {
            // The inner repeater has a modelData of its own.
            readonly property var deal: modelData

            width: list.width
            height: Theme.itemSizeSmall

            Item {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                height: parent.height

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    width: page.numberWidth
                    text: deal.number
                    color: Theme.secondaryColor
                }
                Column {
                    x: page.numberWidth
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - page.numberWidth - 2 * page.pointsWidth
                    Label {
                        width: parent.width
                        textFormat: Text.StyledText
                        truncationMode: TruncationMode.Fade
                        text: deal.passedOut ? qsTr("Passed out")
                                                  : qsTr("%1 by %2").arg(Suits.colored(game.contractText(deal), page.red))
                                                    .arg(game.playerName(deal.declarer))
                        color: Theme.highlightColor
                    }
                    Label {
                        width: parent.width
                        truncationMode: TruncationMode.Fade
                        text: page.resultText(deal)
                              + (deal.rubberWon >= 0 ? " · " + qsTr("rubber")
                                 : deal.gameWon >= 0 ? " · " + qsTr("game") : "")
                              + (deal.honorsPoints > 0 ? " · " + qsTr("honours %1").arg(deal.honorsPoints) : "")
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }
                Repeater {
                    model: 2
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: index === 0 ? page.pointsWidth : 0
                        width: page.pointsWidth
                        horizontalAlignment: Text.AlignRight
                        text: deal.points !== undefined && deal.points[index] > 0 ? deal.points[index] : "–"
                        color: Cards.playerColor(index)
                        font.pixelSize: Theme.fontSizeLarge
                        font.family: Theme.fontFamilyHeading
                    }
                }
            }

            // A line under each game won.
            Rectangle {
                visible: deal.gameWon >= 0
                anchors.bottom: parent.bottom
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                height: Math.max(1, Math.round(Theme.pixelRatio))
                color: Theme.rgba(Theme.highlightColor, 0.5)
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: qsTr("No deals played yet")
        }

        VerticalScrollDecorator { }
    }
}
