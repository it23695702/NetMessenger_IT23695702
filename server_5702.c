#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 11702

/*
 * Handles one connected client.
 * Each client will run inside its own thread.
 */
void *handle_client(void *arg)
{
    int client_socket = *((int *)arg);

    char buffer[1024];
    ssize_t bytes_received;

    /* We already copied the socket descriptor,
       so this allocated memory is no longer needed. */
    free(arg);

    printf("Client thread started.\n");

    /*
     * First command must be REGISTER <username>
     */
    memset(buffer, 0, sizeof(buffer));

    bytes_received = recv(client_socket,
                          buffer,
                          sizeof(buffer) - 1,
                          0);

    if (bytes_received <= 0)
    {
        printf("Client disconnected before registration.\n");
        close(client_socket);
        return NULL;
    }

    buffer[bytes_received] = '\0';

    printf("Received: %s", buffer);

    /*
     * Check whether the first command is REGISTER.
     */
    if (strncmp(buffer, "REGISTER ", 9) == 0)
    {
        char username[64];

        /*
         * Extract username from:
         * REGISTER <username>
         */
        if (sscanf(buffer + 9, "%63s", username) == 1)
        {
            char response[256];

            snprintf(response,
                     sizeof(response),
                     "OK REGISTERED %s NID:6957\n",
                     username);

            send(client_socket,
                 response,
                 strlen(response),
                 0);

            printf("Registered username: %s\n", username);
        }
        else
        {
            const char *error =
                "ERR 005 INVALID_REGISTER NID:6957\n";

            send(client_socket,
                 error,
                 strlen(error),
                 0);
        }
    }
    else
    {
        const char *error =
            "ERR 005 REGISTER_REQUIRED NID:6957\n";

        send(client_socket,
             error,
             strlen(error),
             0);
    }

    /*
     * Keep the client connection alive.
     *
     * Later this loop will process:
     * LIST
     * BCAST
     * PMSG
     * JOIN
     * LEAVE
     * ROOMS
     * RMSG
     * SENDFILE
     * QUIT
     */
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        bytes_received = recv(client_socket,
                              buffer,
                              sizeof(buffer) - 1,
                              0);

        if (bytes_received <= 0)
        {
            printf("Client disconnected.\n");
            break;
        }

        buffer[bytes_received] = '\0';

        printf("Client message: %s", buffer);
    }

    close(client_socket);

    return NULL;
}


int main(void)
{
    int server_socket;

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    socklen_t client_length = sizeof(client_address);

    /*
     * Step 1:
     * Create an IPv4 TCP socket.
     */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Server socket created successfully.\n");

    /*
     * Step 2:
     * Prepare the server address.
     */
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    /*
     * Step 3:
     * Bind the server socket to personalised port 11702.
     */
    if (bind(server_socket,
             (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0)
    {
        perror("bind");
        close(server_socket);
        return 1;
    }

    printf("Server bound to port %d.\n", PORT);

    /*
     * Step 4:
     * Start listening for incoming client connections.
     */
    if (listen(server_socket, 5) < 0)
    {
        perror("listen");
        close(server_socket);
        return 1;
    }

    printf("NetMessenger server listening on port %d...\n",
           PORT);

    /*
     * Step 5:
     * Continuously accept clients.
     *
     * Each accepted client gets a separate thread.
     */
    while (1)
    {
        int *client_socket_ptr;
        pthread_t thread_id;

        /*
         * Allocate memory for this client's socket descriptor.
         */
        client_socket_ptr = malloc(sizeof(int));

        if (client_socket_ptr == NULL)
        {
            perror("malloc");
            continue;
        }

        /*
         * Wait for a new client connection.
         */
        *client_socket_ptr =
            accept(server_socket,
                   (struct sockaddr *)&client_address,
                   &client_length);

        if (*client_socket_ptr < 0)
        {
            perror("accept");
            free(client_socket_ptr);
            continue;
        }

        printf("A client connected successfully!\n");

        /*
         * Create a new thread to handle this client.
         */
        if (pthread_create(&thread_id,
                           NULL,
                           handle_client,
                           client_socket_ptr) != 0)
        {
            perror("pthread_create");

            close(*client_socket_ptr);
            free(client_socket_ptr);

            continue;
        }

        /*
         * Automatically release thread resources
         * when the client thread finishes.
         */
        pthread_detach(thread_id);
    }

    close(server_socket);

    return 0;
}
