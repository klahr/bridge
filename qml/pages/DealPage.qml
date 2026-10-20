import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"
import "../components/Suits.js" as Suits
import "../cards/cards.js" as Cards

// The deal just played: all four hands as dealt, the auction and the result.
Page {
    id: page

    readonly property color red: Theme.colorScheme === Theme.LightOnDark ? "#ff6e6e" : "#c62828"
    readonly property var deal: game.lastDeal
    readonly property var auction: game.auction
    property string note: ""

    // A hand as four lines, spades first, each from the ace down.
    function handText(ids) {
        var lines = []
        var order = [3, 2, 1, 0]
        for (var k = 0; k < 4; ++k) {
            var s = order[k]
            var ranks = []
            var sorted = ids.filter(function(id) { return Cards.suit(id) === s })
            sorted.sort(function(a, b) { return Cards.high(b) - Cards.high(a) })
            for (var i = 0; i < sorted.length; ++i)
                ranks.push(Cards.rankText(sorted[i]))
            lines.push(Suits.colored(Cards.suits[s], red) + " " + (ranks.length ? ranks.join(" ") : "–"))
        }
        return lines.join("<br>")
    }

    function points(ids) {
        var total = 0
        for (var i = 0; i < ids.length; ++i) {
            var r = Cards.rank(ids[i])
            total += r === 1 ? 4 : r > 10 ? r - 10 : 0
        }
        return total
    }

    allowedOrientations: Orientation.Portrait

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: page.deal.passedOut === true || page.deal.declarer === undefined
                       ? qsTr("The deal")
                       : game.contractText(page.deal) + " " + qsTr("by %1").arg(game.playerName(page.deal.declarer))
                description: page.deal.result === undefined ? ""
                             : page.deal.result < 0 ? qsTr("Down %n", "", -page.deal.result)
                             : page.deal.result === 0 ? qsTr("Made") : qsTr("Made with %n overtrick(s)", "", page.deal.result)
            }

            // The four hands round the table, north at the top.
            Item {
                id: diagram
                readonly property real handHeight: nameMetrics.height + 4 * Math.ceil(lineMetrics.height) + Theme.paddingMedium
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                height: 3 * handHeight

                FontMetrics {
                    id: lineMetrics
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamilyHeading
                }
                FontMetrics {
                    id: nameMetrics
                    font.pixelSize: Theme.fontSizeExtraSmall
                }

                Repeater {
                    model: 4
                    Column {
                        id: handColumn
                        readonly property var ids: game.dealt[index]
                        x: index === 1 ? 0 : index === 3 ? diagram.width - width : (diagram.width - width) / 2
                        y: index === 2 ? 0 : index === 0 ? 2 * diagram.handHeight : diagram.handHeight
                        width: diagram.width / 3

                        Label {
                            text: game.playerName(index) + " · " + qsTr("%1 points").arg(page.points(handColumn.ids))
                            color: Cards.playerColor(index)
                            font.pixelSize: Theme.fontSizeExtraSmall
                            truncationMode: TruncationMode.Fade
                            width: parent.width
                        }
                        Label {
                            textFormat: Text.StyledText
                            text: page.handText(handColumn.ids)
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeSmall
                            font.family: Theme.fontFamilyHeading
                        }
                    }
                }
            }

            SectionHeader { text: qsTr("Auction") }

            AuctionTable {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                height: Math.ceil((Suits.column(game.dealer) + page.auction.length) / 4) * cellHeight + cellHeight + Theme.paddingMedium
                calls: page.auction
                dealer: game.dealer
                onCallClicked: {
                    selectedIndex = index
                    page.note = game.playerName(page.auction[index].player) + " "
                                + Suits.colored(game.callText(page.auction[index].call), page.red) + ": " + game.explain(index)
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                textFormat: Text.StyledText
                text: page.note.length ? page.note : qsTr("Tap a call to see what it showed.")
                color: page.note.length ? Theme.highlightColor : Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeSmall
            }
        }
        VerticalScrollDecorator { }
    }
}
