#include "main.h"
#include "helper.h"
#include "models.h"

int numPlayers;
bool debug;

std::vector<Player> seats;
int seatsTaken = 0;

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
}

std::string getUsername(int fd)
{
    std::string message = "What is your username?";
    do_write_string(fd, message);
    while (true)
    {
        unsigned short rlen;
        do_read(fd, (char *)&rlen, sizeof(rlen));
        rlen = ntohs(rlen);
        char buf[rlen + 1];
        do_read(fd, buf, rlen);
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
            continue;
        }
        for (int i = 0; i < numPlayers; i++)
        {
            Player player = seats[i];
            if (player.username.size() > 0 && player.username == username)
            {
                message = "This username is already taken. Please choose another one.";
                do_write_string(fd, message);
                continue;
            }
        }
        return username;
    }
}
int main(int argc, char *argv[])
{
    numPlayers = 0;
    processCommandLine(argc, argv);

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

    if (listen(listen_fd, 8) < 0)
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
            // TODO: Revert this.
            break;
        }
        else
        {
            // get the username of the player
            std::string username = getUsername(comm_fd);
            if (debug)
            {
                std::cout << "Received username from " << clientName << ": " << username << std::endl;
            }
        }

        close(comm_fd);
    }
    return 0;
}