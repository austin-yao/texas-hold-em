#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <arpa/inet.h>

bool do_read(int fd, char *buf, int len);
bool do_write(int fd, char *buf, int len);
bool do_write_cstr(int fd, char *buf, int len);
bool do_write_string(int fd, std::string msg);
std::string stringifyAddress(sockaddr_in src);