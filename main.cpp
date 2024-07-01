#include "main.h"
#include "helper.h"
#include "models.h"

int numPlayers;
bool debug;

int seatsTaken = 0;
bool gameStarted;

Game game;

// how to best compare two hands?
// compute a score for all of them?
// what to do if the board is supposed to be chopped?
// need a function to evaluate the best five hands made out of any of the 7 cards.

std::shared_mutex seatsMutex;

void processCommandLine(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Number of players not specified" << std::endl;
        exit(1);
    }

    int c;
    unsetenv("POSIXLY_CORRECT");
    while ((c = getopt(argc, argv, "v")) != -1)
    {
        if (c == 'v')
        {
            std::cout << "Debug mode on" << std::endl;
            debug = true;
        }
    }

    if (optind >= argc)
    {
        std::cerr << "Number of players not specified" << std::endl;
        exit(1);
    }

    if (debug)
    {
        numPlayers = atoi(argv[2]);
        std::cout << "Number of players: " << numPlayers << std::endl;
    }
    else
    {
        numPlayers = atoi(argv[1]);
    }
    if (numPlayers > 9 || numPlayers <= 1)
    {
        std::cout << "Must be between 2 and 9 players inclusive." << std::endl;
        exit(1);
    }
}

/*
Helper functions, should abstract away maybe.
*/

void resetPlayer(Player *player)
{
    player->amountInStreet = 0;
    player->allIn = false;
    player->folded = false;
    player->cards.clear();
}

void resetPlayers(Hand &hand)
{
    for (Player *player : hand.playersInHand)
    {
        resetPlayer(player);
    }
}

void resetHand(Hand &hand)
{
    hand.toCall = 0;
    hand.prevRaise = 0;
}

std::string getUsername(int fd)
{
    std::string message = "What is your username?";
    do_write_string(fd, message);
    while (true)
    {
        std::shared_lock<std::shared_mutex> lock(seatsMutex);
        unsigned short rlen;
        do_read_help(fd, (char *)&rlen, sizeof(rlen));
        rlen = ntohs(rlen);
        char buf[rlen + 1];
        do_read_help(fd, buf, rlen);
        buf[rlen] = 0;
        std::string username = std::string(buf);
        if (debug)
        {
            std::cout << "User inputted username " << username << std::endl;
        }
        if (username.size() == 0)
        {
            message = "Please input a username";
            do_write_string(fd, message);
            lock.unlock();
            continue;
        }
        bool flag = false;
        for (int i = 0; i < numPlayers; i++)
        {
            Player player = game.seats[i];
            if (player.username == username)
            {
                message = "This username is already taken. Please choose another one.";
                do_write_string(fd, message);
                flag = true;
            }
        }
        lock.unlock();
        if (!flag)
        {
            return username;
        }
    }
}

int addPlayer(Player &player)
{
    std::unique_lock<std::shared_mutex> lock(seatsMutex);
    for (int i = 0; i < game.seats.size(); i++)
    {
        if (!game.seats[i].active)
        {
            game.seats[i] = player;
            lock.unlock();
            return i;
        }
    }
    // not possible to reach here.
    return -1;
}

void broadcastMessage(std::string msg)
{
    for (auto player : game.seats)
    {
        if (player.active)
        {
            do_write_string(player.fd, msg);
        }
    }
}

std::vector<int> calculateBlinds(Hand &hand)
{
    int sb = hand.sb;
    int bb = hand.bb;
    if (debug)
    {
        std::cout << "Original sb: " << sb << std::endl;
        std::cout << "Original bb: " << bb << std::endl;
        std::cout << "Num Players: " << game.numPlayers << std::endl;
    }

    for (int i = 1; i < game.numPlayers; i++)
    {
        int index = (sb + i) % game.numPlayers;
        if (game.seats[index].active)
        {
            std::cout << "Index: " << index << std::endl;
            sb = index;
            break;
        }
    }
    for (int i = 1; i < game.numPlayers; i++)
    {
        int index = (sb + i) % game.numPlayers;
        if (game.seats[index].active)
        {
            bb = index;
            break;
        }
    }
    if (debug)
    {
        std::cout << "New sb: " << sb << std::endl;
        std::cout << "New bb: " << bb << std::endl;
    }
    return {sb, bb};
}

