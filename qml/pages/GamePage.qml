import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.bridge 1.0
import "../components"
import "../components/Suits.js" as Suits
import "../cards"
import "../cards/cards.js" as Cards
import "../online"

Page {
    id: page

    // One card size everywhere: six overlapping cards fill the width, seven squeeze in.
    readonly property real cardWidth: Math.floor((width - 2 * Theme.horizontalPageMargin) / (1 + 5 * 0.76))
    readonly property real cardHeight: Math.round(cardWidth * 1.42)
    readonly property real margin: Theme.horizontalPageMargin
    // Your hand lies in two rows while it holds more than seven cards.
    readonly property real handRowStep: Math.round(cardHeight * 0.5)
    // How much of the side players' cards shows at the edge of the screen.
    readonly property real sideShown: Math.round(cardHeight * 0.42)
    readonly property color red: Theme.colorScheme === Theme.LightOnDark ? "#ff6e6e" : "#c62828"

    readonly property bool bidding: game.phase === Bridge.Bidding
    readonly property bool playing: game.phase === Bridge.Play
    readonly property bool scoring: game.phase === Bridge.Score || game.phase === Bridge.GameOver
    readonly property bool hasContract: game.contract.level !== undefined
    // North's cards are face up once you play them: you declare, or north
    // does and dummy is yours.
    readonly property bool northUp: playing && (game.declarer === 2 || (game.dummy === 2 && game.dummyVisible))
    // West or east as dummy, face up at the side.
    readonly property int sideDummy: playing && game.dummyVisible && (game.dummy === 1 || game.dummy === 3) ? game.dummy : -1

    // The call whose meaning is shown, and that meaning.
    property int explained: -1
    property string note: ""
    property int suggestion: -1

    readonly property var places: computePlaces()
    // Holding the table: the last trick shows over this one.
    readonly property bool peeking: peek.peeking && game.lastTrick.length > 0

    function lastTrickWinner() {
        var last = game.lastTrick
        for (var i = 0; i < last.length; ++i)
            if (last[i].won)
                return last[i].player
        return -1
    }

    allowedOrientations: Orientation.Portrait

    function startNewGame() {
        if (game.phase === Bridge.GameOver || (game.deals.length === 0 && bidding && game.auction.length === 0))
            game.newGame()
        else
            remorse.execute(qsTr("Starting a new game"), function() { game.newGame() })
    }

    // A player's name, or a line meant for you, in the player's colour.
    // Grey and faint for a player who has lost the connection, until a bot plays for them.
    function seatColor(player) { return online.seats.length && online.away(online.toTable(player)) ? Cards.awayColor(player) : Cards.playerColor(player) }
    function coloured(player, text) { return "<font color=\"" + seatColor(player) + "\">" + text + "</font>" }
    function named(player) { return coloured(player, game.playerName(player)) }
    function richCall(callId) { return Suits.colored(game.callText(callId), red) }
    function richContract(contract) { return Suits.colored(game.contractText(contract), red) }

    function showCall(index) {
        var calls = game.auction
        if (index < 0 || index >= calls.length) {
            explained = -1
            note = ""
            return
        }
        explained = index
        note = named(calls[index].player) + " " + richCall(calls[index].call) + ": " + Suits.colored(game.explain(index), red)
    }

    // Where each player's card goes on the table.
    function trickSpot(player) {
        var x = (page.width - cardWidth) / 2
        var top = cluster.y + tableRow.y
        var middle = top + (tableRow.height - cardHeight) / 2
        switch (player) {
        case 1: return { x: x - cardWidth * 1.1, y: middle }
        case 2: return { x: x, y: top }
        case 3: return { x: x + cardWidth * 1.1, y: middle }
        }
        return { x: x, y: top + tableRow.height - cardHeight }
    }

    // Where a player's tricks go, out of sight.
    function seatSpot(player) {
        var w = page.width
        var middle = cluster.y + tableRow.y + (tableRow.height - cardHeight) / 2
        switch (player) {
        case 1: return { x: -cardWidth, y: middle }
        case 2: return { x: (w - cardWidth) / 2, y: cluster.y + northRow.y - cardHeight / 2 }
        case 3: return { x: w, y: middle }
        }
        return { x: (w - cardWidth) / 2, y: cluster.y + handRow.y + Theme.paddingLarge }
    }

    // Where every one of the 52 cards lies, in content coordinates.
    function computePlaces() {
        var cw = cardWidth
        var ch = cardHeight
        var w = page.width
        var hands = game.hands
        var handY = cluster.y + handRow.y + Theme.paddingLarge
        var northY = cluster.y + northRow.y
        var tableY = cluster.y + tableRow.y
        // Cards fly out one after the other as they are dealt.
        var dealing = bidding && game.auction.length === 0
        var p = []
        var i, k, s, n, off, x0, ids, spot

        // Cards out of play are hidden in the middle, and dealt from there.
        for (i = 0; i < 52; ++i)
            p.push({ x: (w - cw) / 2, y: tableY, z: 1, down: true, rot: 0, role: "deck", delay: 0, hidden: true })

        // Tricks taken go to the player who won them.
        var taken = game.taken
        for (s = 0; s < 4; ++s) {
            spot = seatSpot(s)
            ids = taken[s]
            for (i = 0; i < ids.length; ++i)
                p[ids[i]] = { x: spot.x, y: spot.y, z: 5, down: true, rot: 0, role: "taken", delay: 0, hidden: true }
        }

        // Your cards, centred, in two rows while there are many.
        ids = hands[0]
        n = ids.length
        var top = n > 7 ? Math.ceil(n / 2) : n
        off = top > 1 ? Math.min(cw * 0.76, (w - 2 * margin - cw) / (top - 1)) : 0
        for (i = 0; i < n; ++i) {
            var row = i < top ? 0 : 1
            var inRow = row === 0 ? top : n - top
            k = row === 0 ? i : i - top
            x0 = (w - cw - (inRow - 1) * off) / 2
            p[ids[i]] = { x: x0 + k * off, y: handY + row * handRowStep, z: 50 + i, down: false, rot: 0,
                          role: "hand", seat: 0, delay: dealing ? i * 30 : 0 }
        }

        // North across the top: face up in one row when it is yours to play.
        ids = hands[2]
        n = ids.length
        if (northUp) {
            off = n > 1 ? Math.min(cw * 0.76, (w - 2 * margin - cw) / (n - 1)) : 0
            x0 = (w - cw - (n - 1) * off) / 2
            for (i = 0; i < n; ++i)
                p[ids[i]] = { x: x0 + i * off, y: northY, z: 30 + i, down: false, rot: 0, role: "hand", seat: 2, delay: 0 }
        } else {
            off = n > 1 ? Math.min(cw * 0.22, (w - 2 * margin - cw) / (n - 1)) : 0
            x0 = (w - cw - (n - 1) * off) / 2
            for (i = 0; i < n; ++i)
                p[ids[i]] = { x: x0 + i * off, y: northY, z: 30 + i, down: true, rot: 0, role: "bot", delay: dealing ? i * 30 : 0 }
        }

        // West and east hold their cards sideways at the edges. A dummy
        // there lies face up, spread from north's row down so the corners
        // show; east's is turned the other way, its corners towards you,
        // and runs up from the bottom as east would hold it.
        for (s = 1; s <= 3; s += 2) {
            ids = hands[s]
            n = ids.length
            var open = s === sideDummy
            var turned = open && s === 3
            var span = open ? tableY + tableRow.height - northY : tableRow.height
            off = n > 1 ? Math.min(cw * (open ? 0.5 : 0.3), (span - cw) / (n - 1)) : 0
            var centreX = s === 1 ? sideShown - ch / 2 : w - sideShown + ch / 2
            var centreY = (open ? northY : tableY) + (span - (n - 1) * off) / 2
            for (i = 0; i < n; ++i)
                p[ids[i]] = { x: centreX - cw / 2, y: centreY + (turned ? n - 1 - i : i) * off - ch / 2, z: 30 + i,
                              down: !open, rot: turned ? -90 : 90, role: "bot", delay: dealing ? i * 30 : 0 }
        }

        // The trick, each card in front of whoever played it; the winning
        // card lights up once all four are down.
        var trick = game.trick
        for (i = 0; i < trick.length; ++i) {
            spot = trickSpot(trick[i].player)
            p[trick[i].card] = { x: spot.x, y: spot.y, z: 100 + i, down: false, rot: 0, role: "trick", delay: 0,
                                 glow: trick[i].player === game.trickWinner,
                                 glowColor: Cards.playerColor(trick[i].player) }
        }

        // Held down on the table: the last trick back up, over this one.
        if (peeking) {
            for (i = 0; i < trick.length; ++i)
                p[trick[i].card].hidden = true
            var last = game.lastTrick
            for (i = 0; i < last.length; ++i) {
                spot = trickSpot(last[i].player)
                p[last[i].card] = { x: spot.x, y: spot.y, z: 300 + i, down: false, rot: 0, role: "last", delay: 0,
                                    glow: last[i].won, glowColor: Cards.playerColor(last[i].player) }
            }
        }
        return p
    }

    Connections {
        target: game
        onChanged: {
            page.suggestion = -1
            if (page.bidding && game.auction.length > 0)
                page.showCall(game.auction.length - 1)
            else if (!page.bidding)
                page.showCall(-1)
            else
                page.note = ""
        }
    }

    RemorsePopup { id: remorse }

    ConnectionBanner { }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: Math.max(page.height, cluster.y + cluster.height + Theme.paddingLarge)

        PullDownMenu {
            MenuItem {
                text: qsTr("About and rules")
                onClicked: pageStack.push(Qt.resolvedUrl("RulesPage.qml"))
            }
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                text: qsTr("Bidding systems")
                onClicked: pageStack.push(Qt.resolvedUrl("SystemPage.qml"))
            }
            MenuItem {
                text: qsTr("Score sheet")
                onClicked: pageStack.push(Qt.resolvedUrl("ScorePage.qml"))
            }
            MenuItem {
                text: online.active ? qsTr("Online table") : qsTr("Play online")
                onClicked: pageStack.push(Qt.resolvedUrl(online.atTable ? "../online/TablePage.qml" : "../online/LobbyPage.qml"))
            }
            MenuItem {
                text: qsTr("New game")
                // Online, the game must end first.
                visible: !online.active || game.phase === Bridge.GameOver
                onClicked: page.startNewGame()
            }
        }

        Item {
            id: content
            width: parent.width
            height: parent.contentHeight

            // ---------------------------------------------------- board
            Item {
                id: board
                x: margin
                // Below the notch, on phones with one.
                y: Screen.topCutout.height + Theme.paddingLarge
                width: parent.width - 2 * margin
                height: Math.max(us.height, them.height)

                SideScore {
                    id: us
                    width: board.width * 0.3
                    name: game.sideName(0)
                    score: game.scores[0]
                    games: game.games[0]
                    partScore: game.partScores[0]
                    rubber: game.gameScoring === 0
                    vulnerable: game.vulnerable[0]
                    tricks: page.playing ? game.tricks[0] : -1
                    active: page.playing && game.turn >= 0 && game.turn % 2 === 0
                    color: Cards.playerColor(0)
                }

                // The deal: who deals and the contract.
                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: board.width * 0.4
                    spacing: Theme.paddingSmall / 2

                    Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: game.gameScoring === 1 ? qsTr("Deal %1 of 4").arg(Math.min(4, game.dealNumber))
                                                     : qsTr("Deal %1").arg(game.dealNumber)
                        color: Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                    Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        textFormat: Text.StyledText
                        text: page.hasContract ? page.richContract(game.contract) : page.bidding ? qsTr("Bidding") : qsTr("Passed out")
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeLarge
                        font.family: Theme.fontFamilyHeading
                    }
                    Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        textFormat: Text.StyledText
                        wrapMode: Text.Wrap
                        text: page.hasContract ? qsTr("by %1").arg(page.named(game.declarer))
                                               : qsTr("%1 deals").arg(page.named(game.dealer))
                        color: Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }

                SideScore {
                    id: them
                    anchors.right: parent.right
                    width: board.width * 0.3
                    name: game.sideName(1)
                    score: game.scores[1]
                    games: game.games[1]
                    partScore: game.partScores[1]
                    rubber: game.gameScoring === 0
                    vulnerable: game.vulnerable[1]
                    tricks: page.playing ? game.tricks[1] : -1
                    active: page.playing && game.turn >= 0 && game.turn % 2 === 1
                    color: Cards.playerColor(1)
                }
            }

            // Everything else sits together in the middle of the screen, so
            // your hand is within easy reach of the thumb.
            Column {
                id: cluster
                width: parent.width
                y: board.y + board.height
                   + Math.max(Theme.paddingLarge,
                              (page.height - board.y - board.height
                               - (height - info.height + messageBlock.height)) * 0.4)
                spacing: Theme.paddingMedium

                // North's cards (drawn by the card layer).
                Item {
                    id: northRow
                    width: parent.width
                    height: cardHeight
                }

                // The auction, or the trick with west and east at the sides.
                Item {
                    id: tableRow
                    width: parent.width
                    height: Math.round(cardHeight * 1.8)

                    AuctionTable {
                        x: sideShown + Theme.paddingSmall
                        width: parent.width - 2 * x
                        height: parent.height
                        opacity: page.bidding ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { FadeAnimation { } }
                        calls: game.auction
                        dealer: game.dealer
                        turn: page.bidding ? game.turn : -1
                        // The latest call is explained on its own; the mark goes to whoever calls next.
                        selectedIndex: page.bidding && page.explained === game.auction.length - 1 ? -1 : page.explained
                        onCallClicked: page.showCall(index)
                    }
                }

                // Messages, the count of the deal, or the result.
                Item {
                    id: info
                    width: parent.width
                    height: game.phase === Bridge.Score ? scorePanel.height
                                                        : game.phase === Bridge.GameOver ? overPanel.height
                                                                                         : messageBlock.height

                    Column {
                        id: messageBlock
                        x: margin
                        width: parent.width - 2 * margin
                        opacity: page.scoring ? 0 : 1
                        visible: opacity > 0
                        Behavior on opacity { FadeAnimation { } }

                        Label {
                            width: parent.width
                            height: fontMetrics.height
                            horizontalAlignment: Text.AlignHCenter
                            truncationMode: TruncationMode.Fade
                            textFormat: Text.StyledText
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeSmall
                            text: {
                                if (page.bidding) {
                                    if (game.humanTurn)
                                        return page.coloured(0, qsTr("Your call."))
                                    return qsTr("%1 is thinking…").arg(page.named(game.turn))
                                }
                                if (!page.playing)
                                    return ""
                                if (page.peeking)
                                    return page.lastTrickWinner() === 0 ? page.coloured(0, qsTr("You won the last trick."))
                                                                        : qsTr("%1 won the last trick.").arg(page.named(page.lastTrickWinner()))
                                if (game.trickWinner >= 0)
                                    return game.trickWinner === 0 ? page.coloured(0, qsTr("You win the trick."))
                                                                  : qsTr("%1 wins the trick.").arg(page.named(game.trickWinner))
                                if (game.humanTurn) {
                                    var lead = game.trick.length === 0
                                    if (game.turn === 2)
                                        return page.coloured(2, game.dummy === 2 ? (lead ? qsTr("Lead from dummy.") : qsTr("Play from dummy."))
                                                                                : (lead ? qsTr("Lead from North's hand.") : qsTr("Play from North's hand.")))
                                    if (game.tricksPlayed === 0 && lead && !game.dummyVisible)
                                        return page.coloured(0, qsTr("Your opening lead."))
                                    return page.coloured(0, lead ? qsTr("Your lead.") : qsTr("Your turn. Tap a card to play it."))
                                }
                                if (game.turn === game.dummy && game.dummyVisible)
                                    return qsTr("%1 plays from dummy…").arg(page.named(game.declarer))
                                return qsTr("%1 is playing…").arg(page.named(game.turn))
                            }
                        }

                        // What the last call, or the one tapped, means.
                        Label {
                            width: parent.width
                            height: Math.max(fontMetrics.height * 2, implicitHeight)
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            textFormat: Text.StyledText
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: page.note
                        }

                        FontMetrics {
                            id: fontMetrics
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }

                    Column {
                        id: scorePanel
                        readonly property var deal: game.lastDeal
                        readonly property bool passedOut: deal.passedOut === true

                        x: margin
                        width: parent.width - 2 * margin
                        opacity: game.phase === Bridge.Score ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { FadeAnimation { } }

                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            textFormat: Text.StyledText
                            wrapMode: Text.Wrap
                            text: scorePanel.passedOut ? qsTr("Passed out")
                                                       : qsTr("%1 by %2").arg(page.richContract(scorePanel.deal)).arg(page.named(scorePanel.deal.declarer))
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeLarge
                            font.family: Theme.fontFamilyHeading
                        }
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            text: {
                                var d = scorePanel.deal
                                if (scorePanel.passedOut)
                                    return game.gameScoring === 1 ? qsTr("Nobody bid, so the same dealer deals again.")
                                                                  : qsTr("Nobody bid; on to the next dealer.")
                                if (d.result === undefined)
                                    return ""
                                if (d.result < 0)
                                    return qsTr("Down %n, with %1 tricks.", "", -d.result).arg(d.tricks)
                                if (d.result === 0)
                                    return qsTr("Made, with %1 tricks.").arg(d.tricks)
                                return qsTr("Made with %n overtrick(s).", "", d.result)
                            }
                            color: Theme.secondaryHighlightColor
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            textFormat: Text.StyledText
                            visible: !scorePanel.passedOut && scorePanel.deal.points !== undefined
                            text: {
                                var d = scorePanel.deal
                                if (d.points === undefined)
                                    return ""
                                var lines = []
                                for (var s = 0; s < 2; ++s)
                                    if (d.points[s] > 0)
                                        lines.push(page.coloured(s, qsTr("%1 +%2").arg(game.sideName(s)).arg(d.points[s])))
                                var extra = []
                                if (d.honorsPoints > 0)
                                    extra.push(qsTr("%1 for honours in %2's hand").arg(d.honorsPoints).arg(game.playerName(d.honorsSeat)))
                                if (d.rubberWon >= 0)
                                    extra.push(d.rubberWon === 0 ? qsTr("We win the rubber!") : qsTr("They win the rubber."))
                                else if (d.gameWon >= 0)
                                    extra.push(d.gameWon === 0 ? qsTr("Game to us.") : qsTr("Game to them."))
                                return lines.join("   ") + (extra.length ? "<br>" + extra.join(" · ") : "")
                            }
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeMedium
                        }
                    }

                    Column {
                        id: overPanel
                        readonly property int won: game.winner

                        x: margin
                        width: parent.width - 2 * margin
                        opacity: game.phase === Bridge.GameOver ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { FadeAnimation { duration: 400 } }

                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: overPanel.won === 0 ? qsTr("You won") : overPanel.won === 1 ? qsTr("They won") : qsTr("A tie")
                            color: overPanel.won >= 0 ? Cards.playerColor(overPanel.won) : Theme.highlightColor
                            font.pixelSize: Theme.fontSizeHuge
                            font.family: Theme.fontFamilyHeading
                        }
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            text: qsTr("%1 to %2").arg(game.scores[0]).arg(game.scores[1])
                                  + (game.lastDeal.declarer !== undefined
                                     ? " · " + qsTr("last %1 by %2").arg(game.contractText(game.lastDeal)).arg(game.playerName(game.lastDeal.declarer))
                                     : "")
                            color: Theme.secondaryHighlightColor
                            font.pixelSize: Theme.fontSizeMedium
                        }
                    }
                }

                // Space for your hand (cards are drawn by the card layer).
                Item {
                    id: handRow
                    width: parent.width
                    height: cardHeight + handRowStep + Theme.paddingLarge
                }

                BiddingBox {
                    x: margin
                    width: parent.width - 2 * margin
                    visible: page.bidding
                    active: page.bidding && game.humanTurn
                    minLevel: game.minLevel
                    canDouble: game.canDouble
                    canRedouble: game.canRedouble
                    calls: game.auction.length
                    suggestion: page.suggestion
                    onCall: game.call(callId)
                    onExplain: {
                        page.explained = -1
                        page.note = qsTr("%1 would show: %2").arg(page.richCall(callId)).arg(Suits.colored(game.explainCall(callId), page.red))
                    }
                    onHint: {
                        var callId = game.hint()
                        page.suggestion = callId
                        if (callId >= 0) {
                            page.explained = -1
                            page.note = qsTr("Try %1: %2").arg(page.richCall(callId)).arg(Suits.colored(game.explainCall(callId), page.red))
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: Theme.itemSizeSmall
                    visible: !page.bidding

                    Row {
                        anchors.centerIn: parent
                        spacing: Theme.paddingLarge

                        Button {
                            visible: game.phase === Bridge.Score
                            text: qsTr("Next deal")
                            onClicked: game.nextDeal()
                        }
                        Button {
                            visible: page.scoring && !(game.lastDeal.passedOut === true)
                            text: qsTr("Review")
                            onClicked: pageStack.push(Qt.resolvedUrl("DealPage.qml"))
                        }
                        Button {
                            visible: game.phase === Bridge.GameOver
                            text: qsTr("New game")
                            onClicked: game.newGame()
                        }
                    }
                }
            }

            // Hold the table to see the last trick.
            PeekArea {
                id: peek
                y: cluster.y + tableRow.y
                width: parent.width
                height: tableRow.height
                enabled: page.playing && game.lastTrick.length > 0
            }

            // ---------------------------------------------------- cards
            TapToPlay {
                id: tapToPlay
                twice: game.tapTwice
            }

            // All 52 cards live here and glide to wherever the game puts them.
            Repeater {
                model: 52
                TableCard {
                    readonly property bool inHand: place.role === "hand"

                    place: page.places[index]
                    width: cardWidth
                    cardId: index
                    selected: tapToPlay.raised === index
                    glowing: place.glow === true
                    glowColor: place.glowColor !== undefined ? place.glowColor : Theme.highlightColor
                    // game.trick makes these follow the play.
                    readonly property bool yours: inHand && page.playing && game.humanTurn && game.turn === place.seat
                    dimmed: yours && game.trick.length >= 0 && !game.canPlay(index)
                    enabled: yours && game.trick.length >= 0 && game.canPlay(index)

                    onEnabledChanged: if (!enabled) tapToPlay.lower(index)
                    onClicked: if (tapToPlay.tap(index)) game.play(index)
                }
            }
        }
    }
}
