#include "helper.h"

bool do_read(int fd, char *buf, int len)
{
    int rcvd = 0;
    while (rcvd < len)
    {
        int n = read(fd, &buf[rcvd], len - rcvd);
        if (n < 0)
        {
            return false;
        }
        rcvd += n;
    }
    return true;
}

bool do_write(int fd, char *buf, int len)
{
    int sent = 0;
    while (sent < len)
    {
        int n = write(fd, &buf[sent], len - sent);
        if (n < 0)
        {
            return false;
        }
        sent += n;
    }
    return true;
}

bool do_write_cstr(int fd, char *buf, int len)
{
    unsigned short wlen = htons(len);
    if (!do_write(fd, (char *)&wlen, sizeof(wlen)))
    {
        return false;
    }
    if (!do_write(fd, buf, len))
    {
        return false;
    }
    return true;
}

bool do_write_string(int fd, std::string msg)
{
    char cstr[msg.size() + 1];
    std::strcpy(cstr, msg.c_str());

    unsigned short wlen = htons(msg.size());
    if (!do_write(fd, (char *)&wlen, sizeof(wlen)))
    {
        return false;
    }
    if (!do_write(fd, cstr, msg.size()))
    {
        return false;
    }
    return true;
}

std::string stringifyAddress(sockaddr_in src)
{
    struct in_addr in;
    in.s_addr = src.sin_addr.s_addr;
    std::string clientName = std::string(inet_ntoa(in));
    clientName = clientName + ":" + std::to_string(ntohs(src.sin_port));
    return clientName;
}