std::vector<int> gameFinished(Hand &hand)
{
    for (Player *player : hand.playersInHand)
    {
        if (!(player->folded))
        {
            player->stack += hand.pot;
            broadcastMessage(player->username + " wins " + std::to_string(hand.pot));
        }
    }
    hand.pot = 0;
    return calculateBlinds(hand);
}

std::vector<Hand> betStreet(Hand &hand, bool preflop)
{
    int action = 0;
    int numPlayers = hand.playersInHand.size();
    resetHand(hand);
    if (preflop)
    {
        hand.toCall = 2;
    }

    int activePlayers = 0;
    for (Player *player : hand.playersInHand)
    {
        if (!(player->folded || player->allIn))
        {
            activePlayers++;
        }
    }
    if (activePlayers < 2)
    {
        return {};
    }

    std::vector<std::vector<int>> allIns;
    if (!preflop)
    {
        if (numPlayers == 2)
        {
            action = 1;
        }
        else
        {
            for (int i = 0; i < numPlayers + 2; i++)
            {
                if (!(hand.playersInHand[i % numPlayers]->folded || hand.playersInHand[i % numPlayers]->allIn))
                {
                    action = i % numPlayers;
                    break;
                }
            }
        }
    }
    else
    {
        if (numPlayers == 2)
        {
            action = 0;
        }
        else
        {
            action = 2;
        }
    }
    bool firstRound = true;
    while (action != hand.lastToBet || firstRound)
    {
        Player *player = hand.playersInHand[action];
        if (firstRound)
        {
            firstRound = false;
            hand.lastToBet = action;
        }
        if (player->folded || player->allIn)
        {
            action = (action + 1) % numPlayers;
            continue;
        }
        do_write_string(player->fd, "Action is on you");
        bool bbcheck;
        while (true)
        {
            if (hand.prevRaise == 0 || (preflop && player->bb && hand.toCall == 2))
            {
                do_write_string(player->fd, "bet, check, or fold");
            }
            else
            {
                do_write_string(player->fd, "bet, call, or fold");
            }

            std::string response = do_read(player->fd);
            if (debug)
            {
                std::cout << response << std::endl;
            }
            std::vector<std::string> tokens = split(response, ' ');

            if (tokens[0] == "check")
            {
                if (player->bb && hand.toCall == 2)
                {
                    bbcheck = true;
                    break;
                }
                if (hand.prevRaise == 0)
                {
                    break;
                }
                else
                {
                    do_write_string(player->fd, "You cannot check");
                }
            }
            else if (tokens[0] == "fold")
            {
                hand.playersRemaining--;
                player->folded = true;
                break;
            }
            else if (tokens[0] == "call")
            {
                if (hand.prevRaise == 0)
                {
                    do_write_string(player->fd, "No bet to call.");
                }
                else
                {
                    // need to keep track of amount to call.
                    int amount = std::min(player->stack - player->amountInStreet, hand.toCall - player->amountInStreet);

                    // All in case
                    if (amount == player->stack - player->amountInStreet)
                    {
                        player->allIn = true;
                        allIns.push_back({player->stack, action});
                    }
                    player->amountInStreet += amount;
                    hand.pot += amount;
                    break;
                }
            }
            else if (tokens[0] == "bet")
            {
                if (tokens.size() < 2)
                {
                    do_write_string(player->fd, "Please enter a bet");
                }
                else
                {
                    int bet = std::min(atoi(tokens[1].c_str()), player->stack);
                    int raise = bet - hand.toCall;
                    std::cout << "Bet size: " << bet << std::endl;
                    std::cout << "Raise size: " << raise << std::endl;

                    if (bet == player->stack)
                    {
                        allIns.push_back({player->stack, action});
                        player->allIn = true;
                    }
                    else if (raise < hand.prevRaise || raise < 2)
                    {
                        if (debug)
                        {
                            std::cout << hand.prevRaise << std::endl;
                            std::cout << raise << std::endl;
                        }
                        do_write_string(player->fd, "Bet does not meet minimum raise");
                        continue;
                    }
                    hand.toCall = bet;
                    hand.pot += bet - player->amountInStreet;
                    std::cout << "Pot after raise: " << hand.pot << std::endl;
                    player->amountInStreet = bet;
                    hand.prevRaise = raise;
                    hand.lastToBet = action;
                    break;
                }
            }
            else
            {
                do_write_string(player->fd, "Action not recognized");
            }
        }

        // this needs to be the final line
        action = (action + 1) % numPlayers;
        if (bbcheck)
        {
            break;
        }
    }

    // calculating allInLogic
    std::sort(allIns.begin(), allIns.end(), [](const std::vector<int> &a, const std::vector<int> &b)
              { return a[0] < b[0]; });
    int totalAllIn = 0;
    std::vector<Hand> otherHands;
    for (auto allIn : allIns)
    {
        std::cout << "Inside all In 393" << std::endl;
        Player *allInPlayer = hand.playersInHand[allIn[1]];
        if (allInPlayer->stack == 0)
        {
            allInPlayer->allIn = true;
            continue;
        }
        Hand copyHand = hand;
        copyHand.playersInHand.clear();
        for (Player *player : hand.playersInHand)
        {
            if (!(player->folded || player->allIn) || (player == allInPlayer))
            {
                copyHand.playersInHand.push_back(player);
                int amountToSubtract = std::min(allIn[0] - totalAllIn, player->stack);
                copyHand.pot += amountToSubtract;
                hand.pot -= amountToSubtract;
                player->stack -= amountToSubtract;
                player->amountInStreet -= amountToSubtract;
                if (player->stack == 0)
                {
                    player->allIn = true;
                }
            }
        }
        allInPlayer->allIn = true;
        otherHands.push_back(copyHand);
    }

    for (Player *player : hand.playersInHand)
    {
        player->stack -= player->amountInStreet;
        player->amountInStreet = 0;
    }
    return otherHands;
}

