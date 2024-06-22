#include "main.h"
#include "helper.h"
#include "models.h"

int numPlayers;
bool debug;

std::vector<Player> seats;
int seatsTaken = 0;
bool gameStarted;

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
            Player player = seats[i];
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
    for (int i = 0; i < seats.size(); i++)
    {
        if (!seats[i].active)
        {
            seats[i] = player;
            lock.unlock();
            return i;
        }
    }
    // not possible to reach here.
    return -1;
}

void broadcastMessage(std::string msg)
{
    for (auto player : seats)
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
    int utg;

    for (int i = 0; i < numPlayers; i++)
    {
        if (seats[i].active > 0)
        {
            sb = i;
            seats[i].sb = true;
            break;
        }
    }
    for (int i = 1; i < numPlayers; i++)
    {
        int idx = (sb + i) % numPlayers;
        if (seats[idx].active > 0)
        {
            bb = i;
            seats[idx].bb = true;
            break;
        }
    }
    for (int i = 1; i < numPlayers; i++)
    {
        int idx = (bb + i) % numPlayers;
        if (seats[idx].active)
        {
            utg = idx;
            seats[idx].utg = true;
            break;
        }
    }
    std::vector<Player *> playersInHand;
    for (int i = 0; i < numPlayers; i++)
    {
        int idx = (sb + i) % numPlayers;
        if (seats[idx].active)
        {
            playersInHand.push_back(&seats[idx]);
            seats[idx].inHand = true;
        }
    }
    // TODO: keep game state to know what players have which cards
    // TODO: ensure that players who just joined are not dealt cards. keep track of an inhand variable for each.
    while (true)
    {
        // shuffle the cards
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(cards.begin(), cards.end(), rng);

        // TODO: broadcast stack sizes of all players and positions, do this at every action.

        // calculate the blinds
        int pot = std::min(seats[bb].stack, 2) + std::min(seats[sb].stack, 1);
        seats[bb].amountInStreet = std::min(seats[bb].stack, 2);
        seats[sb].amountInStreet = std::min(seats[sb].stack, 2);

        seats[bb].stack -= std::min(seats[bb].stack, 2);
        seats[sb].stack -= std::min(seats[sb].stack, 1);

        // burn the first card
        int index = 1;
        int action = 0;
        std::string playersMessage = "";
        for (int i = 0; i < playersInHand.size(); i++)
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
                playersInHand[i]->amountInStreet = 0;
            }
            playersMessage += playersInHand[i]->username + "(" + std::to_string(playersInHand[i]->stack) + ") ";
        }
        broadcastMessage(playersMessage);
        // broadcast information about the people in the hand.

        // dealing the cards
        int numPlayersInHand = playersInHand.size();
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
        int lastToBet = 1;
        int prevRaise = 2;
        int toCall = 2;
        bool firstRound = true;

        // TODO: handle case where the big blind can also have action.
        while (action != lastToBet || firstRound)
        {
            Player *player = playersInHand[action];
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
            while (true)
            {
                if (prevRaise == 0)
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
                    if (prevRaise == 0)
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
                    numPlayersInHand--;
                    // TODO: handle case where only one player remains
                    break;
                }
                else if (tokens[0] == "call")
                {
                    if (prevRaise == 0)
                    {
                        do_write_string(player->fd, "No bet to call.");
                    }
                    else
                    {
                        // need to keep track of amount to call.
                        int amount = std::max(player->stack, toCall - player->amountInStreet);
                        player->stack -= amount;
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
                        int raise = bet - toCall;
                        // TODO: handle all in case
                        if (raise < prevRaise || raise < 2)
                        {
                            if (debug)
                            {
                                std::cout << prevRaise << std::endl;
                                std::cout << raise << std::endl;
                            }
                            do_write_string(player->fd, "Bet does not meet minimum raise");
                        }
                        else
                        {
                            player->stack -= bet - player->amountInStreet;
                            player->amountInStreet = bet;
                            prevRaise = raise;
                            lastToBet = action;
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
        }

        // flop
        index++;
        std::string flop1 = cards[index++];
        std::string flop2 = cards[index++];
        std::string flop3 = cards[index++];
        std::string flop = "Flop: " + flop1 + " " + flop2 + " " + flop3;
        broadcastMessage(flop);

        break;
    }
}

int main(int argc, char *argv[])
{
    numPlayers = 0;
    processCommandLine(argc, argv);
    gameStarted = false;

    seats = std::vector<Player>(numPlayers);

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
                gameStarted = true;
                std::thread gameThread(startGame);
                gameThread.detach();
            }
        }
    }
    return 0;
}