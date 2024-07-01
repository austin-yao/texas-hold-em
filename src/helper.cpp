#include "helper.h"

bool do_read_help(int fd, char *buf, int len)
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

std::string do_read(int fd)
{
    unsigned short rlen;
    do_read_help(fd, (char *)&rlen, sizeof(rlen));
    rlen = ntohs(rlen);
    char buf[rlen + 1];
    do_read_help(fd, buf, rlen);
    return std::string(buf);
}

std::string stringifyAddress(sockaddr_in src)
{
    struct in_addr in;
    in.s_addr = src.sin_addr.s_addr;
    std::string clientName = std::string(inet_ntoa(in));
    clientName = clientName + ":" + std::to_string(ntohs(src.sin_port));
    return clientName;
}

std::vector<std::string> split(std::string str, char target)
{
    std::vector<std::string> ans;
    size_t start = 0;
    size_t end;

    while ((end = str.find(target, start)) != std::string::npos)
    {
        std::string token = str.substr(start, end - start);
        if (token != " ")
        {
            ans.push_back(token);
        }
        start = end + 1;
    }
    if (ans.size() == 0)
    {
        ans.push_back(str);
    }
    else if (start < str.size())
    {

        ans.push_back(str.substr(start, str.size() - start));
    }
    return ans;
}