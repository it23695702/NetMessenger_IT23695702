#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 11702
#define MAX_CLIENTS 10
#define MAX_ROOMS 10
#define USERNAME_SIZE 64
#define ROOM_NAME_SIZE 64
#define BUFFER_SIZE 2048

typedef struct
{
    int socket;
    char username[USERNAME_SIZE];
    int active;
} Client;

typedef struct
{
    char name[ROOM_NAME_SIZE];
    int member_sockets[MAX_CLIENTS];
    int member_count;
    int active;
} Room;


/* Shared data */
Client clients[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;


/* =========================================================
   CLIENT FUNCTIONS
   ========================================================= */

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


/* =========================================================
   ROOM FUNCTIONS
   ========================================================= */

int find_room(const char *room_name)
{
    int i;

    for (i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i].active &&
            strcmp(rooms[i].name, room_name) == 0)
        {
            return i;
        }
    }

    return -1;
}


int create_room(const char *room_name)
{
    int i;
    int j;

    for (i = 0; i < MAX_ROOMS; i++)
    {
        if (!rooms[i].active)
        {
            rooms[i].active = 1;

            strncpy(rooms[i].name,
                    room_name,
                    ROOM_NAME_SIZE - 1);

            rooms[i].name[ROOM_NAME_SIZE - 1] = '\0';

            rooms[i].member_count = 0;

            for (j = 0; j < MAX_CLIENTS; j++)
            {
                rooms[i].member_sockets[j] = -1;
            }

            return i;
        }
    }

    return -1;
}


int is_room_member(int room_index,
                   int client_socket)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (rooms[room_index].member_sockets[i]
            == client_socket)
        {
            return 1;
        }
    }

    return 0;
}


int add_room_member(int room_index,
                    int client_socket)
{
    int i;

    if (is_room_member(room_index,
                       client_socket))
    {
        return 1;
    }

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (rooms[room_index].member_sockets[i] == -1)
        {
            rooms[room_index].member_sockets[i]
                = client_socket;

            rooms[room_index].member_count++;

            return 1;
        }
    }

    return 0;
}


int remove_room_member(int room_index,
                       int client_socket)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (rooms[room_index].member_sockets[i]
            == client_socket)
        {
            rooms[room_index].member_sockets[i] = -1;

            if (rooms[room_index].member_count > 0)
            {
                rooms[room_index].member_count--;
            }

            return 1;
        }
    }

    return 0;
}


/*
 * When a client disconnects,
 * remove that socket from every room.
 */
void remove_client_from_all_rooms(int client_socket)
{
    int i;

    pthread_mutex_lock(&rooms_mutex);

    for (i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i].active)
        {
            remove_room_member(i,
                               client_socket);
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
}


/* =========================================================
   LIST
   ========================================================= */

void send_user_list(int client_socket)
{
    char response[BUFFER_SIZE];
    int i;

    strcpy(response, "OK USERS");

    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active)
        {
            strcat(response, " ");
            strcat(response, clients[i].username);
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    strcat(response, " NID:6957\n");

    send(client_socket,
         response,
         strlen(response),
         0);
}


/* =========================================================
   BCAST
   ========================================================= */

