#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string.h>

// Author: Kenyon Leblanc

#define MAX_THREADS 3
#define CHUNK_SIZE 4096

typedef struct
{
    int id;
    char *data;
    int data_len;
    char *output;
    int *output_len;
} ThreadData;

void compress_chunk(ThreadData *thread_data);
void compress(int size, char **argv);

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("wzip: file1 [file2 ...]\n");
        return 1;
    }

    compress(argc, argv);

    return 0;
}

void *compress_thread(void *arg)
{
    ThreadData *thread_data = (ThreadData *)arg;
    compress_chunk(thread_data);
    return NULL;
}

void compress_chunk(ThreadData *thread_data)
{
    char c;
    int count = 0;

    char *data = thread_data->data;
    int data_len = thread_data->data_len;

    char *output = thread_data->output;
    int *output_len = thread_data->output_len;

    c = data[0];
    count = 1;

    for (int i = 1; i < data_len; i++)
    {
        if (c == data[i])
        {
            count++;
        }
        else
        {
            memcpy(output + *output_len, &count, sizeof(int));
            *output_len += sizeof(int);
            output[*output_len] = c;
            *output_len += sizeof(char);
            c = data[i];
            count = 1;
        }
    }

    memcpy(output + *output_len, &count, sizeof(int));
    *output_len += sizeof(int);
    output[*output_len] = c;
    *output_len += sizeof(char);
}

void compress(int size, char **argv)
{
    pthread_t threads[MAX_THREADS];
    ThreadData thread_data[MAX_THREADS];

    for (int i = 1; i < size; i++)
    {
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1)
        {
            printf("Error: Unable to open input file.\n");
            exit(1);
        }

        struct stat st;
        fstat(fd, &st);
        size_t file_size = st.st_size;

        char *file_data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (file_data == MAP_FAILED)
        {
            printf("Error: Unable to mmap input file.\n");
            exit(1);
        }

        char *output = malloc(file_size * 2); // Allocate more space than needed for compressed data
        int output_len = 0;

        int num_threads = file_size > CHUNK_SIZE ? MAX_THREADS : 1;
        size_t chunk_size = (file_size + num_threads - 1) / num_threads;

        for (int j = 0; j < num_threads; j++)
        {
            thread_data[j].id = j;
            thread_data[j].data = file_data + j * chunk_size;
            thread_data[j].data_len = j;
            output_len = (j == num_threads - 1) ? file_data + file_size - thread_data[j].data : chunk_size;
            thread_data[j].output = output;
            thread_data[j].output_len = &output_len;
            if (pthread_create(&threads[j], NULL, compress_thread, (void *)&thread_data[j]) != 0)
            {
                printf("Error: Unable to create thread.\n");
                exit(1);
            }
        }

        for (int j = 0; j < num_threads; j++)
        {
            pthread_join(threads[j], NULL);
        }

        fwrite(output, sizeof(char), output_len, stdout);

        munmap(file_data, file_size);
        close(fd);
        free(output);
    }
}
