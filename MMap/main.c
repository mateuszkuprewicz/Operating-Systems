#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#define MAX_ALPHABET 256

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s n\n", name);
    fprintf(stderr, "10000 >= n > 0 - number of children\n");
    exit(EXIT_FAILURE);
}

struct shared_data {
    char alphabet[MAX_ALPHABET];
    int counts[MAX_ALPHABET];
    pthread_mutex_t mutex;
    int alphabet_size;
};

void child_work(char*, struct shared_data*, int offset, int size);

int main(int argc, char** argv)
{
    if(argc != 3) usage("argc");
    char* file_name = argv[1];
    int N = atoi(argv[2]);
    int fd;
    if((fd = open(file_name, O_RDONLY)) == -1)
        ERR("open");

    int file_size;
    struct stat st;
    if(stat(file_name, &st) != 0)
        ERR("stat");
    file_size = st.st_size;

    char* fp;
    if((fp = (char*)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED)
        ERR("mmap");
    close(fd);

    struct shared_data *shared;
    if((shared = (struct shared_data*)mmap(NULL, sizeof(struct shared_data), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0))==MAP_FAILED)
    ERR("mmap");

    char *alphabet; int* alphabet_count;
    alphabet = shared->alphabet;
    alphabet_count = shared->counts;
    pthread_mutex_t *mutex;
    int* global_alphabet_size;
    mutex = &shared->mutex;
    global_alphabet_size = &shared->alphabet_size;
    
    *global_alphabet_size = 0;

    for(int i = 0; i < MAX_ALPHABET; i++)
    {
        (alphabet)[i] = '\0';
        (alphabet_count)[i] = 0;
    }
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(mutex, &mutex_attr);

    char* text = malloc(file_size + 1);
    if(text == NULL) ERR("malloc");
    memcpy(text, fp, file_size);
    text[file_size] = '\0';
    printf("%s\n", text);

    int chunk = file_size/N;
    if(chunk == 0) 
    {
        N = file_size;
        chunk = 1;
    }

    int prev_pointer = -chunk;
    for(int i = 0; i < N; ++i)
    {
        prev_pointer+=chunk;
        int size = chunk;
        if(i == N - 1) size = file_size - prev_pointer;
        switch(fork())
        {
            case 0:
                child_work(fp, shared, prev_pointer, size);
                exit(0);
            case -1:
                ERR("fork");
        }
    }

    pid_t pid;
    int status;
    int summarizing = 1;
    for (;;)
    {
        pid = waitpid(-1, &status, 0);
        if(pid == -1) {
            if(errno == ECHILD) break; 
            ERR("waitpid");
        }

        if(WIFSIGNALED(status)) {
            summarizing = 0;
            printf("Calculations not finished\n");
            break; 
        }
    }

    if(summarizing)
    {
        int ii = 0;
        int alphabet_size = 0;
        while(alphabet_count[ii]!=0)
        {
            alphabet_size++;
            printf("%c => %d\n", alphabet[ii], alphabet_count[ii]);
            ii++;
        }
        printf("alphabet_size: %d\n", alphabet_size);
    }
    
    free(text);
    if(munmap(fp, file_size)) ERR("munmap");
    if(munmap(shared,sizeof(struct shared_data))) ERR("munmap");
}

void child_work(char *fp, struct shared_data* shared, int offset, int size)
{
    char* text = fp + offset;
    int local_count[MAX_ALPHABET];
    
    for (int i = 0; i < MAX_ALPHABET; i++)
    {
        local_count[i] = 0;
    }

    for(int i = 0; i < size; i++)
    {
        unsigned char c = (unsigned char)text[i];
        local_count[c]++;
    }

    char *alphabet = shared->alphabet;
    int* alphabet_count = shared->counts;
    pthread_mutex_t* mutex = &shared->mutex;
    int* alphabet_size = &shared->alphabet_size;
    
    srand(getpid());
    int chance = rand() % 100;
    if(chance < 3) abort();

    int error;
    if((error = pthread_mutex_lock(mutex))!=0)
    {
        if (error == EOWNERDEAD)
        {
            pthread_mutex_consistent(mutex);
        }
        else
        {
            ERR("pthread_mutex_lock");
        }
    }

    for (int c = 0; c < 256; c++)
    {
        if (local_count[c] > 0)
        {
            int found = 0;
            for(int j = 0; j < *alphabet_size; j++)
            {
                if((unsigned char)alphabet[j] == c)
                {
                    alphabet_count[j] += local_count[c];
                    found = 1;
                    break;
                }
            }
            if(found == 0)
            {
                alphabet[*alphabet_size] = (char)c;
                alphabet_count[*alphabet_size] = local_count[c];
                (*alphabet_size)++;
            }
        }
    }

    pthread_mutex_unlock(mutex);
    srand(getpid());
}