void broadcast_message(int sender_socket,
                       const char *from,
                       const char *message)
{
    char event[BUFFER_SIZE];
    int i;

    snprintf(event,
             sizeof(event),
             "MSG BCAST %s %s\n",
             from,
             message);

    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        /*
         * Assignment says broadcast to all OTHER clients.
         */
        if (clients[i].active &&
            clients[i].socket != sender_socket)
        {
            send(clients[i].socket,
                 event,
                 strlen(event),
                 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


/* =========================================================
   PRIVATE MESSAGE
   ========================================================= */

int private_message(const char *from,
                    const char *target,
                    const char *message)
{
    char event[BUFFER_SIZE];
    int i;

    snprintf(event,
             sizeof(event),
             "MSG PRIV %s %s\n",
             from,
             message);

    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            strcmp(clients[i].username,
                   target) == 0)
        {
            send(clients[i].socket,
                 event,
                 strlen(event),
                 0);

            pthread_mutex_unlock(&clients_mutex);

            return 1;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return 0;
}


/* =========================================================
   ROOMS COMMAND
   ========================================================= */

void send_rooms(int client_socket)
{
    char response[BUFFER_SIZE];
    int i;
    int first = 1;

    strcpy(response, "OK ROOMS ");

    pthread_mutex_lock(&rooms_mutex);

    for (i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i].active)
        {
            if (!first)
            {
                strcat(response, ",");
            }

            strcat(response,
                   rooms[i].name);

            first = 0;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);

    strcat(response,
           " NID:6957\n");

    send(client_socket,
         response,
         strlen(response),
         0);
}


/* =========================================================
   ROOM MESSAGE
   ========================================================= */

void send_room_message(int room_index,
                       int sender_socket,
                       const char *sender,
                       const char *message)
{
    char event[BUFFER_SIZE];
    int i;

    snprintf(event,
             sizeof(event),
             "MSG ROOM %s %s %s\n",
             rooms[room_index].name,
             sender,
             message);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (rooms[room_index].member_sockets[i] != -1 &&
            rooms[room_index].member_sockets[i]
                != sender_socket)
        {
            send(rooms[room_index].member_sockets[i],
                 event,
                 strlen(event),
                 0);
        }
    }
}


/* =========================================================
   CLIENT THREAD
   ========================================================= */

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
       ===================================================== */

    memset(buffer,
           0,
           sizeof(buffer));

    bytes_received =
        recv(client_socket,
             buffer,
             sizeof(buffer) - 1,
             0);

    if (bytes_received <= 0)
    {
        close(client_socket);

        return NULL;
    }

    buffer[bytes_received] = '\0';

    buffer[strcspn(buffer,
                   "\r\n")] = '\0';


    if (strncmp(buffer,
                "REGISTER ",
                9) != 0)
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

        close(client_socket);

        return NULL;
    }


    if (!add_client(client_socket,
                    username))
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
        memset(buffer,
               0,
               sizeof(buffer));

        bytes_received =
            recv(client_socket,
                 buffer,
                 sizeof(buffer) - 1,
                 0);


        if (bytes_received <= 0)
        {
            printf("%s disconnected.\n",
                   username);

            break;
        }


        buffer[bytes_received] = '\0';

        buffer[strcspn(buffer,
                       "\r\n")] = '\0';


        printf("Command from %s: %s\n",
               username,
               buffer);


        /* ================= LIST ================= */

        if (strcmp(buffer,
                   "LIST") == 0)
        {
            send_user_list(client_socket);
        }


        /* ================= BCAST ================= */

        else if (strncmp(buffer,
                         "BCAST ",
                         6) == 0)
        {
            char *message =
                buffer + 6;


            if (*message == '\0')
            {
                const char *error =
                    "ERR 005 INVALID_COMMAND NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);

                continue;
            }


            broadcast_message(client_socket,
                              username,
                              message);


            {
                const char *response =
                    "OK SENT NID:6957\n";

                send(client_socket,
                     response,
                     strlen(response),
                     0);
            }
        }


        /* ================= PMSG ================= */

        else if (strncmp(buffer,
                         "PMSG ",
                         5) == 0)
        {
            char target[USERNAME_SIZE];
            char message[BUFFER_SIZE];


            if (sscanf(buffer + 5,
                       "%63s %2047[^\n]",
                       target,
                       message) != 2)
            {
                const char *error =
                    "ERR 005 INVALID_COMMAND NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);

                continue;
            }


            if (private_message(username,
                                target,
                                message))
            {
                const char *response =
                    "OK SENT NID:6957\n";

                send(client_socket,
                     response,
                     strlen(response),
                     0);
            }
            else
            {
                const char *error =
                    "ERR 002 USER_NOT_FOUND NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);
            }
        }


        /* ================= JOIN ================= */

        else if (strncmp(buffer,
                         "JOIN ",
                         5) == 0)
        {
            char room_name[ROOM_NAME_SIZE];
            int room_index;


            if (sscanf(buffer + 5,
                       "%63s",
                       room_name) != 1)
            {
                const char *error =
                    "ERR 005 INVALID_COMMAND NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);

                continue;
            }


            pthread_mutex_lock(&rooms_mutex);


            room_index =
                find_room(room_name);


            /*
             * JOIN creates room if necessary.
             */
            if (room_index == -1)
            {
                room_index =
                    create_room(room_name);
            }


            if (room_index == -1)
            {
                pthread_mutex_unlock(&rooms_mutex);

                const char *error =
                    "ERR 005 SERVER_FULL NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);

                continue;
            }


            add_room_member(room_index,
                            client_socket);


            pthread_mutex_unlock(&rooms_mutex);


            {
                char response[256];

                snprintf(response,
                         sizeof(response),
                         "OK JOINED %s NID:6957\n",
                         room_name);

                send(client_socket,
                     response,
                     strlen(response),
                     0);
            }


            printf("%s joined room %s\n",
                   username,
                   room_name);
        }


        /* ================= LEAVE ================= */

        else if (strncmp(buffer,
                         "LEAVE ",
                         6) == 0)
        {
            char room_name[ROOM_NAME_SIZE];
            int room_index;


            if (sscanf(buffer + 6,
                       "%63s",
                       room_name) != 1)
            {
                const char *error =
                    "ERR 005 INVALID_COMMAND NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);

                continue;
            }


            pthread_mutex_lock(&rooms_mutex);


            room_index =
                find_room(room_name);


            if (room_index == -1)
            {
                pthread_mutex_unlock(&rooms_mutex);

                const char *error =
                    "ERR 003 ROOM_NOT_FOUND NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);

                continue;
            }


            remove_room_member(room_index,
                               client_socket);


            pthread_mutex_unlock(&rooms_mutex);


            {
                char response[256];

                snprintf(response,
                         sizeof(response),
                         "OK LEFT %s NID:6957\n",
                         room_name);

                send(client_socket,
                     response,
                     strlen(response),
                     0);
            }


            printf("%s left room %s\n",
                   username,
                   room_name);
        }


        /* ================= ROOMS ================= */

        else if (strcmp(buffer,
                        "ROOMS") == 0)
        {
            send_rooms(client_socket);
        }


        /* ================= RMSG ================= */

        else if (strncmp(buffer,
                         "RMSG ",
                         5) == 0)
        {
            char room_name[ROOM_NAME_SIZE];
            char message[BUFFER_SIZE];

            int room_index;


            if (sscanf(buffer + 5,
                       "%63s %2047[^\n]",
                       room_name,
                       message) != 2)
            {
                const char *error =
                    "ERR 005 INVALID_COMMAND NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);

                continue;
            }


            pthread_mutex_lock(&rooms_mutex);


            room_index =
                find_room(room_name);


            if (room_index == -1)
            {
                pthread_mutex_unlock(&rooms_mutex);

                const char *error =
                    "ERR 003 ROOM_NOT_FOUND NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);

                continue;
            }


            /*
             * Sender must belong to room.
             */
            if (!is_room_member(room_index,
                                client_socket))
            {
                pthread_mutex_unlock(&rooms_mutex);

                const char *error =
                    "ERR 003 ROOM_NOT_FOUND NID:6957\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);

                continue;
            }


            send_room_message(room_index,
                              client_socket,
                              username,
                              message);


            pthread_mutex_unlock(&rooms_mutex);


            {
                const char *response =
                    "OK SENT NID:6957\n";

                send(client_socket,
                     response,
                     strlen(response),
                     0);
            }
        }


        /* ================= QUIT ================= */

        else if (strcmp(buffer,
                        "QUIT") == 0)
        {
            const char *response =
                "OK BYE NID:6957\n";

            send(client_socket,
                 response,
                 strlen(response),
                 0);

            break;
        }


        /* ================= INVALID ================= */

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


    /* Remove client from rooms first. */
    remove_client_from_all_rooms(client_socket);


    if (registered)
    {
        remove_client(client_socket);
    }


    close(client_socket);


    printf("Client thread finished: %s\n",
           username);


    return NULL;
}


/* =========================================================
   MAIN
   ========================================================= */

int main(void)
{
    int server_socket;

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    socklen_t client_length;


    memset(clients,
           0,
           sizeof(clients));

    memset(rooms,
           0,
           sizeof(rooms));


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


    memset(&server_address,
           0,
           sizeof(server_address));


    server_address.sin_family =
        AF_INET;

    server_address.sin_addr.s_addr =
        INADDR_ANY;

    server_address.sin_port =
        htons(PORT);


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


    if (listen(server_socket,
               MAX_CLIENTS) < 0)
    {
        perror("listen");

        close(server_socket);

        return 1;
    }


    printf("NetMessenger server listening on port %d...\n",
           PORT);


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


        pthread_detach(thread_id);
    }


    close(server_socket);

    return 0;
}
