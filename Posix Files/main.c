#define _XOPEN_SOURCE 500

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <ftw.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define LIMIT 128
#define MAX_PATH_SIZE 1024
#define MAXFD 32


struct dir_scan
{
    char *path;
    char *extension;
    int depth;
};

struct dir_scan_node
{
    struct dir_scan *data;
    struct dir_scan_node *next;
};

struct dir_scan_queue
{
    struct dir_scan_node *head;
    struct dir_scan_node *tail;
};

void cleanup(struct dir_scan_queue queue)
{
    while(queue.head)
    {
        struct dir_scan_node *node = queue.head;
        queue.head = queue.head->next;
        free(node->data->path);
        free(node->data);
        free(node);
    }
}

void scan_recursive(struct dir_scan_queue queue, FILE *file_stream)
{
    DIR *dirp;
    struct dirent *dp;
    struct stat file_stat;  

    while(queue.head != NULL)
    {
        struct dir_scan_node *cur_dir = queue.head;
        dirp = opendir(cur_dir->data->path);
        if (dirp == NULL) 
        {
            perror("opendir");  
            fprintf(stderr, "Couldn't open directory: %s\n", cur_dir->data->path);
            
            queue.head = queue.head->next;
            free(cur_dir->data->path);
            free(cur_dir->data);
            free(cur_dir);
            continue;
        }

        while((dp = readdir(dirp)) != NULL)
        {
            if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
                continue;

            char full_path[MAX_PATH_SIZE];
            snprintf(full_path, MAX_PATH_SIZE, "%s/%s", cur_dir->data->path, dp->d_name);

            if(lstat(full_path, &file_stat) != 0)
            {
                fprintf(stderr, "Couldn't access %s\n", full_path);
                continue;
            }

            if(S_ISDIR(file_stat.st_mode))
            {
                if(cur_dir->data->depth <= 1)
                    continue;

                struct dir_scan_node *new_node = malloc(sizeof(struct dir_scan_node));
                if(new_node == NULL)
                {
                    cleanup(queue);
                    ERR("malloc");
                }
                struct dir_scan *new_dir = malloc(sizeof(struct dir_scan));
                if(new_dir == NULL)
                {
                    free(new_node);
                    cleanup(queue);
                    ERR("malloc");
                }

                new_dir->path = strdup(full_path);
                if(new_dir->path == NULL)
                    ERR("strdup");
                new_dir->depth = cur_dir->data->depth - 1;
                new_dir->extension = cur_dir->data->extension;
                new_node->data = new_dir;
                new_node->next = NULL;
                queue.tail->next = new_node;
                queue.tail = new_node;

                char *ext = strrchr(dp->d_name, '.');
                if(ext == NULL && cur_dir->data->extension != NULL)
                    continue;
                if(ext != NULL && cur_dir->data->extension != NULL && strcmp(ext, cur_dir->data->extension) != 0)
                    continue;
                fprintf(file_stream, "Directory: %s\n", full_path);
                continue;
            }

            char *ext = strrchr(dp->d_name, '.');
            if(ext == NULL && cur_dir->data->extension != NULL)
                continue;
            if(ext != NULL && cur_dir->data->extension != NULL && strcmp(ext, cur_dir->data->extension) != 0)
                continue;
            
            if(S_ISREG(file_stat.st_mode))
            {
                fprintf(file_stream, "Regular file: %s\n", full_path);
            }
            else if(S_ISLNK(file_stat.st_mode))
            {
                fprintf(file_stream, "Link file: %s\n", full_path);
            }
            else{
                fprintf(file_stream, "Unknown file: %s\n", dp->d_name);
            }
        }
        queue.head = queue.head->next;
        free(cur_dir->data->path);
        free(cur_dir->data);
        free(cur_dir);
        closedir(dirp);
    }
}

int main(int argc, char** argv)
{    
    struct dir_scan dir_scans[LIMIT];
    for(int i = 0; i < LIMIT; i++)
        dir_scans[i].extension = NULL;
    int dir_scans_length = -1;
    int print_output_to_L1_OUTPUTFILE_flag = 0;

    int i = 1;
    if(strcmp(argv[1], "-p") != 0)
        ERR("Bad execution syntax");
    
    while(i < argc)
    {
        if(strcmp(argv[i], "-p") == 0)
        {
            dir_scans_length++;
            if(i >= argc - 1)
                ERR("Path name after -p flag not specified.");
            i++;
            if(dir_scans_length >= LIMIT)
                ERR("max number of folders to scan exceeded");

            dir_scans[dir_scans_length].path = argv[i];
            dir_scans[dir_scans_length].depth = MAXFD;
        }
        else if (strcmp(argv[i], "-e") == 0)
        {
            if(i >= argc - 1)
                ERR("extension name after -e flag not specified.");
            i++;
            dir_scans[dir_scans_length].extension = argv[i];
        }
        else if(strcmp(argv[i], "-d") == 0)
        {
            if(i >= argc - 1)   
                ERR("recursion depth after -d flag not specified.");
            i++;
            dir_scans[dir_scans_length].depth = atoi(argv[i]);
        }
        else if(strcmp(argv[i], "-o") == 0)
        {
            if(i != argc - 1)
                ERR("-o flag is supposed to be the last argument for executing program");
            print_output_to_L1_OUTPUTFILE_flag = 1;
        }
        else {
            fprintf(stderr, "%s ", argv[i]);
            ERR("unknown flag");
        }
        i++;
    }
    dir_scans_length++;
    FILE *file_stream = stdout;
    if(print_output_to_L1_OUTPUTFILE_flag == 1)
    {
        char* write_path = getenv("L1_OUTPUTFILE"); 
        if(write_path)
        {
            file_stream = fopen(write_path, "w");
        }
    }

    for(int j = 0; j < dir_scans_length; j++)
    {
        struct dir_scan_queue queue;
        struct dir_scan_node* node = malloc(sizeof(struct dir_scan_node));
        if(node == NULL)
            ERR("malloc");
        struct dir_scan *data = malloc(sizeof(struct dir_scan));
        if(data == NULL)
        {
            free(node);
            ERR("malloc");
        }
        memcpy(data, &dir_scans[j], sizeof(struct dir_scan));
        data->path = strdup(dir_scans[j].path);
        node->data = data;
        node->next = NULL;
        queue.head = node;
        queue.tail = node;
        printf("Scanning %s\n", dir_scans[j].path);
        fprintf(file_stream, "The contents of: %s\n", dir_scans[j].path);
        scan_recursive(queue, file_stream);
    }
    fclose(file_stream);
    exit(EXIT_SUCCESS);
}