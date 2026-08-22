#include "common.h"
#include <asm-generic/errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

#define BACKLOG 5
#define MAX_EVENTS 4
#define CLIENT_BUFFER_SIZE 16
#define SINGLE_MES_SIZE 4
#define CITIES_NUM 20
#define GREEK 'g'
#define PERSIAN 'p'

volatile sig_atomic_t do_work = 1;

typedef struct {
    int connected;
    int fd;
    char buffer[CLIENT_BUFFER_SIZE];
    int buffer_size;
} scout;

void SIGINT_handler()
{
    do_work = 0;
}

int parse_message(char message[], char cities[])
{
    char owner = message[0];
    if(owner != GREEK && owner != PERSIAN)
        return -1;
    char d1 = message[1]; char d2 = message[2];
    if(d1 != '0' && d1 != '1' && d2 != '2')
        return -2;
    if(d2 < '0' || d2 > '9')
        return -3;
    int city = (d1 - '0') * 10 + (d2 - '0'); 
    if(city > 20 || city < 1)
        return -4;
    if(message[3] != '\n')
        return -5;

    if(cities[city - 1] == owner) return 0;
    
    cities[city - 1] = owner;
    return 1;
}

void close_client(scout scouts[], int client_index, int* scout_number)
{
    printf("client disconnected\n");
    close(scouts[client_index].fd);
    scouts[client_index].connected = 0;
    scouts[client_index].buffer_size = 0;
    scouts[client_index].fd = -1;
    (*scout_number)--;
}

void do_server(int sfd, char cities[], sigset_t *old_mask)
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

    while(do_work)
    {
        int nfds = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, old_mask);
        if (nfds < 0) 
        {
            if (errno != EINTR)
                ERR("epoll_wait");
        }
        for(int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;
            if(fd == sfd) //new client
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
            else //new message from client
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
                ssize_t n = read(fd, msg, CLIENT_BUFFER_SIZE - scouts[client_index].buffer_size);
                if (n < 0) ERR("read");
                if (n == 0) 
                {
                    close_client(scouts, client_index, &scout_number);
                    continue;
                }

                memcpy(scouts[client_index].buffer + scouts[client_index].buffer_size, msg, n);
                scouts[client_index].buffer_size += n;
                int written_bytes_num = 0;

                while(scouts[client_index].buffer_size >= SINGLE_MES_SIZE) //parsing client's messages loop
                {
                    char temp[SINGLE_MES_SIZE + 1];
                    memcpy(temp, scouts[client_index].buffer + written_bytes_num, SINGLE_MES_SIZE);
                    temp[SINGLE_MES_SIZE] = '\0';
                    printf("%d: %s\n", scouts[client_index].fd, temp);
                    written_bytes_num += SINGLE_MES_SIZE;
                    scouts[client_index].buffer_size -= SINGLE_MES_SIZE;
                    int result = parse_message(temp, cities);
                    if(result < 0)
                    {
                        close_client(scouts, client_index, &scout_number);
                        break;
                    }
                    //printf("%d\n", result);
                    if(result == 1)
                    {
                        printf("A city changed its owner\n");
                        for(int j = 0; j < MAX_EVENTS; j++)
                        {
                            if(scouts[j].connected == 1 && scouts[j].fd != scouts[client_index].fd)
                            {
                                if(write(scouts[j].fd, temp, SINGLE_MES_SIZE) < 0)
                                {
                                    if (errno == EPIPE)
                                    {
                                        close_client(scouts, j, &scout_number);
                                    }
                                    else if(errno == EAGAIN || errno == EWOULDBLOCK)
                                    {
                                        fprintf(stderr, "Warning: Buffer full for client %d, dropping message.\n", scouts[j].fd);
                                    }
                                    else
                                    {
                                        ERR("write"); 
                                    }
                                }    
                            }       
                        }
                    }
                }
                memmove(scouts[client_index].buffer, scouts[client_index].buffer + written_bytes_num, scouts[client_index].buffer_size);
            }
        }
    }
    for(int i = 0; i < MAX_EVENTS; i++)
    {
        if(scouts[i].connected == 1)
            close_client(scouts, i, &scout_number);
    }
}

int main(int argc, char **argv)
{
    if(argc!=2)
        ERR("Invalid argument number");

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    sethandler(SIGINT_handler, SIGINT);

    int port = atoi(argv[1]);                                      
    int tcp_listen_socket = bind_tcp_socket(port, BACKLOG);

    char cities[CITIES_NUM];
    for(int i = 0; i < CITIES_NUM; i++)
        cities[i] = GREEK; 

    do_server(tcp_listen_socket, cities, &oldmask);

    if (TEMP_FAILURE_RETRY(close(tcp_listen_socket)) < 0)
        ERR("close");

    for(int i = 0; i < CITIES_NUM; i++)
        printf("%d : %c\n", i + 1, cities[i]);
}