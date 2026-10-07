#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 11702
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 2048
#define FILE_CHUNK 4096
#define MAX_FILE_SIZE (10 * 1024 * 1024)

int client_running = 1;


/* =========================================================
   TCP HELPERS
   ========================================================= */

ssize_t recv_line(int socket_fd, char *buffer, size_t max_size)
{
    size_t total = 0;
    char ch;

    while (total < max_size - 1)
    {
        ssize_t n = recv(socket_fd, &ch, 1, 0);

        if (n <= 0)
        {
            return n;
        }

        if (ch == '\n')
        {
            break;
        }

        if (ch != '\r')
        {
            buffer[total++] = ch;
        }
    }

    buffer[total] = '\0';

    return (ssize_t)total;
}


int send_all(int socket_fd, const void *buffer, size_t size)
{
    const char *data = (const char *)buffer;
    size_t total = 0;

    while (total < size)
    {
        ssize_t n = send(socket_fd,
                         data + total,
                         size - total,
                         0);

        if (n <= 0)
        {
            return -1;
        }

        total += (size_t)n;
    }

    return 0;
}


int recv_exact(int socket_fd, void *buffer, size_t size)
{
    char *data = (char *)buffer;
    size_t total = 0;

    while (total < size)
    {
        ssize_t n = recv(socket_fd,
                         data + total,
                         size - total,
                         0);

        if (n <= 0)
        {
            return -1;
        }

        total += (size_t)n;
    }

    return 0;
}


/* =========================================================
   SEND FILE
   ========================================================= */

int send_file_command(int client_socket, const char *command)
{
    char target[64];
    char filename[256];
    char header[1024];

    FILE *fp;

    long file_size;

    char file_buffer[FILE_CHUNK];

    size_t bytes_read;


    /*
     * User types:
     *
     * SENDFILE Ravi test.txt
     */
    if (sscanf(command,
               "SENDFILE %63s %255s",
               target,
               filename) != 2)
    {
        printf("Usage: SENDFILE <user> <filename>\n");

        return 0;
    }


    /*
     * Basic filename safety.
     */
    if (strstr(filename, "..") != NULL)
    {
        printf("Invalid filename.\n");

        return 0;
    }


    /*
     * Open file in binary mode.
     */
    fp = fopen(filename, "rb");


    if (fp == NULL)
    {
        printf("File not found: %s\n",
               filename);

        return 0;
    }


    /*
     * Find file size.
     */
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        perror("fseek");

        fclose(fp);

        return 0;
    }


    file_size = ftell(fp);


    if (file_size < 0)
    {
        perror("ftell");

        fclose(fp);

        return 0;
    }


    if (file_size > MAX_FILE_SIZE)
    {
        printf("File is too large. Maximum is 10 MB.\n");

        fclose(fp);

        return 0;
    }


    rewind(fp);


    /*
     * Send:
     *
     * SENDFILE Ravi test.txt 42\n
     */
    snprintf(header,
             sizeof(header),
             "SENDFILE %s %s %ld\n",
             target,
             filename,
             file_size);


    if (send_all(client_socket,
                 header,
                 strlen(header)) < 0)
    {
        perror("send");

        fclose(fp);

        return 0;
    }


    /*
     * Send raw file bytes.
     *
     * IMPORTANT:
     * fread arguments:
     *
     * 1. file_buffer
     * 2. 1
     * 3. sizeof(file_buffer)
     * 4. fp
     */
    while ((bytes_read = fread(file_buffer,
                               1,
                               sizeof(file_buffer),
                               fp)) > 0)
    {
        if (send_all(client_socket,
                     file_buffer,
                     bytes_read) < 0)
        {
            perror("send");

            fclose(fp);

            return 0;
        }
    }


    if (ferror(fp))
    {
        perror("fread");

        fclose(fp);

        return 0;
    }


    fclose(fp);


    printf("Uploading %s (%ld bytes) to %s...\n",
           filename,
           file_size,
           target);


    return 1;
}


/* =========================================================
   RECEIVE FILE
   ========================================================= */

