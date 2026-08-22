#include "common.h"
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

#define BACKLOG 5
#define MAX_EVENTS 4
#define CLIENT_BUFFER_SIZE 16
#define SINGLE_MES_SIZE 4

typedef struct {
    int connected;
    int fd;
    char buffer[CLIENT_BUFFER_SIZE];
    int buffer_size;
} scout;

void do_server(int sfd)
{
    int scout_number = 0;
    scout scouts[MAX_EVENTS];
    for(int i = 0; i < MAX_EVENTS; i++)
    {
        scouts[i].connected = 0;
        scouts[i].fd = -1;
    }

    int epoll_fd = epoll_create1(0);
    if(epoll_fd < 0)
        ERR("epoll_create1");

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sfd;
    struct epoll_event events[MAX_EVENTS];
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sfd, &ev))
        ERR("epoll_ctl");

    while(1)
    {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) 
        {
            if (errno != EINTR)
                ERR("epoll_wait");
        }
        for(int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;
            if(fd == sfd)
            {
                if(scout_number == 4) 
                {
                    int rejected_fd = accept(sfd, NULL, NULL);
                    if (rejected_fd >= 0) close(rejected_fd);
                    continue;
                }

                int client_fd = add_new_client(sfd);
                ev.events = EPOLLIN;
                ev.data.fd = client_fd;

                for(int j = 0; j < MAX_EVENTS; j++)
                {
                    if(scouts[j].connected == 0)
                    {
                        scouts[j].connected = 1;
                        scouts[j].fd = client_fd;
                        scouts[j].buffer_size = 0;
                        scout_number++;
                        break;
                    }
                }

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev))
                    ERR("epoll_ctl");
                int flags = fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK;
                fcntl(client_fd, F_SETFL, flags);
            }
            else 
            {
                int client_index = -1;
                for(int j = 0; j < MAX_EVENTS; j++)
                {
                    if(scouts[j].connected == 1 && scouts[j].fd == fd)
                    {
                        client_index = j;
                        break;
                    }
                }
                if(client_index == -1) 
                {
                    close(fd);
                    continue;
                }

                char msg[CLIENT_BUFFER_SIZE];
                ssize_t n = read(fd, &msg, CLIENT_BUFFER_SIZE - scouts[client_index].buffer_size);
                if (n < 0) ERR("read");
                if (n == 0) 
                {
                    printf("client disconnected\n");
                    close(fd);
                    scouts[client_index].connected = 0;
                    scouts[client_index].buffer_size = 0;
                    scouts[client_index].fd = -1;
                    scout_number--;
                    continue;
                }
                strncpy(scouts[client_index].buffer + scouts[client_index].buffer_size, msg, n);
                scouts[client_index].buffer_size += n;
                int written_bytes_num = 0;
                while(scouts[client_index].buffer_size >= SINGLE_MES_SIZE)
                {
                    char temp[SINGLE_MES_SIZE + 1];
                    strncpy(temp, scouts[client_index].buffer + written_bytes_num, SINGLE_MES_SIZE);
                    temp[SINGLE_MES_SIZE] = '\0';
                    printf("%d: %s\n", scouts[client_index].fd, temp);
                    written_bytes_num += SINGLE_MES_SIZE;
                    scouts[client_index].buffer_size -= SINGLE_MES_SIZE;
                }
                memmove(scouts[client_index].buffer, scouts[client_index].buffer + written_bytes_num, scouts[client_index].buffer_size);
            }
        }
    }

}

int main(int argc, char **argv)
{
    if(argc!=2)
        ERR("Invalid argument number");
    int port = atoi(argv[1]);                                      
    int tcp_listen_socket = bind_tcp_socket(port, BACKLOG);

    do_server(tcp_listen_socket);

    if (TEMP_FAILURE_RETRY(close(tcp_listen_socket)) < 0)
        ERR("close");

}