void broadcastStacks(Hand &hand)
{
    std::string playersMessage = "";
    for (int i = 0; i < hand.playersInHand.size(); i++)
    {
        if (i == 0)
        {
            playersMessage += "sb: ";
        }
        else if (i == 1)
        {
            playersMessage += "bb: ";
        }
        else
        {
            hand.playersInHand[i]->amountInStreet = 0;
        }
        playersMessage += hand.playersInHand[i]->username + "(" + std::to_string(hand.playersInHand[i]->stack) + ") ";
    }
    broadcastMessage(playersMessage);
}

std::vector<int> handValue(std::vector<std::string> userCards)
{
    // create a map out of this
    std::vector<std::unordered_set<char>> cards(13);
    std::vector<int> ans;
    for (std::string card : userCards)
    {
        cards[cardToNum[card[0]]].insert(card[1]);
    }

    std::vector<char> suits = {'h', 'c', 'd', 's'};
    // check for a royal
    for (char suit : suits)
    {
        bool flag = true;
        for (int i = 8; i <= 12; i++)
        {
            if (cards[i].find(suit) == cards[i].end())
            {
                flag = false;
                break;
            }
        }
        if (flag)
        {
            // royal flush
            return {10};
        }
    }
    // check for a straight flush
    for (char suit : suits)
    {
        // check in decending order of flushes
        for (int i = 8; i >= 0; i--)
        {
            bool flag = true;
            for (int j = 0; j < 5; j++)
            {
                if (cards[i + j].find(suit) == cards[i + j].end())
                {
                    flag = false;
                    break;
                }
            }
            if (flag)
            {
                return {9, i};
            }
        }
        // checking the wheel
        bool flag = true;
        if (cards[12].find(suit) == cards[12].end())
        {
            continue;
        }
        for (int i = 0; i < 5; i++)
        {
            if (cards[i].find(suit) == cards[i].end())
            {
                flag = false;
                break;
            }
        }
        if (flag)
        {
            return {9, -1};
        }
    }

    // check Quads
    for (int i = 12; i >= 0; i--)
    {
        if (cards[i].size() == 4)
        {
            return {8, i};
        }
    }

    // check full house
    std::vector<int> trips;
    std::vector<int> pairs;

    for (int i = 12; i >= 0; i--)
    {
        if (cards[i].size() == 3)
        {
            trips.push_back(i);
            pairs.push_back(i);
        }
        else if (cards[i].size() == 2)
        {
            pairs.push_back(i);
        }
    }
    for (int i = 0; i < trips.size(); i++)
    {
        for (int j = 0; j < pairs.size(); j++)
        {
            if (pairs[j] == trips[i])
            {
                continue;
            }
            return {7, trips[i], pairs[j]};
        }
    }

    // check flush
    for (char suit : suits)
    {
        int count = 0;
        std::vector<int> nums;
        for (int i = 12; i >= 0; i--)
        {
            if (cards[i].find(suit) != cards[i].end())
            {
                count++;
                if (count <= 5)
                {
                    nums.push_back(i);
                }
            }
        }
        if (count >= 5)
        {
            ans.push_back(6);
            for (int num : nums)
            {
                ans.push_back(num);
            }
            return ans;
        }
    }

    // check straight
    for (int i = 8; i >= 0; i--)
    {
        bool flag = true;
        for (int j = 0; j < 5; j++)
        {
            if (cards[i + j].size() == 0)
            {
                flag = false;
                break;
            }
        }
        if (flag)
        {
            return {5, i};
        }
    }

    // check the wheel
    if (cards[12].size() > 0 && cards[0].size() > 0 && cards[1].size() > 0 && cards[2].size() > 0 && cards[3].size() > 0)
    {
        return {5, -1};
    }

    // check trips
    if (trips.size() != 0)
    {
        int trip = trips[0];
        ans = {4, trip};
        for (int i = 12; i >= 0; i--)
        {
            if (cards[i].size() > 0 && i != trip)
            {
                ans.push_back(i);
                if (ans.size() == 4)
                {
                    return ans;
                }
            }
        }
    }

    // check two pair
    if (pairs.size() >= 2)
    {
        int top1 = pairs[0];
        int top2 = pairs[1];
        ans = {3, top1, top2};

        for (int i = 12; i >= 0; i--)
        {
            if (cards[i].size() > 0 && i != top1 && i != top2)
            {
                ans.push_back(i);
                return ans;
            }
        }
    }

    // check pair
    if (pairs.size() > 0)
    {
        int pair = pairs[0];
        ans = {2, pair};
        for (int i = 12; i >= 0; i--)
        {
            if (cards[i].size() > 0 && i != pair)
            {
                ans.push_back(i);
                if (ans.size() == 5)
                {
                    return ans;
                }
            }
        }
    }

    // check high card
    ans = {1};
    for (int i = 12; i >= 0; i--)
    {
        if (cards[i].size() > 0)
        {
            ans.push_back(i);
            if (ans.size() == 6)
            {
                break;
            }
        }
    }
    return ans;
}

