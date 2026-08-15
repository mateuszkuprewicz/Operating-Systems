#include "common.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

#define BACKLOG 3
#define MAX_MESS_SIZE 128
#define MAXADDR 5
#define STACK_SIZE 16
#define THREAD_NUM 4
#define DIVISION_NAMES_SIZE 128

int global_sock_fd;
char* global_port;

struct divisions_info
{
    char divisions[DIVISION_NAMES_SIZE][MAX_MESS_SIZE];
    int isAlly[DIVISION_NAMES_SIZE];
    pthread_mutex_t div_mtx;
    int count;
};

struct map_info
{
    int map[100][100];
    pthread_mutex_t mtx_rows[100];
};

struct message
{
    char X[3];
    char Y[3];
    char P;
    char message_data[MAX_MESS_SIZE];
};

struct my_stack
{
    struct message mess[STACK_SIZE];
    int size;
    pthread_mutex_t stack_mtx;
    pthread_cond_t stack_cond_empty;
    pthread_cond_t stack_cond_full;
};

struct thread_arg
{
    struct my_stack* stack;
    struct map_info* map_info;
    struct divisions_info* divisions_info;
};

typedef struct timespec timespec_t;


void msleep(unsigned int milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    timespec_t req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if (nanosleep(&req, &req))
        ERR("nanosleep");
}

int make_socket(int domain, int type)
{
    int sock;
    sock = socket(domain, type, 0);
    if (sock < 0)
        ERR("socket");
    return sock;
}

int bind_inet_socket(uint16_t port, int type)
{
    struct sockaddr_in addr;
    int socketfd, t = 1;
    socketfd = make_socket(PF_INET, type);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        ERR("bind");
    if (SOCK_STREAM == type)
        if (listen(socketfd, BACKLOG) < 0)
            ERR("listen");
    return socketfd;
}

void cleanup(void *arg) { pthread_mutex_unlock((pthread_mutex_t *)arg); }

void * napoleon_function(void *arg)
{
    struct thread_arg* thread_arg = (struct thread_arg*) arg;
    struct map_info* map_info = thread_arg->map_info;
    struct divisions_info* div_info = thread_arg->divisions_info;
    //struct divisions_info* division_info
    while(1)
    {
        msleep(5000);
        for(int i = 0; i < 100; i++)
        {
            if(pthread_mutex_lock(&map_info->mtx_rows[i]))
                ERR("mutex lock");
            for(int j = 0; j < 100; j++)
            {
                printf("%d  ", map_info->map[i][j]);
            }
            if(pthread_mutex_unlock(&map_info->mtx_rows[i]))
                ERR("mutex unlock");
            printf("\n");
        }

        if(pthread_mutex_lock(&div_info->div_mtx))
            ERR("mutex lock");
        int allies_count = 0;
        for(int i = 0; i < div_info->count; i++)
        {
            if(div_info->isAlly[i] == 1)
                allies_count++;
        }
        if(allies_count == 0)
        {
            if(pthread_mutex_unlock(&div_info->div_mtx))
                ERR("mutex unlock");
            continue;
        }
        srand(time(NULL));
        int chosen_div = rand() % allies_count;
        allies_count = -1;
        for(int i = 0; i < div_info->count; i++)
        {
            if(div_info->isAlly[i] == 1)
            {
                allies_count++;
                if(allies_count == chosen_div)
                {
                    int X = rand() % 100;
                    int Y = rand() % 100;
                    char div_name[MAX_MESS_SIZE];
                    strcpy(div_name, div_info->divisions[i]);
                    char full_message[MAX_MESS_SIZE + 5 + 3];
                    snprintf(full_message, sizeof(full_message), "%c%c %c%c 0 %s", X/10 + '0', X%10 + '0', Y/10 + '0', Y%10 + '0', div_name);
                    struct sockaddr_in temp_addr;
                    temp_addr = make_address("localhost", global_port);
                    printf("%d %s \n", global_sock_fd, full_message);
                    if(sendto(global_sock_fd, full_message, strlen(full_message), 0, (struct sockaddr *)&temp_addr, sizeof(temp_addr))<0)
                        ERR("send to");
                    break;
                }
            }
        }
        if(pthread_mutex_unlock(&div_info->div_mtx))
            ERR("mutex unlock");
    }
}

