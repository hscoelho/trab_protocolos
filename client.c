#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#define MAX_CMD_SIZE 100
#define PORT 9000

int g_sock;
struct sockaddr_in g_server_addr;

int initConnection();
void *controlThreadFunction();
void sendMsg(char *msg, int msg_size);
int receiveAck(); // PRECISA DE UM TIMEOUT

int main()
{
    if (initConnection() < 0)
        return -1;

    pthread_t control_thread;
    pthread_create(&control_thread, NULL, controlThreadFunction, NULL);
    pthread_join(control_thread, NULL);

    return 0;
}

int initConnection()
{
    if ((g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        return -1;
    }
    printf("Socket created!\n");

    memset(&g_server_addr, 0, sizeof(g_server_addr));       /* Clear struct */
    g_server_addr.sin_family = AF_INET;                     /* Internet/IP */
    g_server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); /* IP address */
    g_server_addr.sin_port = htons(PORT);                   /* server port */

    if (connect(g_sock, (struct sockaddr *)&g_server_addr, sizeof(g_server_addr)) < 0)
    {
        return -1;
    }
    printf("Connection established!\n");
}

void *controlThreadFunction()
{
    int i = 0;
    while (1)
    {
        char msg[MAX_CMD_SIZE];
        snprintf(msg, sizeof(msg), "MESSAGE_%d", i++);

        do
        {
            sendMsg(msg, sizeof(msg));
        } while (receiveAck() < 0);

        sleep(5);
    }
}

void sendMsg(char *msg, int msg_size)
{
    if (sendto(g_sock, msg, msg_size, 0, &g_server_addr, sizeof(g_server_addr)) < 0)
    {
        printf("Unable to send message\n");
        return -1;
    }

    printf("Message: '%s' sent...\n", msg);
}

int receiveAck()
{
    // TODO?: ADICIONAR UM TIMEOUT
    char ack_msg[MAX_CMD_SIZE];

    if (recv(g_sock, ack_msg, sizeof(ack_msg), 0) < 0)
    {
        printf("[ERROR] recv: %s", strerror(errno));
        return -1;
    }
    printf("MESSAGE RECEIVED: '%s'\n", ack_msg);

    // TODO: TESTAR SE O ACK ESTA CERTO
    return 0;
}