bool handComparator(const std::tuple<std::vector<int>, Player *> &a, const std::tuple<std::vector<int>, Player *> &b)
{
    if (std::get<0>(a)[0] > std::get<0>(b)[0])
    {
        return true;
    }
    else if (std::get<0>(a)[0] < std::get<0>(b)[0])
    {
        return false;
    }
    else
    {
        int size = std::get<0>(a).size();
        int index = 1;
        while (index < size)
        {
            if (std::get<0>(a)[index] > std::get<0>(b)[index])
            {
                return true;
            }
            else if (std::get<0>(a)[index] < std::get<0>(b)[index])
            {
                return false;
            }
            index++;
        }
        return true;
    }
}
/*
Game Functionality
*/
void startGame()
{
    if (debug)
    {
        std::cout << "Game starting" << std::endl;
    }
    // find the blind positions
    int bb;
    int sb;

    for (int i = 0; i < numPlayers; i++)
    {
        if (game.seats[i].active > 0)
        {
            sb = i;
            game.seats[i].sb = true;
            break;
        }
    }
    for (int i = 1; i < numPlayers; i++)
    {
        int idx = (sb + i) % numPlayers;
        if (game.seats[idx].active > 0)
        {
            bb = i;
            game.seats[idx].bb = true;
            break;
        }
    }

    // Hand loop
    while (true)
    {
        // shuffle the cards
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(cards.begin(), cards.end(), rng);

        std::vector<Player *> playersInHand;
        for (int i = 0; i < numPlayers; i++)
        {
            int idx = (sb + i) % numPlayers;
            if (game.seats[idx].active)
            {
                playersInHand.push_back(&game.seats[idx]);
                resetPlayer(&game.seats[idx]);
            }
        }

        if (playersInHand.size() < 2)
        {
            // to wait for players to join
            sleep(5);
            continue;
        }

        Hand currHand = {
            .playersInHand = playersInHand,
            .playersRemaining = static_cast<int>(playersInHand.size()),
            .pot = 0,
            .toCall = 2,
            .prevRaise = 2,
            .lastToBet = 1,
            .sb = sb,
            .bb = bb};

        // calculate the blinds
        currHand.pot = std::min(currHand.playersInHand[1]->stack, 2) + std::min(currHand.playersInHand[0]->stack, 1);
        game.seats[bb].amountInStreet = std::min(currHand.playersInHand[1]->stack, 2);
        game.seats[sb].amountInStreet = std::min(currHand.playersInHand[0]->stack, 1);

        // how to calculate logic for All ins?

        if (currHand.playersInHand[0]->stack == 0)
        {
            currHand.playersInHand[0]->allIn = true;
        }
        if (currHand.playersInHand[1]->stack == 0)
        {
            currHand.playersInHand[1]->allIn = true;
        }

        // burn the first card
        int index = 1;
        int action = 0;
        std::string playersMessage = "";
        std::vector<Hand> allInHands;

        broadcastStacks(currHand);
        // broadcast information about the people in the hand.

        // dealing the cards
        int numPlayersInHand = currHand.playersInHand.size();
        for (int i = 0; i < numPlayersInHand * 2; i++)
        {
            int idx = (index % numPlayersInHand);
            Player *player = playersInHand[idx];
            do_write_string(player->fd, cards[index]);
            player->cards.push_back(cards[index]);
            index++;
        }

        // pre-flop

        broadcastStacks(currHand);

        std::vector<Hand> preflopAllIns = betStreet(currHand, true);
        for (Hand hand : preflopAllIns)
        {
            std::cout << "Adding to preflop all in" << std::endl;
            allInHands.push_back(hand);
        }

        if (currHand.playersRemaining == 1)
        {
            std::vector<int> newBlinds = gameFinished(currHand);
            sb = newBlinds[0];
            bb = newBlinds[1];
            continue;
        }

        // flop
        index++;
        std::string flop1 = cards[index++];
        std::string flop2 = cards[index++];
        std::string flop3 = cards[index++];
        std::string flop = "Flop: " + flop1 + " " + flop2 + " " + flop3;
        std::vector<std::string> communityCards = {flop1, flop2, flop3};
        broadcastMessage(flop);

        currHand.prevRaise = 0;
        currHand.lastToBet = -1;
        currHand.toCall = 0;
        // for the flop
        broadcastStacks(currHand);
        std::vector<Hand> flopAllIns = betStreet(currHand, false);
        for (Hand hand : flopAllIns)
        {
            std::cout << "Adding to flop all in" << std::endl;
            allInHands.push_back(hand);
        }

        if (currHand.playersRemaining == 1)
        {
            std::vector<int> newBlinds = gameFinished(currHand);
            sb = newBlinds[0];
            bb = newBlinds[1];
            continue;
        }

        index++;
        std::string turn = cards[index++];
        broadcastMessage("Turn: " + flop1 + " " + flop2 + " " + flop3 + " | " + turn);
        communityCards.push_back(turn);

        currHand.prevRaise = 0;
        currHand.lastToBet = -1;
        currHand.toCall = 0;
        // for the turn
        broadcastStacks(currHand);
        std::vector<Hand> turnAllIns = betStreet(currHand, false);
        for (Hand hand : turnAllIns)
        {
            std::cout << "Adding to turn all in" << std::endl;
            allInHands.push_back(hand);
        }

        if (currHand.playersRemaining == 1)
        {
            std::vector<int> newBlinds = gameFinished(currHand);
            sb = newBlinds[0];
            bb = newBlinds[1];
            continue;
        }

        index++;
        std::string river = cards[index++];
        broadcastMessage("River: " + flop1 + " " + flop2 + " " + flop3 + " | " + turn + " | " + river);
        communityCards.push_back(turn);

        currHand.prevRaise = 0;
        currHand.lastToBet = -1;
        currHand.toCall = 0;
        // for the river
        broadcastStacks(currHand);
        std::vector<Hand> riverAllIns = betStreet(currHand, false);
        for (Hand hand : riverAllIns)
        {
            std::cout << "Adding to river all in" << std::endl;
            allInHands.push_back(hand);
        }

        std::cout << "Betting finished for river" << std::endl;

        if (currHand.playersRemaining == 1)
        {
            std::vector<int> newBlinds = gameFinished(currHand);
            sb = newBlinds[0];
            bb = newBlinds[1];
            continue;
        }

        std::vector<std::tuple<std::vector<int>, Player *>> results;
        for (Player *player : playersInHand)
        {
            if (!(player->folded))
            {
                communityCards.push_back(player->cards[0]);
                communityCards.push_back(player->cards[1]);
                results.push_back(std::make_tuple(handValue(communityCards), player));
                communityCards.pop_back();
                communityCards.pop_back();
            }
        }

        std::sort(results.begin(), results.end(), handComparator);
        // compare hand history
        allInHands.push_back(currHand);
        for (Hand &hand : allInHands)
        {
            int allInIndex = 0;
            std::vector<std::tuple<std::vector<int>, Player *>> handResults;
            while (allInIndex < results.size())
            {
                Player *currPlayer = std::get<1>(results[allInIndex]);
                if (std::find(hand.playersInHand.begin(), hand.playersInHand.end(), currPlayer) != hand.playersInHand.end())
                {
                    handResults.push_back(results[allInIndex]);
                }
                allInIndex++;
            }
            int bestHandCategory = std::get<0>(handResults[0])[0];
            int numWinners = 1;
            std::unordered_set<std::string> winningUsernames;
            winningUsernames.insert(std::get<1>(handResults[0])->username);
            while (numWinners < handResults.size() && handResults[0] == handResults[numWinners])
            {
                winningUsernames.insert(std::get<1>(handResults[numWinners])->username);
                numWinners++;
            }
            int potPerPlayer = currHand.pot / numWinners;
            int potRemaining = currHand.pot % numWinners;

            std::string winner;
            int handStrength = std::get<0>(handResults[0])[0];
            switch (handStrength)
            {
            case 10:
                winner = "Royal Flush";
                break;
            case 9:
                winner = "Straight Flush";
                break;
            case 8:
                winner = "Quads";
                break;
            case 7:
                winner = "Full House";
                break;
            case 6:
                winner = "Flush";
                break;
            case 5:
                winner = "Straight";
                break;
            case 4:
                winner = "Three of a Kind";
                break;
            case 3:
                winner = "Two Pair";
                break;
            case 2:
                winner = "Pair";
                break;
            case 1:
                winner = "High Card";
                break;
            default:
                winner = "Error";
                break;
            }
            std::cout << "Pot per player: " << potPerPlayer << std::endl;
            for (Player *player : currHand.playersInHand)
            {
                if (!(player->folded || player->allIn) && winningUsernames.find(player->username) != winningUsernames.end())
                {
                    broadcastMessage(player->username + " wins " + std::to_string(potPerPlayer + potRemaining) + " with a " + winner + ".");
                    player->stack += potPerPlayer + potRemaining;
                    potRemaining = std::max(0, --potRemaining);
                }
            }
        }

        std::vector<int> newBlinds = calculateBlinds(currHand);
        sb = newBlinds[0];
        bb = newBlinds[1];
    }
}