void* worker_function(void* arg)
{
    struct thread_arg* thread_arg = (struct thread_arg*) arg;
    struct my_stack* stack = thread_arg->stack;
    struct divisions_info* division_info = thread_arg->divisions_info;
    struct map_info* map_info = thread_arg->map_info;

    while(1)
    {
        struct message my_message;
        pthread_cleanup_push(cleanup, &stack->stack_mtx);
        if(pthread_mutex_lock(&stack->stack_mtx))
            ERR("mutex lock");

        while(stack->size == 0)
        {
            if(pthread_cond_wait(&stack->stack_cond_empty, &stack->stack_mtx))
                ERR("cond wait");
        }

        char side[6];
        char P = stack->mess[stack->size - 1].P;
        if(P == '1')
        {
            char * temp = "wrogi";
            strcpy(side, temp);
        } 
        else if(P == '0')
        {
            char* temp = "nasz";
            strcpy(side, temp);
            side[4] = '\0';
        }

        my_message = stack->mess[stack->size - 1];

        printf("%s oddział %s był widziany na pozycji %s:%s\n", side, stack->mess[stack->size - 1].message_data, stack->mess[stack->size - 1].X, stack->mess[stack->size - 1].Y);
        stack->size--;
        if(pthread_cond_signal(&stack->stack_cond_full))
            ERR("cond signal");
        pthread_cleanup_pop(1); //unlock

        msleep(10);
        int division_exists = 0;
        int division_index = -1;

        if(pthread_mutex_lock(&division_info->div_mtx))
            ERR("mutex lock");
        for(int i = 0; i < division_info->count; i++)
        {
            if(strcmp(division_info->divisions[i], my_message.message_data) == 0)
            {
                division_exists = 1;
                division_index = i;
                break;
            }
        }
        if(division_exists == 0)
        {
            strcpy(division_info->divisions[division_info->count], my_message.message_data);
            if(my_message.P == '0')
                division_info->isAlly[division_info->count] = 1;
            else
                division_info->isAlly[division_info->count] = 0;
            division_index = division_info->count;
            division_info->count++;
        }
        if(pthread_mutex_unlock(&division_info->div_mtx))
            ERR("mutex unlock");

        for(int i = 0; i < 100; i++)
        {
            int exit_flag = 0;
            if(pthread_mutex_lock(&map_info->mtx_rows[i]))
                ERR("mutex lock");

            for(int j = 0; j < 100; j++)
            {
                if(map_info->map[i][j] == division_index)
                {
                    map_info->map[i][j] = -1;
                    exit_flag = 1;
                    break;
                }
            }

            if(pthread_mutex_unlock(&map_info->mtx_rows[i]))
                ERR("mutex unlock");
            if(exit_flag) break;
        }
        
        int X, Y;
        X = (my_message.X[0] - '0') * 10 + my_message.X[1] - '0';
        Y = (my_message.Y[0] - '0') * 10 + my_message.Y[1] - '0';

        if(pthread_mutex_lock(&map_info->mtx_rows[Y]))
            ERR("mutex lock");
        map_info->map[Y][X] = division_index;
        
        if(pthread_mutex_unlock(&map_info->mtx_rows[Y]))
            ERR("mutex unlock");
    }
}



