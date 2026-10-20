import QtQuick 2.6
import Sailfish.Silica 1.0
import "../cards/cards.js" as Cards

Page {
    id: page

    // A player's name in their colour.
    function named(player) { return "<font color=\"" + Cards.playerColor(player) + "\">" + game.playerName(player) + "</font>" }

    allowedOrientations: Orientation.Portrait

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("Bridge") }

            Repeater {
                model: [
                    { title: qsTr("The game"), text: qsTr("Four players in two partnerships: you and %1 across the table against %2 and %3. Each deal has two parts: the bidding, where the sides compete to name the contract, and the play, where the declaring side tries to make it.").arg(page.named(2)).arg(page.named(1)).arg(page.named(3)) },
                    { title: qsTr("The bidding"), text: qsTr("The dealer calls first, then the others in turn clockwise. A bid names a number of tricks over six, from one to seven, and a trump suit or no trumps; each bid must be higher than the last, clubs lowest, then diamonds, hearts, spades and no trumps. Instead of bidding, a player may pass, double an opponent's bid or redouble a double. Three passes in a row end the bidding, and the last bid is the contract. Should all four pass at once, the cards are dealt again.") },
                    { title: qsTr("The play"), text: qsTr("Of the side that won the contract, whoever first named its strain is the declarer. The player to the declarer's left leads, and then the declarer's partner, the dummy, lays their cards face up; the declarer plays them too. Players must follow suit if they can; if not, they may trump or throw any card. The highest trump takes the trick, or else the highest card of the suit led, and its taker leads the next.") },
                    { title: qsTr("Scoring"), text: qsTr("A contract made scores 20 a trick over six in clubs and diamonds, 30 in hearts and spades, and at no trumps 40 for the first and 30 for each after. 100 such points make a game, in one deal or over several. Overtricks, small slams (twelve tricks), grand slams (all thirteen) and holding four or five trump honours, or all four aces at no trumps, all score bonuses. When a contract fails, the defenders score for each trick it falls short. Doubling raises the stakes, and a vulnerable side wins and loses more.") },
                    { title: qsTr("Rubber and Chicago"), text: qsTr("In rubber bridge, a side that has won a game is vulnerable, and the first to win two games wins the rubber: 700 if the other side won none, 500 if they won one. The higher total wins. Chicago is four deals: nobody is vulnerable on the first, the dealer's side on the second and third, and everyone on the fourth. Every game scores 300, or 500 vulnerable, and a partscore on the last deal 100.") },
                    { title: qsTr("Bidding systems"), text: qsTr("The bots bid by a system, a set of agreements about what each call shows, and read everyone's calls by it. Choose one for your side and one for theirs in the settings: Standard American, two over one, Acol, Precision or Enkel modern standard. Tap any call in the auction to see what it shows; calls that do not mean what they say are underlined. The bidding systems page lists the openings and the main answers.") },
                    { title: qsTr("Playing here"), text: qsTr("%1 sit at the bottom, %2 across, with %3 to your left and %4 to your right. To bid, choose a level and then a suit; press and hold a call to see what it would show, or tap ? for the bots' suggestion. When your side declares, you play dummy's cards as well as your own; tap a card to play it, and cards you may not play are dimmed. Review a deal afterwards to see all four hands and what every call meant.").arg(page.named(0)).arg(page.named(2)).arg(page.named(1)).arg(page.named(3)) }
                ]
                Column {
                    width: column.width
                    SectionHeader { text: modelData.title }
                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        wrapMode: Text.Wrap
                        textFormat: Text.StyledText
                        text: modelData.text
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }

            SectionHeader { text: qsTr("About") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                text: qsTr("Bridge %1 for Sailfish OS.").arg(Qt.application.version)
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeSmall
            }
        }
        VerticalScrollDecorator { }
    }
}
