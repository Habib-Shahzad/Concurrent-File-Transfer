#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#define PORT 1000
#define INT_SIZE 4
#define LONG_SIZE 8
#define STRING_SIZE 100

int setupSocket(int i)
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    int opt = 1;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT + i);

    // Forcefully attaching socket to the port
    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                             (socklen_t *)&addrlen)) < 0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    return new_socket;
}

struct args
{
    int thread_number;
    char *chunk;
    long size;
};

// Check if the file path exists
int file_exists(char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

void slice_str(const char *str, char *buffer, size_t start, size_t end)
{
    size_t j = 0;
    for (size_t i = start; i <= end; ++i)
    {
        buffer[j++] = str[i];
    }
    buffer[j] = 0;
}

void *sendChunk(void *input)
{
    int thread_number = ((struct args *)input)->thread_number;
    char *chunk = ((struct args *)input)->chunk;
    long size = ((struct args *)input)->size;

    int port = setupSocket(thread_number);

    int chunk_of_chunk_size = 1000; // send 1000 bytes at one time

    if (size < chunk_of_chunk_size)
        send(port, chunk, size, 0);

    else
    {
        int number_of_chunks_of_chunk = size / chunk_of_chunk_size;
        int extra_space_left = size % chunk_of_chunk_size;

        int start = 0;
        int end = chunk_of_chunk_size;

        for (int i = 0; i < number_of_chunks_of_chunk; i++)
        {
            char *chunk_of_chunk = malloc(chunk_of_chunk_size);
            slice_str(chunk, chunk_of_chunk, start, end - 1);
            start = end;
            end += chunk_of_chunk_size;
            send(port, chunk_of_chunk, chunk_of_chunk_size, 0);
            free(chunk_of_chunk);
        }

        if (extra_space_left)
        {
            char *extra_space_of_chunk = malloc(extra_space_left);
            slice_str(chunk, extra_space_of_chunk, size - extra_space_left, size - 1);
            send(port, extra_space_of_chunk, extra_space_left, 0);
            free(extra_space_of_chunk);
        }
    }

    return NULL;
}

// Get the size of file in bytes
long getFileSize(char *fileName)
{
    FILE *file = fopen(fileName, "r");
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);
    return size;
}

int main(int argc, char const *argv[])
{
    printf("Welcome to process A, please enter a filename in process B. \n");

    int new_socket = setupSocket(-1);

    char path[STRING_SIZE] = {0};

    read(new_socket, path, STRING_SIZE);

    int file_found = file_exists(path);

    if (file_found)
    {
        char *file_found_str = malloc(INT_SIZE);
        sprintf(file_found_str, "%d", file_found);
        // Telling process B, we found the file.
        send(new_socket, file_found_str, INT_SIZE, 0);

        printf("Retrieved Path: %s\n", path);
        printf("File succesfully found.\n \n");
        long file_size = getFileSize(path);

        printf("File size: %ld\n", file_size);

        // Process B telling us the number of threads
        char *number_of_chunks_str = malloc(STRING_SIZE);
        read(new_socket, number_of_chunks_str, STRING_SIZE);
        int number_of_chunks = atoi(number_of_chunks_str);

        printf("Recieved number of chunks: %d\n", number_of_chunks);
        long chunk_size = file_size / number_of_chunks;
        printf("Chunk Size: %ld\n", chunk_size);

        //Since its an integer division, there would be some extra space left
        int extra_space_size = file_size % number_of_chunks;
        printf("Extra space size = %d\n", extra_space_size);

        // Telling process B the size of the file
        char *file_size_str = malloc(STRING_SIZE);
        sprintf(file_size_str, "%ld", file_size);
        send(new_socket, file_size_str, STRING_SIZE, 0);

        // Creating an array of threads
        pthread_t threads[number_of_chunks];

        FILE *fp = fopen(path, "rb");

        struct args *data[number_of_chunks];
        char *chunks[number_of_chunks];

        for (int i = 0; i < number_of_chunks; i++)
        {
            int size = chunk_size;
            if (i == number_of_chunks - 1)
                size += extra_space_size;
            chunks[i] = malloc(size);
            data[i] = (struct args *)malloc(sizeof(struct args));
        }

        for (int i = 0; i < number_of_chunks; i++)
        {
            int size = chunk_size;
            int offset = i * chunk_size;
            if (i == (number_of_chunks - 1))
            {
                size += extra_space_size;
                offset = file_size - size;
            }
            pread(fileno(fp), chunks[i], size, offset);
            data[i]->thread_number = i;
            data[i]->chunk = chunks[i];
            data[i]->size = size;
            pthread_create(&threads[i], NULL, sendChunk, (void *)data[i]);
        }

        for (int i = 0; i < number_of_chunks; i++)
        {
            pthread_join(threads[i], NULL);
            free(data[i]);
        }

        sleep(1);

        for (int i = 0; i < number_of_chunks; i++)
        {
            free(chunks[i]);
        }

        free(file_found_str);
        free(file_size_str);
        free(number_of_chunks_str);

        fclose(fp);
    }

    else
    {
        printf("Could not find the file: '%s'\n", path);
        exit(1);
    }

    return 0;
}