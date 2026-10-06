#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 11702
#define SERVER_IP "127.0.0.1"

int main(void)
{
    int client_socket;

    struct sockaddr_in server_address;

    char username[64];
    char command[1024];
    char response[1024];

    ssize_t bytes_received;

    /*
     * Step 1:
     * Create an IPv4 TCP socket.
     */
    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Client socket created successfully.\n");

    /*
     * Step 2:
     * Prepare server address.
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
     * Step 3:
     * Connect to the NetMessenger server.
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
     * Step 4:
     * REGISTER must be the first command.
     */
    printf("Enter username: ");

    if (fgets(username, sizeof(username), stdin) == NULL)
    {
        close(client_socket);
        return 1;
    }

    username[strcspn(username, "\n")] = '\0';

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
     * Receive REGISTER response.
     */
    bytes_received = recv(client_socket,
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
     * Step 5:
     * Keep the client connected.
     *
     * The user can now type protocol commands.
     */
    while (1)
    {
        printf("> ");

        if (fgets(command,
                  sizeof(command),
                  stdin) == NULL)
        {
            break;
        }

        /*
         * fgets() already keeps the newline,
         * which matches our line-based protocol.
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
         * QUIT will later be handled properly
         * by the server.
         */
        if (strncmp(command, "QUIT", 4) == 0)
        {
            break;
        }

        /*
         * At this intermediate stage the server
         * does not yet send responses for LIST,
         * BCAST, etc., so we do not call recv()
         * here yet.
         */
    }

    close(client_socket);

    printf("Disconnected from server.\n");

    return 0;
}
