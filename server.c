#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>

#define BUFFER_SIZE 100
#define CMD_MAX_SIZE 30

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
int g_cmd_index_ack = -1;
pthread_mutex_t mtx_index_ack = PTHREAD_MUTEX_INITIALIZER;
int g_socket;
struct sockaddr_in g_server_address;
struct sockaddr_in g_client_address;

int setLastAddedIndex(int new_value);
int getLastAddedIndex();
int setLastAckIndex(int new_value);
int getLastAckIndex();
void sendAckMessage(cmd_id_t cmd_id);
void *ackThreadFunction();
void readCmd(char *message, int message_size);
void *receiverThreadFunction();
int initSocket();

int main()
{
    if (initSocket() < 0)
        return -1;

    pthread_t receiver_thread;
    pthread_create(&receiver_thread, NULL, receiverThreadFunction, NULL);

    pthread_t ack_thread;
    pthread_create(&ack_thread, NULL, ackThreadFunction, NULL);

    pthread_join(receiver_thread, NULL);
    pthread_join(ack_thread, NULL);

    return 0;
}

int initSocket()
{
    g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_socket < 0)
    {
        printf("[ERROR] Couldn't create socket.");
        return -1;
    }
    printf("Socket created...\n");

    g_server_address.sin_family = AF_INET;
    g_server_address.sin_port = htons(9000);
    g_server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(g_socket, (struct sockaddr *)&g_server_address, sizeof(g_server_address)) < 0)
    {
        printf("[ERROR] Couldn't bind socket.");
        return -1;
    }
    printf("Socket bound...\n");

    return 0;
}

/*
    Esta thread recebe e lida com comandos enviados pelo client
*/
void *receiverThreadFunction()
{
    struct sockaddr_in *client_addr;

    while (1)
    {
        int client_struct_length = sizeof(*client_addr);

        char client_message[CMD_MAX_SIZE];
        memset(client_message, 0, BUFFER_SIZE * sizeof(char));

        // Receive client's message:
        // MSG_WAITALL (since Linux 2.2)
        // This flag requests that the operation block until the full request is satisfied.
        // However, the call may still return less data than requested if a signal is caught,
        // an error or disconnect occurs, or the next data to be received is of a different
        // type than that returned.

        if (recvfrom(g_socket, client_message, BUFFER_SIZE, 0,
                     (struct sockaddr *restrict)client_addr, &client_struct_length) < 0)
        {
            printf("[ERROR] Couldn't receive message!\n");
        }

        readCmd(client_message, sizeof(client_message));
    }
}

/*
    Esta funcao lê a mensagem em string, transforma na struct Command e adiciona ao buffer de comandos
*/
void readCmd(char *message, int message_size)
{
    // TODO: Fazer esta função
    printf("Msg from client: %s\n", message);
    int last_added = getLastAddedIndex();

    struct Command cmd = {.cmd_id = last_added, .seq = 10, .value = 30};
    g_cmd_buffer[last_added + 1] = cmd;
    setLastAddedIndex(last_added + 1);
}

/*
    Esta thread manda mensagens de confirmação para o client
*/
void *ackThreadFunction()
{
    while (1)
    {
        int last_added = getLastAddedIndex();
        int last_ack = getLastAckIndex();

        if (last_added > last_ack)
        {
            // ENVIAR RESPOSTA PARA O CLIENT AQUI
            sendAckMessage(g_cmd_buffer[last_ack + 1].cmd_id);
            setLastAckIndex(last_ack + 1);
        }
    }
}

/*
    Esta funcao manda a resposta de confirmaçao para o client de acordo com o cmd_id
*/
void sendAckMessage(cmd_id_t cmd_id)
{
    printf("CMD_ID = %d ACK \n", cmd_id);
}

int getLastAckIndex()
{
    pthread_mutex_lock(&mtx_index_ack);
    int last_ack = g_cmd_index_ack;
    pthread_mutex_unlock(&mtx_index_ack);
    return last_ack;
}

int setLastAckIndex(int new_value)
{
    pthread_mutex_lock(&mtx_index_ack);
    g_cmd_index_ack = new_value;
    pthread_mutex_unlock(&mtx_index_ack);
    return new_value;
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