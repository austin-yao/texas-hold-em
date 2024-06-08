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