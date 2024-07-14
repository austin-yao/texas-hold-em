#include "../include/main.h"
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
                if (playersInHand.size() == 1)
                {
                    game.seats[idx].sb = true;
                }
                else if (playersInHand.size() == 2)
                {
                    game.seats[idx].bb = true;
                }
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

        std::cout << game.seats[bb].username << " " << game.seats[bb].bb << std::endl;
        std::cout << game.seats[sb].username << " " << game.seats[sb].sb << std::endl;

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
            std::vector<int> newBlinds = gameFinished(currHand, game);
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
            std::vector<int> newBlinds = gameFinished(currHand, game);
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
            std::vector<int> newBlinds = gameFinished(currHand, game);
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
            std::vector<int> newBlinds = gameFinished(currHand, game);
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

            int handStrength = std::get<0>(handResults[0])[0];
            std::string winner = handLabel(handStrength);

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

        std::vector<int> newBlinds = calculateBlinds(currHand, game);
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