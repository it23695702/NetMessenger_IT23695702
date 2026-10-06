#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 11702
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

int client_running = 1;


/*
 * Receiver thread.
 *
 * Continuously receives messages/events
 * from the server while the main thread
 * accepts keyboard commands.
 */
void *receive_messages(void *arg)
{
    int client_socket = *((int *)arg);

    char buffer[BUFFER_SIZE];

    ssize_t bytes_received;


    while (client_running)
    {
        memset(buffer, 0, sizeof(buffer));


        bytes_received =
            recv(client_socket,
                 buffer,
                 sizeof(buffer) - 1,
                 0);


        if (bytes_received <= 0)
        {
            if (client_running)
            {
                printf("\nServer disconnected.\n");
            }

            client_running = 0;

            break;
        }


        buffer[bytes_received] = '\0';


        printf("\nServer: %s",
               buffer);


        printf("> ");

        fflush(stdout);
    }


    return NULL;
}


int main(void)
{
    int client_socket;

    struct sockaddr_in server_address;

    char username[64];
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    ssize_t bytes_received;

    pthread_t receiver_thread;


    /* Create TCP socket. */
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


    /* Prepare server address. */
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


    /* Connect to server. */
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


    /* REGISTER */
    printf("Enter username: ");


    if (fgets(username,
              sizeof(username),
              stdin) == NULL)
    {
        close(client_socket);

        return 1;
    }


    username[strcspn(username, "\r\n")] = '\0';


    snprintf(command,
             sizeof(command),
             "REGISTER %s\n",
             username);


    if (send(client_socket,
             command,
             strlen(command),
             0) < 0)
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
        recv(client_socket,
             response,
             sizeof(response) - 1,
             0);


    if (bytes_received <= 0)
    {
        printf("Server disconnected.\n");

        close(client_socket);

        return 1;
    }


    response[bytes_received] = '\0';


    printf("Server: %s",
           response);


    /*
     * Registration failed.
     */
    if (strncmp(response,
                "OK REGISTERED",
                13) != 0)
    {
        printf("Registration failed.\n");

        close(client_socket);

        return 1;
    }


    /*
     * Start background receiver thread.
     */
    if (pthread_create(&receiver_thread,
                       NULL,
                       receive_messages,
                       &client_socket) != 0)
    {
        perror("pthread_create");

        close(client_socket);

        return 1;
    }


    /*
     * Main thread:
     * read and send commands.
     */
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


        if (send(client_socket,
                 command,
                 strlen(command),
                 0) < 0)
        {
            perror("send");

            break;
        }


        /*
         * If user typed QUIT,
         * allow server response briefly
         * and then stop.
         */
        if (strncmp(command,
                    "QUIT",
                    4) == 0)
        {
            sleep(1);

            client_running = 0;

            break;
        }
    }


    /*
     * Stop receiver thread.
     */
    client_running = 0;

    shutdown(client_socket,
             SHUT_RDWR);


    pthread_join(receiver_thread,
                 NULL);


    close(client_socket);


    printf("Disconnected from server.\n");


    return 0;
}
