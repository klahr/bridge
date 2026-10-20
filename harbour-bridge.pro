TARGET = harbour-bridge

CONFIG += sailfishapp c++17

VERSION = 0.1.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

# The card code shared with the other card games, a git submodule.
!exists($$PWD/sailfish-cards/cards.pri): error("sailfish-cards is missing: run git submodule update --init")
include($$PWD/sailfish-cards/cards.pri)

SOURCES += src/harbour-bridge.cpp \
    src/gamecontroller.cpp \
    src/core/auction.cpp \
    src/core/bidding.cpp \
    src/core/bot.cpp \
    src/core/card.cpp \
    src/core/game.cpp \
    src/core/hand.cpp \
    src/core/scoring.cpp \
    src/core/solver.cpp

HEADERS += src/gamecontroller.h \
    src/core/auction.h \
    src/core/bidding.h \
    src/core/bot.h \
    src/core/card.h \
    src/core/game.h \
    src/core/hand.h \
    src/core/scoring.h \
    src/core/solver.h

DISTFILES += qml/harbour-bridge.qml \
    qml/components/AuctionTable.qml \
    qml/components/BiddingBox.qml \
    qml/components/CallButton.qml \
    qml/components/SideScore.qml \
    qml/components/Suits.js \
    qml/cover/CoverPage.qml \
    qml/pages/DealPage.qml \
    qml/pages/GamePage.qml \
    qml/pages/RulesPage.qml \
    qml/pages/ScorePage.qml \
    qml/pages/SettingsPage.qml \
    qml/pages/SystemPage.qml \
    rpm/harbour-bridge.changes \
    rpm/harbour-bridge.spec \
    translations/*.ts \
    harbour-bridge.desktop \
    LICENSE \
    README.md \
    tests/Makefile \
    tests/test_rules.cpp

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

CONFIG += sailfishapp_i18n

# Rule tests: plain C++, no Qt. `make check` here, or `make -C tests` on any host.
check.commands = $(CXX) -std=c++17 -O2 -I$$CARDS_DIR/src -o test_rules $$PWD/tests/test_rules.cpp \
    $$PWD/src/core/auction.cpp $$PWD/src/core/bidding.cpp $$PWD/src/core/bot.cpp $$PWD/src/core/card.cpp \
    $$PWD/src/core/game.cpp $$PWD/src/core/hand.cpp $$PWD/src/core/scoring.cpp $$PWD/src/core/solver.cpp \
    $$CARDS_DIR/src/cards/cards.cpp && ./test_rules
QMAKE_EXTRA_TARGETS += check
