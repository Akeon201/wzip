// Include necessary header files
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string.h>

// Define constants
#define MAX_THREADS 3
#define CHUNK_SIZE 4096

// Define a struct for passing data to threads
typedef struct {
    int id;
    char* data;
    int data_len;
    char* output;
    int* output_len;
    char* first_char;
    char* last_char;
    int* first_count;
    int* last_count;
} ThreadData;

// Function prototypes
void compress_chunk(ThreadData* thread_data);
void compress(int size, char** argv);

// Main function
int main(int argc, char* argv [ ]) {

    // Check for valid input
    if (argc < 2) {
        printf("wzip: file1 [file2 ...]\n");
        return 1;
    }

    // Call the compress function
    compress(argc, argv);

    return 0;
}

// Function to be executed by each thread
void* compress_thread(void* arg) {
    ThreadData* thread_data = (ThreadData*)arg;
    compress_chunk(thread_data);
    return NULL;
}

// Compress a chunk of data using RLE
void compress_chunk(ThreadData* thread_data) {
    char c;
    int count = 0;

    char* data = thread_data->data;
    int data_len = thread_data->data_len;

    char* output = thread_data->output;
    int* output_len = thread_data->output_len;

    c = data [ 0 ];
    count = 1;

    // Iterate through the data and compress it
    for (int i = 1; i < data_len; i++) {
        if (c == data [ i ]) {
            count++;
        }
        else {
            // Handle the first character of the first chunk
            if (c == *thread_data->first_char && thread_data->id == 0) {
                *thread_data->first_count += count;
            }
            else {
                // Write the count and character to the output
                memcpy(output + *output_len, &count, sizeof(int));
                *output_len += sizeof(int);
                output [ *output_len ] = c;
                *output_len += sizeof(char);
            }
            c = data [ i ];
            count = 1;
        }
    }

    *thread_data->last_char = c;
    *thread_data->last_count = count;
}

// Compress multiple files using RLE and multiple threads
void compress(int size, char** argv) {
    pthread_t threads [ MAX_THREADS ];
    ThreadData thread_data [ MAX_THREADS ];
    char prev_last_char = '\0';
    int prev_last_count = 0;
    char* final_output = NULL;
    int final_output_len = 0;

    // Iterate through each input file
    for (int i = 1; i < size; i++) {
        // Open the file and check for errors
        int fd = open(argv [ i ], O_RDONLY);
        if (fd == -1) {
            printf("Error: Unable to open input file.\n");
            exit(1);
        }

        // Get file size
        struct stat st;
        fstat(fd, &st);
        size_t file_size = st.st_size;

        // Memory-map the file for reading
        char* file_data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (file_data == MAP_FAILED) {
            printf("Error: Unable to mmap input file.\n");
            exit(1);
        }

        // Allocate memory for output
        char* output = malloc(file_size * 2); // Allocate more space than needed for compressed data
        int output_len = 0;

        // Calculate number of threads and chunk size
        int num_threads = file_size > CHUNK_SIZE ? MAX_THREADS : 1;
        size_t chunk_size = (file_size + num_threads - 1) / num_threads;

        // Declare arrays for thread-specific values
        char first_char [ MAX_THREADS ], last_char [ MAX_THREADS ];
        int first_count [ MAX_THREADS ], last_count [ MAX_THREADS ];

        // Create threads and pass data to them
        for (int j = 0; j < num_threads; j++) {
            thread_data [ j ].id = j;
            thread_data [ j ].data = file_data + j * chunk_size;
            thread_data [ j ].data_len = (j == num_threads - 1) ? file_data + file_size - thread_data [ j ].data : chunk_size;
            thread_data [ j ].output = output;
            thread_data [ j ].output_len = &output_len;
            thread_data [ j ].first_char = &first_char [ j ];
            thread_data [ j ].last_char = &last_char [ j ];
            thread_data [ j ].first_count = &first_count [ j ];
            thread_data [ j ].last_count = &last_count [ j ];
            if (pthread_create(&threads [ j ], NULL, compress_thread, (void*)&thread_data [ j ]) != 0) {
                printf("Error: Unable to create thread.\n");
                exit(1);
            }
        }

        // Join threads and handle results
        for (int j = 0; j < num_threads; j++) {
            pthread_join(threads [ j ], NULL);

            // Handle overlapping characters between chunks
            if (j == 0 && prev_last_char == first_char [ j ]) {
                first_count [ j ] += prev_last_count;
            }

            if (j > 0 && last_char [ j - 1 ] == first_char [ j ]) {
                first_count [ j ] += last_count [ j - 1 ];
            }
            else if (j > 0) {
                // Write the count and character from the previous chunk to the output
                memcpy(output + output_len, &last_count [ j - 1 ], sizeof(int));
                output_len += sizeof(int);
                output [ output_len ] = last_char [ j - 1 ];
                output_len += sizeof(char);
            }
        }

        // Write the count and character from the last chunk to the output
        memcpy(output + output_len, &last_count [ num_threads - 1 ], sizeof(int));
        output_len += sizeof(int);
        output [ output_len ] = last_char [ num_threads - 1 ];
        output_len += sizeof(char);

        // Update previous character and count for the next file
        if (i < size - 1 && i != 0) {
            prev_last_char = last_char [ num_threads - 1 ];
            prev_last_count = last_count [ num_threads - 1 ];
            output_len -= sizeof(int) + sizeof(char);
        }

        // Append the current output to the final output
        final_output = realloc(final_output, final_output_len + output_len);
        memcpy(final_output + final_output_len, output, output_len);
        final_output_len += output_len;

        // Clean up
        munmap(file_data, file_size);
        close(fd);
        free(output);
    }

    // Write the final output to stdout and free memory
    fwrite(final_output, sizeof(char), final_output_len, stdout);
    free(final_output);
}
