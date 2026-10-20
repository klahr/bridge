import QtQuick 2.6
import Sailfish.Silica 1.0

// One side's place on the score board: name, total, games won and the
// part score towards the next, and the tricks taken this deal.
Item {
    id: root

    property string name
    property int score
    property int games
    property int partScore
    property bool rubber: true
    property bool vulnerable
    // -1 when no deal is being played.
    property int tricks: -1
    property bool active
    property color color: Theme.highlightColor

    height: column.height + Theme.paddingSmall

    property int _shown: 0
    onScoreChanged: {
        var gained = score - _shown
        _shown = score
        if (gained > 0) {
            gain.text = "+" + gained
            gainAnimation.restart()
        }
    }

    Rectangle {
        anchors { fill: parent; leftMargin: Theme.paddingSmall / 2; rightMargin: Theme.paddingSmall / 2 }
        radius: Theme.paddingSmall
        color: Theme.rgba(root.color, 0.18)
        opacity: root.active ? 1 : 0
        Behavior on opacity { FadeAnimation { } }
    }

    Column {
        id: column
        width: parent.width

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.paddingSmall
            Label {
                text: root.name
                color: root.color
                font.pixelSize: Theme.fontSizeSmall
            }
            // Vulnerable: more to win, and more to lose.
            Label {
                visible: root.vulnerable
                text: qsTr("vul")
                color: "#ff6e6e"
                font.pixelSize: Theme.fontSizeExtraSmall
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Label {
            id: scoreLabel
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.score
            color: root.color
            font.pixelSize: Theme.fontSizeExtraLarge
            font.family: Theme.fontFamilyHeading
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: (root.rubber ? (root.games > 0 ? "●" : "○") + (root.games > 1 ? "●" : "○") + "  " : "")
                  + (root.partScore > 0 ? qsTr("%1 on").arg(root.partScore) : "")
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.tricks >= 0 ? qsTr("%n trick(s)", "", root.tricks) : " "
            color: root.tricks >= 0 ? root.color : Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    Label {
        id: gain
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: scoreLabel.contentWidth / 2 + width / 2 + Theme.paddingSmall
        y: scoreLabel.y
        opacity: 0
        color: root.color
        font.pixelSize: Theme.fontSizeSmall
        font.bold: true
    }

    ParallelAnimation {
        id: gainAnimation
        NumberAnimation { target: gain; property: "opacity"; from: 1; to: 0; duration: 1400; easing.type: Easing.InQuad }
        NumberAnimation {
            target: gain; property: "y"
            from: scoreLabel.y; to: scoreLabel.y - Theme.paddingLarge
            duration: 1400; easing.type: Easing.OutQuad
        }
    }
}
