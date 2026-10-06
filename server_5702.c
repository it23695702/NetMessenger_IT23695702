#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 11702

int main(void)
{
    int server_socket;
    int client_socket;
    char buffer[1024];
    ssize_t bytes_received;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    socklen_t client_length = sizeof(client_address);

    /* Step 1: Create a TCP socket */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Server socket created successfully.\n");

    /* Step 2: Prepare the server address */
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    /* Step 3: Bind the socket to our personalised port */
    if (bind(server_socket,
             (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0)
    {
        perror("bind");
        close(server_socket);
        return 1;
    }

    printf("Server bound to port %d.\n", PORT);

    /* Step 4: Start listening for clients */
    if (listen(server_socket, 5) < 0)
    {
        perror("listen");
        close(server_socket);
        return 1;
    }

    printf("NetMessenger server listening on port %d...\n", PORT);

    /* Step 5: Wait for one client */
    client_socket = accept(server_socket,
                           (struct sockaddr *)&client_address,
                           &client_length);

    if (client_socket < 0)
    {
        perror("accept");
        close(server_socket);
        return 1;
    }

    printf("A client connected successfully!\n");
/* receive REGISTER here */
memset(buffer, 0, sizeof(buffer));

bytes_received = recv(client_socket,
                      buffer,
                      sizeof(buffer) - 1,
                      0);

if (bytes_received <= 0)
{
    printf("Client disconnected before registration.\n");
    close(client_socket);
    close(server_socket);
    return 0;
}

buffer[bytes_received] = '\0';

printf("Received: %s", buffer);

/* process REGISTER here */
if (strncmp(buffer, "REGISTER ", 9) == 0)
{
    char username[64];

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

/* ONLY AFTER processing */
close(client_socket);
close(server_socket);

return 0;
}