int receive_file(int client_socket, const char *header)
{
    char sender[64];
    char filename[256];

    long long file_size_ll;

    size_t file_size;

    char output_name[512];

    FILE *fp;

    char file_buffer[FILE_CHUNK];

    size_t remaining;


    /*
     * Server sends:
     *
     * FILE Amal test.txt 42
     */
    if (sscanf(header,
               "FILE %63s %255s %lld",
               sender,
               filename,
               &file_size_ll) != 3)
    {
        printf("\nInvalid FILE header from server.\n");

        return 0;
    }


    if (file_size_ll < 0 ||
        file_size_ll > MAX_FILE_SIZE)
    {
        printf("\nInvalid incoming file size.\n");

        return 0;
    }


    file_size = (size_t)file_size_ll;


    /*
     * Prevent dangerous filenames.
     */
    if (strstr(filename, "..") != NULL ||
        strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL)
    {
        printf("\nInvalid incoming filename.\n");

        /*
         * Still consume raw bytes.
         */
        remaining = file_size;

        while (remaining > 0)
        {
            size_t chunk;

            if (remaining > sizeof(file_buffer))
            {
                chunk = sizeof(file_buffer);
            }
            else
            {
                chunk = remaining;
            }


            if (recv_exact(client_socket,
                           file_buffer,
                           chunk) < 0)
            {
                return 0;
            }


            remaining -= chunk;
        }


        return 0;
    }


    /*
     * Save received file as:
     *
     * received_test.txt
     */
    snprintf(output_name,
             sizeof(output_name),
             "received_%s",
             filename);


    fp = fopen(output_name, "wb");


    if (fp == NULL)
    {
        perror("fopen");

        /*
         * Consume incoming bytes anyway.
         */
        remaining = file_size;

        while (remaining > 0)
        {
            size_t chunk;

            if (remaining > sizeof(file_buffer))
            {
                chunk = sizeof(file_buffer);
            }
            else
            {
                chunk = remaining;
            }


            if (recv_exact(client_socket,
                           file_buffer,
                           chunk) < 0)
            {
                return 0;
            }


            remaining -= chunk;
        }


        return 0;
    }


    remaining = file_size;


    /*
     * Receive exactly file_size raw bytes.
     */
    while (remaining > 0)
    {
        size_t chunk;


        if (remaining > sizeof(file_buffer))
        {
            chunk = sizeof(file_buffer);
        }
        else
        {
            chunk = remaining;
        }


        if (recv_exact(client_socket,
                       file_buffer,
                       chunk) < 0)
        {
            printf("\nFile transfer interrupted.\n");

            fclose(fp);

            return 0;
        }


        if (fwrite(file_buffer,
                   1,
                   chunk,
                   fp) != chunk)
        {
            printf("\nCould not save received file.\n");

            fclose(fp);

            return 0;
        }


        remaining -= chunk;
    }


    fclose(fp);


    printf("\nServer: FILE %s %s %zu\n",
           sender,
           filename,
           file_size);


    printf("Received file saved as: %s\n",
           output_name);


    return 1;
}


/* =========================================================
   RECEIVER THREAD
   ========================================================= */

void *receive_messages(void *arg)
{
    int client_socket = *((int *)arg);

    char buffer[BUFFER_SIZE];

    ssize_t bytes_received;


    while (client_running)
    {
        bytes_received =
            recv_line(client_socket,
                      buffer,
                      sizeof(buffer));


        if (bytes_received <= 0)
        {
            if (client_running)
            {
                printf("\nServer disconnected.\n");
            }


            client_running = 0;

            break;
        }


        /*
         * Incoming file.
         */
        if (strncmp(buffer,
                    "FILE ",
                    5) == 0)
        {
            receive_file(client_socket,
                         buffer);
        }
        else
        {
            printf("\nServer: %s\n",
                   buffer);
        }


        printf("> ");

        fflush(stdout);
    }


    return NULL;
}


/* =========================================================
   MAIN
   ========================================================= */

