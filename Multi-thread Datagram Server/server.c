#include "common.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>

#define BACKLOG 3
#define MAX_MESS_SIZE 128
#define MAXADDR 5
#define STACK_SIZE 16
#define THREAD_NUM 4
#define DIVISION_NAMES_SIZE 128


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

void cleanup(void *arg) { pthread_mutex_unlock((pthread_mutex_t *)arg); }

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

void* map_printer_function(void* arg)
{
    struct thread_arg* thread_arg = (struct thread_arg*) arg;
    struct map_info* map_info = thread_arg->map_info;

    while(1)
    {
        msleep(2000); 
        printf("\n--- Map state ---\n");
        printf ("type: 0 - enemy, 1 - ally\n");
        for(int i = 0; i < 100; i++)
        {
            if(pthread_mutex_lock(&map_info->mtx_rows[i]))
                ERR("mutex lock");
            
            int row_has_data = 0;
            for(int j = 0; j < 100; j++) {
                if(map_info->map[i][j] != -1) { row_has_data = 1; break; }
            }

            if(row_has_data) {
                printf("Row %d: ", i);
                for(int j = 0; j < 100; j++)
                {
                    if(map_info->map[i][j] != -1)
                        printf("[col %d -> type %d ] ", j, map_info->map[i][j]);
                }
                printf("\n");
            }

            if(pthread_mutex_unlock(&map_info->mtx_rows[i]))
                ERR("mutex unlock");
        }
        printf("---------------------------\n");
    }
    return NULL;
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
            char * temp = "nasz";
            strcpy(side, temp);
        } 
        else if(P == '0')
        {
            char* temp = "wrogi";
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
            if(my_message.P == '1')
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

    pthread_t printer_thread;
    if(pthread_create(&printer_thread, NULL, map_printer_function, (void*) &arg))
        ERR("printer create");
    
    while(1)
    {
        int read_bytes = recvfrom(sock_fd, buffer, buf_size, 0, &client_addr, &size);
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
        while(stack.size == STACK_SIZE)
        {
            if(pthread_cond_wait(&stack.stack_cond_full, &stack.stack_mtx))
                ERR("cond wait");
        }
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
    int sock_fd = bind_inet_socket(atoi(argv[1]), SOCK_DGRAM);
    pthread_t threads[THREAD_NUM];

    do_server(sock_fd, threads);
    return 0;
}