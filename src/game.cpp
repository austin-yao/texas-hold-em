#include "../include/game.h"

void resetPlayer(Player *player)
{
    player->amountInStreet = 0;
    player->allIn = false;
    player->folded = false;
    player->cards.clear();
    player->bb = false;
    player->sb = false;
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

std::vector<int> calculateBlinds(Hand &hand, Game &game)
{
    int sb = hand.sb;
    int bb = hand.bb;

    for (int i = 1; i < game.numPlayers; i++)
    {
        int index = (sb + i) % game.numPlayers;
        if (game.seats[index].active && game.seats[index].stack > 0)
        {
            std::cout << "Index: " << index << std::endl;
            sb = index;
            break;
        }
    }
    for (int i = 1; i < game.numPlayers; i++)
    {
        int index = (sb + i) % game.numPlayers;
        if (game.seats[index].active && game.seats[index].stack > 0)
        {
            bb = index;
            break;
        }
    }
    return {sb, bb};
}

std::vector<int> gameFinished(Hand &hand, Game &game)
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
    return calculateBlinds(hand, game);
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
        if (hand.playersRemaining == 1)
        {
            break;
        }
        do_write_string(player->fd, "Action is on you");
        bool bbcheck;
        while (true)
        {
            if (hand.toCall == 0 || (preflop && player->bb && hand.toCall == 2))
            {
                do_write_string(player->fd, "bet, check, or fold");
            }
            else
            {
                do_write_string(player->fd, "bet, call, or fold");
            }

            std::string response = do_read(player->fd);
            std::vector<std::string> tokens = split(response, ' ');

            if (tokens[0] == "check")
            {
                if (player->bb && hand.toCall == 2)
                {
                    bbcheck = true;
                    break;
                }
                if (hand.toCall == 0)
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
                if (hand.toCall == 0)
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
                        // player->stack is the amount in this pot?
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

        action = (action + 1) % numPlayers;
        if (bbcheck)
        {
            break;
        }
    }

    // calculating allInLogic
    // if everyone all in for the same amount, don't push back to allIns.
    std::sort(allIns.begin(), allIns.end(), [](const std::vector<int> &a, const std::vector<int> &b)
              { return a[0] < b[0]; });

    // add in a check here.
    bool everyoneAllIn = true;
    if (allIns.size() > 0)
    {
        int allInAmount = allIns[0][0];
        for (Player *player : hand.playersInHand)
        {
            if (player->folded)
            {
                continue;
            }
            if (player->amountInStreet < player->stack)
            {
                everyoneAllIn = false;
                break;
            }
            if (player->amountInStreet != allInAmount)
            {
                everyoneAllIn = false;
                break;
            }
        }
    }
    else
    {
        everyoneAllIn = false;
    }

    int totalAllIn = 0;
    std::vector<Hand> otherHands;
    if (!everyoneAllIn && allIns.size() > 0)
    {
        for (auto allIn : allIns)
        {
            Player *allInPlayer = hand.playersInHand[allIn[1]];
            std::cout << "Inside all In 393 with " << allIn[0] << " all in player: " << allInPlayer->username << std::endl;
            if (allInPlayer->stack == 0)
            {
                allInPlayer->allIn = true;
                continue;
            }
            Hand copyHand = hand;
            copyHand.playersInHand.clear();
            copyHand.pot = 0;
            for (Player *player : hand.playersInHand)
            {
                if ((!player->folded && player->stack > 0) || (player == allInPlayer))
                {
                    std::cout << "264 " << player->username << std::endl;
                    copyHand.playersInHand.push_back(player);
                    int amountToSubtract = std::min(allIn[0] - totalAllIn, player->stack);
                    std::cout << "Amount to subtract: " << amountToSubtract << std::endl;
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
            std::cout << "Copy hand.pot: " << copyHand.pot << std::endl;
            otherHands.push_back(copyHand);
        }
    }

    for (Player *player : hand.playersInHand)
    {
        player->stack -= player->amountInStreet;
        if (player->stack == 0)
        {
            player->allIn = true;
        }
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