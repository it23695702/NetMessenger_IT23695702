#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define PORT 11702
#define MAX_CLIENTS 10
#define MAX_ROOMS 10
#define USERNAME_SIZE 64
#define ROOM_NAME_SIZE 64
#define BUFFER_SIZE 2048
#define FILE_CHUNK 4096
#define MAX_FILE_SIZE (10 * 1024 * 1024)

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

Client clients[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;


/* =========================================================
   TCP HELPER FUNCTIONS
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


int add_client(int socket_fd, const char *username)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (!clients[i].active)
        {
            clients[i].socket = socket_fd;

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


void remove_client(int socket_fd)
{
    int i;

    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            clients[i].socket == socket_fd)
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


int find_user_socket(const char *username)
{
    int i;
    int result = -1;

    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            strcmp(clients[i].username, username) == 0)
        {
            result = clients[i].socket;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return result;
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


int is_room_member(int room_index, int socket_fd)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (rooms[room_index].member_sockets[i] == socket_fd)
        {
            return 1;
        }
    }

    return 0;
}


int add_room_member(int room_index, int socket_fd)
{
    int i;

    if (is_room_member(room_index, socket_fd))
    {
        return 1;
    }

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (rooms[room_index].member_sockets[i] == -1)
        {
            rooms[room_index].member_sockets[i] = socket_fd;
            rooms[room_index].member_count++;

            return 1;
        }
    }

    return 0;
}


int remove_room_member(int room_index, int socket_fd)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (rooms[room_index].member_sockets[i] == socket_fd)
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


void remove_client_from_all_rooms(int socket_fd)
{
    int i;

    pthread_mutex_lock(&rooms_mutex);

    for (i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i].active)
        {
            remove_room_member(i, socket_fd);
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
}


/* =========================================================
   LIST
   ========================================================= */

void send_user_list(int socket_fd)
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

    send_all(socket_fd,
             response,
             strlen(response));
}


/* =========================================================
   BCAST
   ========================================================= */

void broadcast_message(int sender_socket,
                       const char *sender,
                       const char *message)
{
    char event[BUFFER_SIZE];
    int i;

    snprintf(event,
             sizeof(event),
             "MSG BCAST %s %s\n",
             sender,
             message);

    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            clients[i].socket != sender_socket)
        {
            send_all(clients[i].socket,
                     event,
                     strlen(event));
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


/* =========================================================
   PRIVATE MESSAGE
   ========================================================= */

int private_message(const char *sender,
                    const char *target,
                    const char *message)
{
    char event[BUFFER_SIZE];
    int target_socket;

    target_socket = find_user_socket(target);

    if (target_socket == -1)
    {
        return 0;
    }

    snprintf(event,
             sizeof(event),
             "MSG PRIV %s %s\n",
             sender,
             message);

    if (send_all(target_socket,
                 event,
                 strlen(event)) < 0)
    {
        return 0;
    }

    return 1;
}


/* =========================================================
   ROOMS
   ========================================================= */

void send_rooms(int socket_fd)
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

            strcat(response, rooms[i].name);

            first = 0;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);

    strcat(response, " NID:6957\n");

    send_all(socket_fd,
             response,
             strlen(response));
}


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
            rooms[room_index].member_sockets[i] != sender_socket)
        {
            send_all(rooms[room_index].member_sockets[i],
                     event,
                     strlen(event));
        }
    }
}


/* =========================================================
   FILE STORAGE
   ========================================================= */

void make_storage_directories(const char *sender)
{
    char path[512];

    mkdir("storage", 0755);

    mkdir("storage/IT23695702", 0755);

    snprintf(path,
             sizeof(path),
             "storage/IT23695702/%s",
             sender);

    mkdir(path, 0755);
}


int safe_filename(const char *filename)
{
    if (filename == NULL ||
        filename[0] == '\0')
    {
        return 0;
    }

    if (strstr(filename, "..") != NULL ||
        strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL)
    {
        return 0;
    }

    return 1;
}


/* =========================================================
   SENDFILE
   ========================================================= */

