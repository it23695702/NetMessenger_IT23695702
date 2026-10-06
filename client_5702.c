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
    char username[64];
    char command[128];
    char response[256];
    ssize_t bytes_received;
    struct sockaddr_in server_address;

    /* Step 1: Create a TCP socket */
    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Client socket created successfully.\n");

    /* Step 2: Prepare the server address */
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

    /* Step 3: Connect to the server */
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
printf("Enter username: ");
scanf("%63s", username);

snprintf(command,
         sizeof(command),
         "REGISTER %s\n",
         username);

send(client_socket,
     command,
     strlen(command),
     0);

bytes_received = recv(client_socket,
                      response,
                      sizeof(response) - 1,
                      0);

if (bytes_received > 0)
{
    response[bytes_received] = '\0';
    printf("Server: %s", response);
}
else
{
    printf("Server disconnected.\n");
}
    close(client_socket);

    return 0;
}
