#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#define BUFFER_SIZE 100
#define MAX_CMD_SIZE 100

#define PORT 9000

enum CommandId
{
    OpenValve,
    CloseValve,
    GetLevel,
    CommTest,
    SetMax,
    Start,
    Unknown
};
typedef enum CommandId cmd_id_t;

struct Command
{
    cmd_id_t cmd_id;
    int seq;
    int value;
};

/*
    Sobre o buffer de comandos:
    Seria melhor se esse g_cmd_buffer fosse um buffer "circular"
    Outra estrutura de dados que seria indicada é o std::queue do C++, que é um buffer FIFO
*/

struct Command g_cmd_buffer[BUFFER_SIZE];
int g_cmd_index_added = -1;
pthread_mutex_t mtx_index_added = PTHREAD_MUTEX_INITIALIZER;
int g_cmd_index_exec = -1;
// pthread_mutex_t mtx_index_added = PTHREAD_MUTEX_INITIALIZER;

int g_serversock, g_clientsock;
struct sockaddr_in g_server_addr, g_client_addr;

int setLastAddedIndex(int new_value);
int getLastAddedIndex();

int initConnection();

void *connectionThreadFunction();
void readMsg(char *msg, int msg_size);
struct Command decodeCmd(char *message, int message_size);
void storeCmd(struct Command cmd);
bool hasReceived(int cmd_seq);

int main()
{
    if (initConnection() < 0)
    {
        return -1;
    }

    pthread_t connection_thread;
    pthread_create(&connection_thread, NULL, connectionThreadFunction, NULL);

    pthread_join(connection_thread, NULL);

    return 0;
}

int initConnection()
{
    /* Create the TCP socket */
    if ((g_serversock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        return -1;
    }
    printf("Socket created!\n");

    memset(&g_server_addr, 0, sizeof(g_server_addr));       /* Clear struct */
    g_server_addr.sin_family = AF_INET;                     /* Internet/IP */
    g_server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); /* Incoming addr */
    g_server_addr.sin_port = htons(PORT);                   /* server port */

    /* Bind the server socket */
    if (bind(g_serversock, (struct sockaddr *)&g_server_addr,
             sizeof(g_server_addr)) < 0)
    {
        return -1;
    }
    printf("Socket bound!\n");

    listen(g_serversock, 5);
    int sock_len = sizeof(g_client_addr);
    g_clientsock = accept(g_serversock, (struct sockaddr *)&g_client_addr, &sock_len);
    if (g_clientsock < 0)
    {
        printf("FAILED TO CONNECT! ERROR:%d\n", g_clientsock);
        return -1;
    }
    printf("Client connected! IP:%s\n", inet_ntoa(g_client_addr.sin_addr));

    return 0;
}

void *connectionThreadFunction()
{
    while (1)
    {
        char client_message[MAX_CMD_SIZE];
        // memset(client_message, 0, BUFFER_SIZE * sizeof(char));

        // Receive client's message:
        // MSG_WAITALL (since Linux 2.2)
        // This flag requests that the operation block until the full request is satisfied.
        // However, the call may still return less data than requested if a signal is caught,
        // an error or disconnect occurs, or the next data to be received is of a different
        // type than that returned.

        readMsg(client_message, sizeof(client_message));

        struct Command cmd = decodeCmd(client_message, sizeof(client_message));

        if (!hasReceived(cmd.seq))
        {
            sendAck(cmd);
            storeCmd(cmd);
        }
    }
}

void readMsg(char *msg, int msg_size)
{
    if (recv(g_clientsock, msg, msg_size, 0) < 0)
    {
        printf("[ERROR recvfrom] %s\n", strerror(errno));
    }
}

struct Command decodeCmd(char *message, int message_size)
{
    // TODO: Fazer esta função
    printf("Msg from client: %s\n", message);
    int last_added = getLastAddedIndex();

    struct Command cmd = {.cmd_id = last_added, .seq = 10, .value = 30};

    return cmd;
}

bool hasReceived(int cmd_seq)
{
    // TODO: adicionar teste se seq esta em um array
    return false;
}

void storeCmd(struct Command cmd)
{
    int last_added = getLastAddedIndex();
    g_cmd_buffer[last_added + 1] = cmd;
    setLastAddedIndex(last_added + 1);
    // TODO: adicionar seq a algum array
}

void sendAck(struct Command cmd)
{
    char ack_response[MAX_CMD_SIZE];
    snprintf(ack_response, sizeof(ack_response), "id=%d#seq=%d!value=%d", cmd.cmd_id, cmd.value, cmd.value);

    if (sendto(g_clientsock, ack_response, strlen(ack_response), 0,
               &g_client_addr, sizeof(g_client_addr)) < 0)
    {
        printf("Unable to send message\n");
        return;
    }
}

int getLastAddedIndex()
{
    pthread_mutex_lock(&mtx_index_added);
    int last_added = g_cmd_index_added;
    pthread_mutex_unlock(&mtx_index_added);
    return last_added;
}

int setLastAddedIndex(int new_value)
{
    pthread_mutex_lock(&mtx_index_added);
    g_cmd_index_added = new_value;
    pthread_mutex_unlock(&mtx_index_added);
    return new_value;
}