void handle_sendfile(int sender_socket,
                     const char *sender,
                     const char *command)
{
    char target[USERNAME_SIZE];
    char filename[256];

    long long file_size_ll;
    size_t file_size;

    char *file_data = NULL;

    char storage_path[1024];
    char file_header[1024];
    char response[1024];

    FILE *file;

    int target_socket;


    if (sscanf(command,
               "SENDFILE %63s %255s %lld",
               target,
               filename,
               &file_size_ll) != 3)
    {
        const char *error =
            "ERR 005 INVALID_COMMAND NID:6957\n";

        send_all(sender_socket,
                 error,
                 strlen(error));

        return;
    }


    if (!safe_filename(filename))
    {
        const char *error =
            "ERR 005 INVALID_COMMAND NID:6957\n";

        send_all(sender_socket,
                 error,
                 strlen(error));

        return;
    }


    if (file_size_ll < 0 ||
        file_size_ll > MAX_FILE_SIZE)
    {
        const char *error =
            "ERR 005 INVALID_FILE_SIZE NID:6957\n";

        send_all(sender_socket,
                 error,
                 strlen(error));

        return;
    }


    file_size = (size_t)file_size_ll;


    /*
     * Allocate at least one byte so a zero-size
     * file is also handled safely.
     */
    file_data = malloc(file_size > 0 ? file_size : 1);

    if (file_data == NULL)
    {
        const char *error =
            "ERR 005 SERVER_ERROR NID:6957\n";

        send_all(sender_socket,
                 error,
                 strlen(error));

        return;
    }


    /*
     * IMPORTANT:
     * Receive exactly the number of raw bytes
     * declared in the SENDFILE header.
     */
    if (file_size > 0 &&
        recv_exact(sender_socket,
                   file_data,
                   file_size) < 0)
    {
        printf("File receive failed from %s.\n",
               sender);

        free(file_data);

        return;
    }


    /*
     * Check whether target is currently online.
     */
    target_socket = find_user_socket(target);

    if (target_socket == -1)
    {
        const char *error =
            "ERR 002 USER_NOT_FOUND NID:6957\n";

        send_all(sender_socket,
                 error,
                 strlen(error));

        free(file_data);

        return;
    }


    /*
     * Create:
     * storage/IT23695702/<sender>/
     */
    make_storage_directories(sender);


    snprintf(storage_path,
             sizeof(storage_path),
             "storage/IT23695702/%s/%s",
             sender,
             filename);


    file = fopen(storage_path, "wb");

    if (file == NULL)
    {
        perror("fopen");

        {
            const char *error =
                "ERR 005 SERVER_ERROR NID:6957\n";

            send_all(sender_socket,
                     error,
                     strlen(error));
        }

        free(file_data);

        return;
    }


    if (file_size > 0)
    {
        if (fwrite(file_data,
                   1,
                   file_size,
                   file) != file_size)
        {
            fclose(file);

            free(file_data);

            {
                const char *error =
                    "ERR 005 SERVER_ERROR NID:6957\n";

                send_all(sender_socket,
                         error,
                         strlen(error));
            }

            return;
        }
    }


    fclose(file);


    printf("File stored: %s (%zu bytes)\n",
           storage_path,
           file_size);


    /*
     * Forward header to recipient.
     */
    snprintf(file_header,
             sizeof(file_header),
             "FILE %s %s %zu\n",
             sender,
             filename,
             file_size);


    if (send_all(target_socket,
                 file_header,
                 strlen(file_header)) < 0)
    {
        const char *error =
            "ERR 002 USER_NOT_FOUND NID:6957\n";

        send_all(sender_socket,
                 error,
                 strlen(error));

        free(file_data);

        return;
    }


    /*
     * Forward exact raw file bytes.
     */
    if (file_size > 0)
    {
        if (send_all(target_socket,
                     file_data,
                     file_size) < 0)
        {
            const char *error =
                "ERR 002 USER_NOT_FOUND NID:6957\n";

            send_all(sender_socket,
                     error,
                     strlen(error));

            free(file_data);

            return;
        }
    }


    snprintf(response,
             sizeof(response),
             "OK FILESENT %s %s NID:6957\n",
             target,
             filename);


    send_all(sender_socket,
             response,
             strlen(response));


    printf("File sent: %s -> %s : %s (%zu bytes)\n",
           sender,
           target,
           filename,
           file_size);


    free(file_data);
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

    bytes_received =
        recv_line(client_socket,
                  buffer,
                  sizeof(buffer));


    if (bytes_received <= 0)
    {
        close(client_socket);

        return NULL;
    }


    if (strncmp(buffer,
                "REGISTER ",
                9) != 0)
    {
        const char *error =
            "ERR 005 REGISTER_REQUIRED NID:6957\n";

        send_all(client_socket,
                 error,
                 strlen(error));

        close(client_socket);

        return NULL;
    }


    if (sscanf(buffer + 9,
               "%63s",
               username) != 1)
    {
        const char *error =
            "ERR 005 INVALID_REGISTER NID:6957\n";

        send_all(client_socket,
                 error,
                 strlen(error));

        close(client_socket);

        return NULL;
    }


    pthread_mutex_lock(&clients_mutex);


    if (username_exists(username))
    {
        pthread_mutex_unlock(&clients_mutex);

        {
            const char *error =
                "ERR 001 USERNAME_TAKEN NID:6957\n";

            send_all(client_socket,
                     error,
                     strlen(error));
        }

        close(client_socket);

        return NULL;
    }


    if (!add_client(client_socket,
                    username))
    {
        pthread_mutex_unlock(&clients_mutex);

        {
            const char *error =
                "ERR 005 SERVER_FULL NID:6957\n";

            send_all(client_socket,
                     error,
                     strlen(error));
        }

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

        send_all(client_socket,
                 response,
                 strlen(response));
    }


    printf("Registered username: %s\n",
           username);


    /* =====================================================
       COMMAND LOOP
       ===================================================== */

    while (1)
    {
        bytes_received =
            recv_line(client_socket,
                      buffer,
                      sizeof(buffer));


        if (bytes_received <= 0)
        {
            printf("%s disconnected.\n",
                   username);

            break;
        }


        printf("Command from %s: %s\n",
               username,
               buffer);


        /* ================= LIST ================= */

        if (strcmp(buffer, "LIST") == 0)
        {
            send_user_list(client_socket);
        }


        /* ================= BCAST ================= */

        else if (strncmp(buffer,
                         "BCAST ",
                         6) == 0)
        {
            char *message = buffer + 6;

            if (*message == '\0')
            {
                const char *error =
                    "ERR 005 INVALID_COMMAND NID:6957\n";

                send_all(client_socket,
                         error,
                         strlen(error));

                continue;
            }

            broadcast_message(client_socket,
                              username,
                              message);

            {
                const char *response =
                    "OK SENT NID:6957\n";

                send_all(client_socket,
                         response,
                         strlen(response));
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

                send_all(client_socket,
                         error,
                         strlen(error));

                continue;
            }


            if (private_message(username,
                                target,
                                message))
            {
                const char *response =
                    "OK SENT NID:6957\n";

                send_all(client_socket,
                         response,
                         strlen(response));
            }
            else
            {
                const char *error =
                    "ERR 002 USER_NOT_FOUND NID:6957\n";

                send_all(client_socket,
                         error,
                         strlen(error));
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

                send_all(client_socket,
                         error,
                         strlen(error));

                continue;
            }


            pthread_mutex_lock(&rooms_mutex);

            room_index =
                find_room(room_name);


            if (room_index == -1)
            {
                room_index =
                    create_room(room_name);
            }


            if (room_index == -1)
            {
                pthread_mutex_unlock(&rooms_mutex);

                {
                    const char *error =
                        "ERR 005 SERVER_FULL NID:6957\n";

                    send_all(client_socket,
                             error,
                             strlen(error));
                }

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

                send_all(client_socket,
                         response,
                         strlen(response));
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

                send_all(client_socket,
                         error,
                         strlen(error));

                continue;
            }


            pthread_mutex_lock(&rooms_mutex);

            room_index =
                find_room(room_name);


            if (room_index == -1)
            {
                pthread_mutex_unlock(&rooms_mutex);

                {
                    const char *error =
                        "ERR 003 ROOM_NOT_FOUND NID:6957\n";

                    send_all(client_socket,
                             error,
                             strlen(error));
                }

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

                send_all(client_socket,
                         response,
                         strlen(response));
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

                send_all(client_socket,
                         error,
                         strlen(error));

                continue;
            }


            pthread_mutex_lock(&rooms_mutex);

            room_index =
                find_room(room_name);


            if (room_index == -1)
            {
                pthread_mutex_unlock(&rooms_mutex);

                {
                    const char *error =
                        "ERR 003 ROOM_NOT_FOUND NID:6957\n";

                    send_all(client_socket,
                             error,
                             strlen(error));
                }

                continue;
            }


            if (!is_room_member(room_index,
                                client_socket))
            {
                pthread_mutex_unlock(&rooms_mutex);

                {
                    const char *error =
                        "ERR 003 ROOM_NOT_FOUND NID:6957\n";

                    send_all(client_socket,
                             error,
                             strlen(error));
                }

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

                send_all(client_socket,
                         response,
                         strlen(response));
            }
        }


        /* ================= SENDFILE ================= */

        else if (strncmp(buffer,
                         "SENDFILE ",
                         9) == 0)
        {
            handle_sendfile(client_socket,
                            username,
                            buffer);
        }


        /* ================= QUIT ================= */

        else if (strcmp(buffer,
                        "QUIT") == 0)
        {
            const char *response =
                "OK BYE NID:6957\n";

            send_all(client_socket,
                     response,
                     strlen(response));

            break;
        }


        /* ================= INVALID ================= */

        else
        {
            const char *error =
                "ERR 005 INVALID_COMMAND NID:6957\n";

            send_all(client_socket,
                     error,
                     strlen(error));
        }
    }


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

    int reuse = 1;


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


    /*
     * Allows quick server restart after testing.
     */
    setsockopt(server_socket,
               SOL_SOCKET,
               SO_REUSEADDR,
               &reuse,
               sizeof(reuse));


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
