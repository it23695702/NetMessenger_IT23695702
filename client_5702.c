#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 11702
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

int main(void)
{
    int client_socket;

    struct sockaddr_in server_address;

    char username[64];
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    ssize_t bytes_received;

    /*
     * Step 1: Create TCP socket.
     */
    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Client socket created successfully.\n");


    /*
     * Step 2: Prepare server address.
     */
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_address.sin_addr) <= 0)
    {
        perror("inet_pton");

        close(client_socket);

        return 1;
    }


    /*
     * Step 3: Connect to server.
     */
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


    /*
     * Step 4: REGISTER must be first command.
     */
    printf("Enter username: ");

    if (fgets(username,
              sizeof(username),
              stdin) == NULL)
    {
        close(client_socket);

        return 1;
    }

    /*
     * Remove newline from username.
     */
    username[strcspn(username, "\n")] = '\0';


    /*
     * Build REGISTER command.
     */
    snprintf(command,
             sizeof(command),
             "REGISTER %s\n",
             username);


    /*
     * Send REGISTER command.
     */
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
     * Receive REGISTER response.
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

    printf("Server: %s", response);


    /*
     * If REGISTER failed, do not continue.
     */
    if (strncmp(response, "OK REGISTERED", 13) != 0)
    {
        printf("Registration failed.\n");

        close(client_socket);

        return 1;
    }


    /*
     * Step 5:
     * Interactive command loop.
     */
    while (1)
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
         * Send command to server.
         */
        if (send(client_socket,
                 command,
                 strlen(command),
                 0) < 0)
        {
            perror("send");

            break;
        }


        /*
         * Wait for server response.
         */
        bytes_received =
            recv(client_socket,
                 response,
                 sizeof(response) - 1,
                 0);

        if (bytes_received <= 0)
        {
            printf("Server disconnected.\n");

            break;
        }

        response[bytes_received] = '\0';

        printf("Server: %s", response);


        /*
         * QUIT ends the client.
         */
        if (strncmp(command, "QUIT", 4) == 0)
        {
            break;
        }
    }


    close(client_socket);

    printf("Disconnected from server.\n");

    return 0;
}
