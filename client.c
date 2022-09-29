#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CMD_SIZE 20

int g_socket;

struct sockaddr_in g_server_address;

int initSocket()
{
    g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (g_socket < 0)
    {
        printf("Error while creating socket\n");
        return -1;
    }
    printf("Socket created successfully\n");

    // Set port and IP:
    g_server_address.sin_family = AF_INET;
    g_server_address.sin_port = htons(9000);
    g_server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    return 0;
}

void *transmitterThreadFunction()
{
    int i = 0;
    while (1)
    {
        char cmd[MAX_CMD_SIZE] = "Message";
        int server_struct_length = sizeof(g_server_address);

        if (sendto(g_socket, cmd, strlen(cmd), 0,
                   &g_server_address, server_struct_length) < 0)
        {
            printf("Unable to send message\n");
            return;
        }

        printf("Message number %d sent...\n", i++);

        sleep(5);
    }
}

int main()
{
    initSocket();

    pthread_t transmitter_thread;
    pthread_create(&transmitter_thread, NULL, transmitterThreadFunction, NULL);
    pthread_join(transmitter_thread, NULL);

    return 0;
}