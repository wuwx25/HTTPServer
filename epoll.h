#ifndef EPOLL_H
#define EPOLL_H
#include<sys/epoll.h>
#include<fcntl.h>
#include<unistd.h>

static int setnonblocking(int fd) {
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

class myEpoll{
private:
    int epollfd;
public:
    myEpoll(){ epollfd = epoll_create(5);}
    ~myEpoll(){ close(epollfd);}
    int getEpollfd();
    void addfd(int fd, bool one_shot);
    void removefd(int fd);
    void modfd(int fd, int ev);
};


#endif