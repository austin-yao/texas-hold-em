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
    // TODO: factor in when a player just joins they can't join in a blind.
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
        if (player->inHand)
        {
            player->stack += hand.pot;
            broadcastMessage(player->username + " wins " + std::to_string(hand.pot));
        }
    }
    hand.pot = 0;
    return calculateBlinds(hand);
}

void preflopBet(Hand &hand)
{
    bool firstRound = true;
    int numPlayersInHand = hand.playersInHand.size();
    int action = 2;
    if (numPlayersInHand == 2)
    {
        action = 0;
    }
    while (action != hand.lastToBet || firstRound)
    {
        Player *player = hand.playersInHand[action];
        if (player->bb)
        {
            firstRound = false;
        }
        if (!player->inHand)
        {
            action = (action + 1) % numPlayersInHand;
            continue;
        }
        do_write_string(player->fd, "Action is on you");
        bool bbCheck;
        while (true)
        {
            if (player->bb && hand.prevRaise == 2)
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
                if (player->bb && hand.prevRaise == 2)
                {
                    bbCheck = true;
                    break;
                }
                else
                {
                    do_write_string(player->fd, "You cannot check");
                }
            }
            else if (tokens[0] == "fold")
            {
                numPlayersInHand--;
                player->inHand = false;
                // TODO: handle case where only one player remains
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
                    int amount = std::min(player->stack, hand.toCall - player->amountInStreet);
                    player->stack -= amount;
                    hand.pot += amount;
                    // TODO: handle side pot logic
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
                    // TODO: handle all in case
                    if (raise < hand.prevRaise || raise < 2)
                    {
                        if (debug)
                        {
                            std::cout << hand.prevRaise << std::endl;
                            std::cout << raise << std::endl;
                        }
                        do_write_string(player->fd, "Bet does not meet minimum raise");
                    }
                    else
                    {
                        player->stack -= bet - player->amountInStreet;
                        hand.pot += bet - player->amountInStreet;
                        player->amountInStreet = bet;
                        hand.prevRaise = raise;
                        hand.lastToBet = action;
                        hand.toCall = bet;
                        break;
                    }
                }
            }
            else
            {
                do_write_string(player->fd, "Action not recognized");
            }
        }

        // this needs to be the final line
        action = (action + 1) % numPlayersInHand;
        if (bbCheck)
        {
            break;
        }
    }
}

void betStreet(Hand &hand)
{
    int action = 0;
    int numPlayers = hand.playersInHand.size();
    for (int i = 0; i < numPlayers; i++)
    {
        if (hand.playersInHand[i]->inHand)
        {
            action = i;
            break;
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
        if (!player->inHand)
        {
            action = (action + 1) % numPlayers;
            continue;
        }
        do_write_string(player->fd, "Action is on you");
        while (true)
        {
            if (hand.prevRaise == 0)
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
                player->inHand = false;
                // TODO: handle case where only one player remains
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
                    int amount = std::min(player->stack, hand.toCall - player->amountInStreet);
                    player->stack -= amount;
                    hand.pot += amount;
                    // TODO: handle side pot logic
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
                    // TODO: handle all in case
                    if (raise < hand.prevRaise || raise < 2)
                    {
                        if (debug)
                        {
                            std::cout << hand.prevRaise << std::endl;
                            std::cout << raise << std::endl;
                        }
                        do_write_string(player->fd, "Bet does not meet minimum raise");
                    }
                    else
                    {
                        player->stack -= bet - player->amountInStreet;
                        hand.pot += bet - player->amountInStreet;
                        player->amountInStreet = bet;
                        hand.prevRaise = raise;
                        hand.lastToBet = action;
                        hand.toCall = bet;
                        break;
                    }
                }
            }
            else
            {
                do_write_string(player->fd, "Action not recognized");
            }
        }

        // this needs to be the final line
        action = (action + 1) % numPlayers;
    }
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
    int utg;

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
    // TODO: keep game state to know what players have which cards for showdown.
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
                game.seats[idx].inHand = true;
            }
            else
            {
                game.seats[idx].inHand = false;
            }
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

        currHand.playersInHand[1]->stack -= std::min(currHand.playersInHand[1]->stack, 2);
        currHand.playersInHand[0]->stack -= std::min(currHand.playersInHand[0]->stack, 1);

        // burn the first card
        int index = 1;
        int action = 0;
        std::string playersMessage = "";

        broadcastStacks(currHand);
        // broadcast information about the people in the hand.

        // dealing the cards
        int numPlayersInHand = currHand.playersInHand.size();
        for (int i = 0; i < numPlayersInHand * 2; i++)
        {
            int idx = (index % numPlayersInHand);
            Player *player = playersInHand[idx];
            do_write_string(player->fd, cards[index]);
            index++;
        }

        // pre-flop
        std::cout << "Pre-Flop" << std::endl;
        if (numPlayersInHand == 2)
        {
            // small blind is also UTG
            action = 0;
        }
        else
        {
            action = 2;
        }
        // lastToBet was the bigBlind
        bool firstRound = true;

        broadcastStacks(currHand);
        // TODO: broadcast actions and updated stacks.
        preflopBet(currHand);

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
        broadcastMessage(flop);

        currHand.prevRaise = 0;
        currHand.lastToBet = -1;
        currHand.toCall = 0;
        // for the flop
        broadcastStacks(currHand);
        betStreet(currHand);

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

        currHand.prevRaise = 0;
        currHand.lastToBet = -1;
        currHand.toCall = 0;
        // for the flop
        broadcastStacks(currHand);
        betStreet(currHand);

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

        currHand.prevRaise = 0;
        currHand.lastToBet = -1;
        currHand.toCall = 0;
        // for the flop
        broadcastStacks(currHand);
        betStreet(currHand);

        if (currHand.playersRemaining == 1)
        {
            std::vector<int> newBlinds = gameFinished(currHand);
            sb = newBlinds[0];
            bb = newBlinds[1];
            continue;
        }

        std::vector<int> newBlinds = gameFinished(currHand);
        sb = newBlinds[0];
        bb = newBlinds[1];

        // TODO: showdown
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