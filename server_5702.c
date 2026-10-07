#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 11702
#define MAX_CLIENTS 10
#define USERNAME_SIZE 64
#define BUFFER_SIZE 1024

typedef struct
{
    int socket;
    char username[USERNAME_SIZE];
    int active;
} Client;

Client clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


/* ---------------------------------------------------------
   Check whether username already exists.
   Caller must hold clients_mutex.
   --------------------------------------------------------- */
int username_exists(const char *username)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            strcmp(clients[i].username, username) == 0)
        {
            return 1;
        }
    }

    return 0;
}


/* ---------------------------------------------------------
   Add a registered client.
   Caller must hold clients_mutex.
   --------------------------------------------------------- */
int add_client(int socket, const char *username)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (!clients[i].active)
        {
            clients[i].socket = socket;

            strncpy(clients[i].username,
                    username,
                    USERNAME_SIZE - 1);

            clients[i].username[USERNAME_SIZE - 1] = '\0';

            clients[i].active = 1;

            return 1;
        }
    }

    return 0;
}


/* ---------------------------------------------------------
   Remove disconnected client.
   --------------------------------------------------------- */
void remove_client(int socket)
{
    int i;

    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            clients[i].socket == socket)
        {
            printf("Removing user: %s\n",
                   clients[i].username);

            clients[i].active = 0;
            clients[i].socket = -1;
            clients[i].username[0] = '\0';

            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


/* ---------------------------------------------------------
   Send list of currently connected users.

   Response:
   USERS <u1> <u2> ... NID:6957
   --------------------------------------------------------- */
void send_user_list(int client_socket)
{
    char response[BUFFER_SIZE];
    int i;

    strcpy(response, "USERS");

    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active)
        {
            if (strlen(response) +
                strlen(clients[i].username) +
                20 < sizeof(response))
            {
                strcat(response, " ");
                strcat(response, clients[i].username);
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    strcat(response, " NID:6957\n");

    send(client_socket,
         response,
         strlen(response),
         0);
}


/* ---------------------------------------------------------
   Broadcast message to all connected clients.

   Event:
   EVENT BCAST <from> <message>
   --------------------------------------------------------- */
void broadcast_message(const char *from,
                       const char *message)
{
    char event[BUFFER_SIZE];
    int i;

    snprintf(event,
             sizeof(event),
             "EVENT BCAST %s %s\n",
             from,
             message);

    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active)
        {
            send(clients[i].socket,
                 event,
                 strlen(event),
                 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


/* ---------------------------------------------------------
   Send private message to one connected user.

   Event:
   EVENT PMSG <from> <message>

   Return:
   1 = target found
   0 = target not found
   --------------------------------------------------------- */
int private_message(const char *from,
                    const char *target,
                    const char *message)
{
    char event[BUFFER_SIZE];
    int i;
    int found = 0;

    snprintf(event,
             sizeof(event),
             "EVENT PMSG %s %s\n",
             from,
             message);

    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            strcmp(clients[i].username, target) == 0)
        {
            send(clients[i].socket,
                 event,
                 strlen(event),
                 0);

            found = 1;

            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return found;
}


/* ---------------------------------------------------------
   Handle one connected client.
   Each client is handled by a separate pthread.
   --------------------------------------------------------- */
void *handle_client(void *arg)
{
    int client_socket = *((int *)arg);

    char buffer[BUFFER_SIZE];
    char username[USERNAME_SIZE];

    ssize_t bytes_received;

    int registered = 0;

    free(arg);

    printf("Client thread started.\n");


    /* =====================================================
       REGISTER
       First command must be REGISTER <username>
       ===================================================== */

    memset(buffer, 0, sizeof(buffer));

    bytes_received =
        recv(client_socket,
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


    if (strncmp(buffer, "REGISTER ", 9) != 0)
    {
        const char *error =
            "ERR 005 REGISTER_REQUIRED NID:6957\n";

        send(client_socket,
             error,
             strlen(error),
             0);

        close(client_socket);

        return NULL;
    }


    if (sscanf(buffer + 9,
               "%63s",
               username) != 1)
    {
        const char *error =
            "ERR 005 INVALID_REGISTER NID:6957\n";

        send(client_socket,
             error,
             strlen(error),
             0);

        close(client_socket);

        return NULL;
    }


    /* =====================================================
       CHECK DUPLICATE USERNAME
       ===================================================== */

    pthread_mutex_lock(&clients_mutex);

    if (username_exists(username))
    {
        pthread_mutex_unlock(&clients_mutex);

        const char *error =
            "ERR 001 USERNAME_TAKEN NID:6957\n";

        send(client_socket,
             error,
             strlen(error),
             0);

        printf("Username already taken: %s\n",
               username);

        close(client_socket);

        return NULL;
    }


    if (!add_client(client_socket, username))
    {
        pthread_mutex_unlock(&clients_mutex);

        const char *error =
            "ERR 005 SERVER_FULL NID:6957\n";

        send(client_socket,
             error,
             strlen(error),
             0);

        close(client_socket);

        return NULL;
    }

    pthread_mutex_unlock(&clients_mutex);

    registered = 1;


    /* REGISTER successful response */
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
    }


    printf("Registered username: %s\n",
           username);


    /* =====================================================
       COMMAND LOOP
       ===================================================== */

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        bytes_received =
            recv(client_socket,
                 buffer,
                 sizeof(buffer) - 1,
                 0);

        if (bytes_received <= 0)
        {
            printf("Client %s disconnected.\n",
                   username);

            break;
        }


        buffer[bytes_received] = '\0';

        printf("Command from %s: %s",
               username,
               buffer);


        /*
         * Remove \n or \r\n from command.
         */
        buffer[strcspn(buffer, "\r\n")] = '\0';


        /* =================================================
           LIST
           ================================================= */
        if (strcmp(buffer, "LIST") == 0)
        {
            send_user_list(client_socket);
        }


        /* =================================================
           BCAST <message>
           ================================================= */
        else if (strncmp(buffer,
                         "BCAST ",
                         6) == 0)
        {
            char *message = buffer + 6;

            if (strlen(message) == 0)
            {
                const char *error =
                    "ERR 005 INVALID_BCAST NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);

                continue;
            }


            /*
             * Acknowledge sender.
             */
            {
                const char *response =
                    "OK BCAST NID:6957\n";

                send(client_socket,
                     response,
                     strlen(response),
                     0);
            }


            /*
             * Send event to all connected clients.
             */
            broadcast_message(username,
                              message);


            printf("Broadcast from %s: %s\n",
                   username,
                   message);
        }


        /* =================================================
           PMSG <user> <message>
           ================================================= */
        else if (strncmp(buffer,
                         "PMSG ",
                         5) == 0)
        {
            char target[USERNAME_SIZE];
            char message[BUFFER_SIZE];

            /*
             * Example:
             *
             * PMSG Ravi Hello Ravi
             *
             * target  = Ravi
             * message = Hello Ravi
             */
            if (sscanf(buffer + 5,
                       "%63s %1019[^\n]",
                       target,
                       message) != 2)
            {
                const char *error =
                    "ERR 005 INVALID_PMSG NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);

                continue;
            }


            /*
             * Try to deliver private message.
             */
            if (private_message(username,
                                target,
                                message))
            {
                char response[256];

                snprintf(response,
                         sizeof(response),
                         "OK PMSG %s NID:6957\n",
                         target);

                send(client_socket,
                     response,
                     strlen(response),
                     0);


                printf("Private message from %s to %s: %s\n",
                       username,
                       target,
                       message);
            }
            else
            {
                const char *error =
                    "ERR 002 USER_NOT_FOUND NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);


                printf("PMSG target not found: %s\n",
                       target);
            }
        }


        /* =================================================
           QUIT
           ================================================= */
        else if (strcmp(buffer, "QUIT") == 0)
        {
            const char *response =
                "OK BYE NID:6957\n";

            send(client_socket,
                 response,
                 strlen(response),
                 0);


            printf("%s requested QUIT.\n",
                   username);

            break;
        }


        /* =================================================
           UNKNOWN / NOT YET IMPLEMENTED
           ================================================= */
        else
        {
            const char *error =
                "ERR 005 INVALID_COMMAND NID:6957\n";

            send(client_socket,
                 error,
                 strlen(error),
                 0);
        }
    }


    /*
     * Remove user from connected-user table.
     */
    if (registered)
    {
        remove_client(client_socket);
    }


    close(client_socket);


    printf("Client thread finished.\n");


    return NULL;
}


/* =========================================================
   MAIN SERVER
   ========================================================= */
int main(void)
{
    int server_socket;

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    socklen_t client_length;


    /*
     * Initialise shared client table.
     */
    memset(clients,
           0,
           sizeof(clients));


    /* =====================================================
       STEP 1 — CREATE TCP SOCKET
       ===================================================== */

    server_socket =
        socket(AF_INET,
               SOCK_STREAM,
               0);


    if (server_socket < 0)
    {
        perror("socket");

        return 1;
    }


    printf("Server socket created successfully.\n");


    /* =====================================================
       STEP 2 — PREPARE SERVER ADDRESS
       ===================================================== */

    memset(&server_address,
           0,
           sizeof(server_address));


    server_address.sin_family =
        AF_INET;


    server_address.sin_addr.s_addr =
        INADDR_ANY;


    server_address.sin_port =
        htons(PORT);


    /* =====================================================
       STEP 3 — BIND
       ===================================================== */

    if (bind(server_socket,
             (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0)
    {
        perror("bind");

        close(server_socket);

        return 1;
    }


    printf("Server bound to port %d.\n",
           PORT);


    /* =====================================================
       STEP 4 — LISTEN
       ===================================================== */

    if (listen(server_socket,
               MAX_CLIENTS) < 0)
    {
        perror("listen");

        close(server_socket);

        return 1;
    }


    printf("NetMessenger server listening on port %d...\n",
           PORT);


    /* =====================================================
       STEP 5 — ACCEPT CLIENTS CONTINUOUSLY
       ===================================================== */

    while (1)
    {
        int *client_socket_ptr;

        pthread_t thread_id;


        client_socket_ptr =
            malloc(sizeof(int));


        if (client_socket_ptr == NULL)
        {
            perror("malloc");

            continue;
        }


        client_length =
            sizeof(client_address);


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
         * Create one worker thread per client.
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
         * Automatically release thread resources.
         */
        pthread_detach(thread_id);
    }


    close(server_socket);

    return 0;
}
