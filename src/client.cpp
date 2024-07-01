#include "../include/client.h"

int processCommandLine(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Server address not provided" << std::endl;
        exit(1);
    }

    std::string server = argv[1];
    size_t colonIndex = server.find(':');
    std::string serverAddress = server.substr(0, colonIndex);
    int port = atoi(server.substr(colonIndex + 1, server.size() - colonIndex - 1).c_str());

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);

    std::cout << "Server Address: " << serverAddress << " and port: " << port << std::endl;

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, serverAddress.c_str(), &(servaddr.sin_addr));
    connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    return sockfd;
}

int main(int argc, char *argv[])
{
    int sockfd = processCommandLine(argc, argv);
    char buf[1000];
    while (true)
    {
        fd_set r;
        FD_ZERO(&r);

        FD_SET(STDIN_FILENO, &r);
        FD_SET(sockfd, &r);

        int numFds = std::max(STDIN_FILENO, sockfd) + 1;
        int ret = select(numFds, &r, NULL, NULL, NULL);
        if (ret < 0)
        {
            std::cout << "Error using select" << std::endl;
        }

        if (FD_ISSET(STDIN_FILENO, &r))
        {
            // read it in and then send it.
            char buffer[1024];
            memset(&buffer, '\0', sizeof(buffer));

            if (fgets(buffer, sizeof(buffer), stdin) != NULL)
            {
                int index = strcspn(buffer, "\n");
                buffer[index] = '\0';
                do_write_cstr(sockfd, buffer, index);
            }
        }
        if (FD_ISSET(sockfd, &r))
        {
            memset(&buf, '\0', sizeof(buf));
            unsigned short rlen;
            do_read_help(sockfd, (char *)&rlen, sizeof(rlen));
            rlen = ntohs(rlen);
            char readBuf[rlen + 1];
            memset(&readBuf, '\0', sizeof(readBuf));
            do_read_help(sockfd, readBuf, rlen);
            readBuf[rlen + 1] = 0;
            std::cout << std::string(readBuf) << std::endl;
        }
    }
    close(sockfd);
}