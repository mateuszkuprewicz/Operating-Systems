#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define LIMIT 11

volatile sig_atomic_t SIGUSR1_signal_flag = 0;
volatile sig_atomic_t SIGUSR2_signal_flag = 0;
volatile sig_atomic_t SIGINT_signal_flag = 0;

void precise_sleep_ms(int ms) {
    struct timespec req, rem;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;

    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        req = rem; 
    }
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;

    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void SIGUSR1_handler()
{
    SIGUSR1_signal_flag = 1;
}

void SIGUSR2_handler()
{
    SIGUSR2_signal_flag = 1;
}

void SIGINT_handler()
{
    SIGINT_signal_flag = 1;
}

void child_work(int index, sigset_t *waiting_set)
{
    printf("Pid: %d, index: %d\n", getpid(), index);
    sethandler(SIGUSR2_handler, SIGUSR2);
    srand(time(NULL)*getpid());


    while(1)
    {
        SIGUSR1_signal_flag = 0;
        int temp;
        while((temp = sigsuspend(waiting_set)))
        {
            if(SIGUSR1_signal_flag == 1 || SIGINT_signal_flag == 1)
                break;
        }
        if(SIGINT_signal_flag == 1)
        {
            char path[LIMIT];
            int temp_end_index = snprintf(path, LIMIT - 1, "%d", getpid());
            path[temp_end_index] = '\0';
            int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
            if(fd == -1)
                ERR("open");
            char counter[LIMIT];
            temp_end_index = snprintf(counter, LIMIT - 1, "%d", index);
            counter[temp_end_index] = '\0';
            write(fd, counter, temp_end_index);
            close(fd);
            break;
        }
        else //SIGUSR1_signal_flag == 1
        {
            SIGUSR2_signal_flag = 0;
            int t = (rand()%101 + 100)*1000;
            while(1)
            {
                precise_sleep_ms(t);
                if(SIGUSR2_signal_flag == 1)
                    break;
                index++;
                printf("Pid: %d, index: %d\n", getpid(), index);
            }
        }
        
    }
}

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        fprintf(stderr, "Invalid argument number\n");
        exit(EXIT_FAILURE);
    }
    int N = atoi(argv[1]);

    printf("Parent process: %d\n", getpid());

    int* children = malloc(N*sizeof(int));
    if(children==NULL)
        ERR("malloc");

    sethandler(SIGUSR1_handler, SIGUSR1);
    sethandler(SIGINT_handler, SIGINT);
    
    sigset_t waiting_set, blocking_set;
    if(sigemptyset(&blocking_set))
        ERR("sigemptyset");
    if(sigaddset(&blocking_set, SIGUSR1))
        ERR("sigaddset");
    if(sigaddset(&blocking_set, SIGINT))
        ERR("sigaddset");
    if(sigprocmask(SIG_BLOCK, &blocking_set, &waiting_set))
        ERR("sigprocmask");

    for(int i = 0; i < N; i++)
    {
        int f = fork();
        if(f == 0)
        {
            child_work(i, &waiting_set);
            exit(EXIT_SUCCESS);
        }
        else if(f == -1)
        {
            perror("Fork:");
            exit(EXIT_FAILURE);
        }
        else{
            children[i] = f;
        }
    }

    srand(time(NULL));
    int working_child = rand()%N;
    if(kill(children[working_child], SIGUSR1))
        ERR("kill");

    int temp;
    while(1)
    {
        SIGUSR1_signal_flag = 0;   
        while((temp = sigsuspend(&waiting_set)))
        {

            if(SIGUSR1_signal_flag == 1 || SIGINT_signal_flag == 1)
                break;
        }

        if(SIGINT_signal_flag == 1)
        {
            if(kill(children[working_child], SIGUSR2))
                ERR("kill");
            for(int i = 0; i < N; i++)
            {
                if(kill(children[i], SIGINT))
                    ERR("kill");
            }
            break;
        }
        else //SIGUSR1_signal_flag == 1
        {
            if(kill(children[working_child], SIGUSR2))
                ERR("kill");
            working_child = (working_child + 1)%N;
            if(kill(children[working_child], SIGUSR1))
                ERR("kill");
        }
    }
    
    free(children);
    
    while(wait(NULL) > 0){}
    if(errno!=ECHILD)
    {
        perror("wait");
    }
}