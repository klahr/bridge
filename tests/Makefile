CARDS ?= ../sailfish-cards
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CORE = ../src/core/card.cpp ../src/core/auction.cpp ../src/core/hand.cpp ../src/core/bidding.cpp \
    ../src/core/scoring.cpp ../src/core/game.cpp ../src/core/solver.cpp ../src/core/bot.cpp
SRC = test_rules.cpp $(CORE) $(CARDS)/src/cards/cards.cpp

test: test_rules
	./test_rules

test_rules: $(SRC) $(wildcard ../src/core/*.h) $(wildcard $(CARDS)/src/cards/*.h)
	$(CXX) $(CXXFLAGS) -I$(CARDS)/src -o $@ $(SRC)

clean:
	rm -f test_rules

.PHONY: test clean