int main(int argc, char *argv[])
{
    numPlayers = 0;
    processCommandLine(argc, argv);
    gameStarted = false;

    std::vector<Player> seats = std::vector<Player>(numPlayers);
    game = {seats, numPlayers, false};

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        std::cout << "Socket not working" << std::endl;
    }

    // creating the binding for the main server
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(10000);
    if (bind(listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        std::cout << "Error binding" << std::endl;
    }

    if (listen(listen_fd, numPlayers) < 0)
    {
        std::cout << "Error listening" << std::endl;
    }

    // listening for connections
    // should make a thread for this.
    while (true)
    {
        struct sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof(clientaddr);
        int comm_fd = accept(listen_fd, (struct sockaddr *)&clientaddr, &clientaddrlen);
        std::string clientName = stringifyAddress(clientaddr);
        if (debug)
        {
            std::cout << "Connection from " << stringifyAddress(clientaddr) << std::endl;
        }

        if (seatsTaken == numPlayers)
        {
            std::string message = "Table is full.";
            do_write_string(comm_fd, message);
        }
        else
        {
            // get the username of the player
            std::string username = getUsername(comm_fd);
            if (debug)
            {
                std::cout << "Received username from " << clientName << ": " << username << std::endl;
            }
            Player newPlayer = {username, clientName, comm_fd, 1000, true};
            int seatPosition = addPlayer(newPlayer);
            std::string message = "You have been seated at seat " + std::to_string(seatPosition);
            do_write_string(comm_fd, message);

            seatsTaken++;
            if (debug)
            {
                std::cout << "Number of players: " << seatsTaken << std::endl;
            }
            if (seatsTaken == 2 && !gameStarted)
            {
                game.started = true;
                std::thread gameThread(startGame);
                gameThread.detach();
            }
        }
    }
    return 0;
}