int main(void)
{
    int client_socket;

    struct sockaddr_in server_address;

    char username[64];

    char command[BUFFER_SIZE];

    char response[BUFFER_SIZE];

    ssize_t bytes_received;

    pthread_t receiver_thread;


    /* =====================================================
       CREATE SOCKET
       ===================================================== */

    client_socket =
        socket(AF_INET,
               SOCK_STREAM,
               0);


    if (client_socket < 0)
    {
        perror("socket");

        return 1;
    }


    printf("Client socket created successfully.\n");


    /* =====================================================
       SERVER ADDRESS
       ===================================================== */

    memset(&server_address,
           0,
           sizeof(server_address));


    server_address.sin_family =
        AF_INET;


    server_address.sin_port =
        htons(PORT);


    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_address.sin_addr) <= 0)
    {
        perror("inet_pton");

        close(client_socket);

        return 1;
    }


    /* =====================================================
       CONNECT
       ===================================================== */

    printf("Connecting to server %s:%d...\n",
           SERVER_IP,
           PORT);


    if (connect(client_socket,
                (struct sockaddr *)&server_address,
                sizeof(server_address)) < 0)
    {
        perror("connect");

        close(client_socket);

        return 1;
    }


    printf("Connected to NetMessenger server successfully!\n");


    /* =====================================================
       REGISTER
       ===================================================== */

    printf("Enter username: ");


    if (fgets(username,
              sizeof(username),
              stdin) == NULL)
    {
        close(client_socket);

        return 1;
    }


    username[strcspn(username,
                     "\r\n")] = '\0';


    snprintf(command,
             sizeof(command),
             "REGISTER %s\n",
             username);


    if (send_all(client_socket,
                 command,
                 strlen(command)) < 0)
    {
        perror("send");

        close(client_socket);

        return 1;
    }


    /*
     * Receive REGISTER response before
     * starting receiver thread.
     */
    bytes_received =
        recv_line(client_socket,
                  response,
                  sizeof(response));


    if (bytes_received <= 0)
    {
        printf("Server disconnected.\n");

        close(client_socket);

        return 1;
    }


    printf("Server: %s\n",
           response);


    if (strncmp(response,
                "OK REGISTERED",
                13) != 0)
    {
        printf("Registration failed.\n");

        close(client_socket);

        return 1;
    }


    /* =====================================================
       START RECEIVER THREAD
       ===================================================== */

    if (pthread_create(&receiver_thread,
                       NULL,
                       receive_messages,
                       &client_socket) != 0)
    {
        perror("pthread_create");

        close(client_socket);

        return 1;
    }


    /* =====================================================
       COMMAND LOOP
       ===================================================== */

    while (client_running)
    {
        printf("> ");

        fflush(stdout);


        if (fgets(command,
                  sizeof(command),
                  stdin) == NULL)
        {
            break;
        }


        /*
         * Remove newline from keyboard input.
         */
        command[strcspn(command,
                        "\r\n")] = '\0';


        /*
         * Ignore empty command.
         */
        if (command[0] == '\0')
        {
            continue;
        }


        /* =================================================
           SENDFILE

           Example:
           SENDFILE Ravi test.txt
           ================================================= */

        if (strncmp(command,
                    "SENDFILE ",
                    9) == 0)
        {
            send_file_command(client_socket,
                              command);

            continue;
        }


        /* =================================================
           NORMAL COMMAND
           ================================================= */

        {
            char outgoing[BUFFER_SIZE + 2];


            snprintf(outgoing,
                     sizeof(outgoing),
                     "%s\n",
                     command);


            if (send_all(client_socket,
                         outgoing,
                         strlen(outgoing)) < 0)
            {
                perror("send");

                break;
            }
        }


        /* =================================================
           QUIT
           ================================================= */

        if (strcmp(command,
                   "QUIT") == 0)
        {
            sleep(1);

            client_running = 0;

            break;
        }
    }


    /* =====================================================
       CLEANUP
       ===================================================== */

    client_running = 0;


    shutdown(client_socket,
             SHUT_RDWR);


    pthread_join(receiver_thread,
                 NULL);


    close(client_socket);


    printf("Disconnected from server.\n");


    return 0;
}