void do_server(int sock_fd, pthread_t *threads)
{
    struct sockaddr client_addr;
    socklen_t size = sizeof(client_addr);
    size_t buf_size = 5 + 3 + MAX_MESS_SIZE;
    char buffer[buf_size];

    struct my_stack stack;
    stack.size = 0;
    if(pthread_cond_init(&stack.stack_cond_empty, NULL))
        ERR("cond empty init");

    if(pthread_cond_init(&stack.stack_cond_full, NULL))
        ERR("cond full init");

    if(pthread_mutex_init(&stack.stack_mtx, NULL))
        ERR("mutex init");

    struct divisions_info division_info;
    division_info.count = 0;
    if(pthread_mutex_init(&division_info.div_mtx, NULL))
        ERR("mutex init");

    struct map_info map_info;
    for(int i = 0; i < 100; i++)
    {
        if(pthread_mutex_init(&map_info.mtx_rows[i], NULL))
            ERR("mutex init");
        for(int j = 0; j < 100; j++)
        {
            map_info.map[i][j] = -1;
        }
    }
        
    struct thread_arg arg;
    arg.divisions_info = &division_info;
    arg.map_info = &map_info;
    arg.stack = &stack;

    for(int i = 0; i < THREAD_NUM; i++)
    {
        if(pthread_create(&threads[i], NULL, worker_function, (void*) &arg))
            ERR("pthread create");
    }

    pthread_t napoleon_id;
    if(pthread_create(&napoleon_id, NULL, napoleon_function, (void*) &arg))
        ERR("napoleon create");
    
    while(1)
    {
        if(pthread_mutex_lock(&stack.stack_mtx))
            ERR("mutex lock");
        while(stack.size == STACK_SIZE)
        {
            if(pthread_cond_wait(&stack.stack_cond_full, &stack.stack_mtx))
                ERR("cond wait");
        }
        if(pthread_mutex_unlock(&stack.stack_mtx))
                ERR("unlock");
            
        int read_bytes = recvfrom(sock_fd, &buffer, buf_size, 0, &client_addr, &size);
        if(read_bytes < 0) ERR("recfrom");
        if(read_bytes == 0) continue;

        if(read_bytes < 9)
        {
            printf("Invalid message\n");
            continue;
        }

        buffer[read_bytes - 1] = '\0';
        char X[3], Y[3], P;
        char message[MAX_MESS_SIZE];
        strncpy(X, buffer, 2);
        X[2] = '\0';
        strncpy(Y, buffer + 3, 2);
        Y[2] = '\0';
        strncpy(&P, buffer + 6, 1);
        strncpy(message, buffer + 8 , MAX_MESS_SIZE);

        //validation
        if(X[0] < '0' || X[0] > '9' || X[1] < '0' || X[1] > '9' || Y[0] < '0' || Y[0] > '9' || Y[1] < '0' || Y[1] > '9')
        {
            printf("Invalid message\n");
            continue;
        }

        if(buffer[2] != ' ' || buffer[5] != ' ' || buffer[7] != ' ')
        {
            printf("Invalid message\n");
            continue;
        }
        
        if(P != '0' && P != '1') 
        {
            printf("Invalid message\n");
            continue;
        }

        if(pthread_mutex_lock(&stack.stack_mtx))
            ERR("mutex lock");
        stack.mess[stack.size].P  = P;
        strcpy(stack.mess[stack.size].X, X);
        strcpy(stack.mess[stack.size].Y, Y);
        strcpy(stack.mess[stack.size].message_data, message);
        message[MAX_MESS_SIZE - 1] = '\0';
        stack.size++;
        if(pthread_mutex_unlock(&stack.stack_mtx))
            ERR("mutex unlock");

        if(pthread_cond_broadcast(&stack.stack_cond_empty))
            ERR("broadcast");
    }
}


int main(int argc, char** argv)
{
    if(argc != 2)
        ERR("Invalid argument count");
    global_sock_fd = bind_inet_socket(atoi(argv[1]), SOCK_DGRAM);
    global_port = argv[1];
    pthread_t threads[THREAD_NUM];

    do_server(global_sock_fd, threads);
    return 0;
}