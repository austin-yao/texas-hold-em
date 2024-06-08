#include "main.h"
#include "helper.h"

int numPlayers;
bool debug;

std::vector<int> seats;

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

int main(int argc, char *argv[])
{
    numPlayers = 0;
    processCommandLine(argc, argv);

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        std::cout << "Socket not working" << std::endl;
    }

    // creating the binding for the master server
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
    while (true)
    {
        struct sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof(clientaddr);
        int comm_fd = accept(listen_fd, (struct sockaddr *)&clientaddr, &clientaddrlen);
        if (debug)
        {
            printf("Connection from %s\n", inet_ntoa(clientaddr.sin_addr));
        }

        unsigned short rlen;
        do_read(comm_fd, (char *)&rlen, sizeof(rlen));
        rlen = ntohs(rlen);
        char buf[rlen + 1];
        do_read(comm_fd, buf, rlen);
        buf[rlen] = 0;
        printf("Echoing: [%s] (%d bytes)\n", buf, rlen);
        unsigned short wlen = htons(rlen);
        do_write(comm_fd, (char *)&wlen, sizeof(wlen));
        do_write(comm_fd, buf, rlen);
        close(comm_fd);
    }
    return 0;
}