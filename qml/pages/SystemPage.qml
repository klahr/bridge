import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components/Suits.js" as Suits
import "../cards/cards.js" as Cards

// What the calls mean in the systems of this game: yours first, then theirs.
Page {
    id: page

    readonly property color red: Theme.colorScheme === Theme.LightOnDark ? "#ff6e6e" : "#c62828"
    readonly property var systems: game.started ? game.gameSystems : [game.system, game.theirSystem]

    allowedOrientations: Orientation.Portrait

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            PageHeader { title: qsTr("Bidding systems") }

            Repeater {
                // Theirs only if it is another one.
                model: page.systems[0] === page.systems[1] ? 1 : 2
                Column {
                    id: systemColumn
                    readonly property int system: page.systems[index]
                    readonly property int side: index
                    width: column.width

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        wrapMode: Text.Wrap
                        text: game.systemName(systemColumn.system)
                        color: Cards.playerColor(systemColumn.side)
                        font.pixelSize: Theme.fontSizeLarge
                        font.family: Theme.fontFamilyHeading
                    }
                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        wrapMode: Text.Wrap
                        text: (page.systems[0] === page.systems[1] ? qsTr("Everyone bids it.")
                                                                    : systemColumn.side === 0 ? qsTr("You and North bid it.")
                                                                                              : qsTr("West and East bid it."))
                              + " " + game.systemDescription(systemColumn.system)
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    Repeater {
                        model: game.systemSummary(systemColumn.system)
                        Item {
                            readonly property bool header: modelData.header !== undefined

                            width: systemColumn.width
                            height: header ? sectionHeader.height : Math.max(callLabel.height, meaning.height) + Theme.paddingSmall

                            SectionHeader {
                                id: sectionHeader
                                visible: parent.header
                                text: parent.header ? modelData.header : ""
                            }
                            Label {
                                id: callLabel
                                visible: !parent.header
                                x: Theme.horizontalPageMargin
                                width: Theme.itemSizeMedium
                                textFormat: Text.StyledText
                                text: parent.header ? "" : Suits.colored(game.callText(modelData.call), page.red)
                                color: Theme.highlightColor
                                font.pixelSize: Theme.fontSizeMedium
                                font.family: Theme.fontFamilyHeading
                            }
                            Label {
                                id: meaning
                                visible: !parent.header
                                x: callLabel.x + callLabel.width
                                width: parent.width - x - Theme.horizontalPageMargin
                                wrapMode: Text.Wrap
                                text: parent.header ? "" : modelData.text
                                color: Theme.secondaryHighlightColor
                                font.pixelSize: Theme.fontSizeSmall
                            }
                        }
                    }

                    Item { width: 1; height: Theme.paddingLarge }
                }
            }
        }
        VerticalScrollDecorator { }
    }
}
