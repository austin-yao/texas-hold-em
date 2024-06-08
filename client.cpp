#include "client.h"
#include "helper.h"

int processCommandLine(int argc, char *argv[])
{
    if (argc < 3)
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
    unsigned short wlen = htons(strlen(argv[2]));
    do_write(sockfd, (char *)&wlen, sizeof(wlen));
    do_write(sockfd, argv[2], strlen(argv[2]));
    unsigned short rlen;
    do_read(sockfd, (char *)&rlen, sizeof(rlen));
    rlen = ntohs(rlen);
    char buf[rlen + 1];
    do_read(sockfd, buf, rlen);
    buf[rlen] = 0;
    printf("Echo: [%s] (%d bytes)\n", buf, rlen);
    close(sockfd);
}