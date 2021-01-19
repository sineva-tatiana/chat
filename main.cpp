#include <sys/socket.h>
#include <netinet/in.h>
//#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include <sys/epoll.h>
#include <fcntl.h>

#include <map>
#include <unordered_map>

#include <iostream>
#include <vector>

using namespace std;

#define MAX_BUF     1024
#define MAX_EVENTS  100

unordered_map<int32_t, std::string> map_login;
unordered_map<int32_t, uint8_t> map_nroom;
int epfd;

void errExit(const char *srt);
void close_connect(int connfd);


int main() {
    int listenfd, connfd;
    struct sockaddr_in serv_addr;

    char send_buff[MAX_BUF];
    std::string buf;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(send_buff, '0', sizeof(send_buff));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(5000);

    bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    listen(listenfd, 10);

    int nfds, s, j;
    struct epoll_event ev;
    struct epoll_event evlist[MAX_EVENTS];

    epfd = epoll_create(1);
    if (epfd == -1)
        errExit("epoll_create");

        ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLRDHUP;
        ev.data.fd = listenfd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1)
            errExit("epoll_ctl");

    while(1) {
        printf("About to epoll_wait()\n");
        nfds = epoll_wait(epfd, evlist, MAX_EVENTS, -1);
        if (nfds == -1) {
            /* Перезапускаем, если операция была прервана сигналом */
            if (errno == EINTR)
                continue;
            else
                errExit("epoll_wait");
        }
        printf("Ready: %d\n", nfds);
        /* Обрабатываем полученный список событий */
        for (j = 0; j < nfds; j++) {
            printf("fd=%d; events: %s%s%s\n", evlist[j].data.fd,
            (evlist[j].events & EPOLLIN)? "EPOLLIN ": "",
            (evlist[j].events & EPOLLHUP)? "EPOLLHUP ": "",
            (evlist[j].events & EPOLLERR)? "EPOLLERR ": "");

            // new connect
            if (evlist[j].data.fd == listenfd){
                printf("listenfd=%d\n", listenfd);
                connfd = accept(listenfd, (struct sockaddr *) NULL, NULL);
                ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLRDHUP;
                ev.data.fd = connfd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev) == -1)
                    errExit("epoll_ctl");

                std::string t_buf = "login:";
                write(connfd, t_buf.data(), t_buf.size());
            }
            // data reception
            else {
                if (evlist[j].events & EPOLLIN) {
                    connfd = evlist[j].data.fd;
                    s =  read(connfd, send_buff, MAX_BUF);
                    if (s == 0)
                        close_connect(connfd);
                    if (s == -1)
                        errExit("read");
                    send_buff[s] = 0;
                    std::string t_buf(send_buff);
                    buf = t_buf;
                    printf("read %d bytes: %s\n", s, (char*)buf.data());

                    // Is it login?
                    if (map_login.find(connfd) == map_login.end()) {
                        map_login[connfd] = buf;
                        std::string t_buf = "room №:";
                        write(connfd, t_buf.data(), t_buf.size());

                    }
                    // Is it №room?
                    else if (map_nroom.find(connfd) == map_nroom.end()) {
                        map_nroom[connfd] = atoi(buf.data());
                    }
                    // It is message
                    else {
                        unordered_map<int32_t, uint8_t>::iterator it;
                        uint8_t cur_nroom = map_nroom[connfd];
                        buf = map_login[connfd] + " >> " + buf;
                        for (it = map_nroom.begin(); it != map_nroom.end(); it++)
                            if (it->second == cur_nroom && it->first != connfd) {
                                write(it->first, buf.data(), buf.size());
                            }
                    }
                } else if (evlist[j].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                    /* Если установлены флаги EPOLLIN и EPOLLHUP, то количество байтов,
                    доступных для чтения, может превышать MAX_BUF. Следовательно,
                    мы закрываем файловый дескриптор, только если не был установлен
                    флаг EPOLLIN. Остальные байты будут прочитаны во время следующих
                    вызовов epoll_wait(). */
                    close_connect(connfd);
                }
            }
        }
    }
}

void errExit(const char *str)
{
    perror(str);
    exit(EXIT_FAILURE);
}

void close_connect(int connfd)
{
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, NULL) == -1)
        errExit("epoll_ctl");
    map_login.erase(connfd);
    map_nroom.erase(connfd);
    printf("closing fd %d\n", connfd);
    if (close(connfd) == -1)
        errExit("close");
}