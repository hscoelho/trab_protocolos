#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include "graphing.h"

#define MAX_CMD_SIZE 100
#define PORT 8000
#define IP_ADDRESS "127.0.0.1"
#define TARGET_LEVEL 80
#define VALVE_OPENING 100

int g_sock;
struct sockaddr_in g_server_addr;
struct sockaddr_in g_client_addr;

int initConnection();

int initPlantComm();
int getAck(char *expected_msg);

void *controlThreadFunction();
int sendMsg(char *msg, int msg_size);
int receiveMsg(char *out_msg, int msg_size);

int g_curr_plant_level;
pthread_mutex_t mtx_plant_level = PTHREAD_MUTEX_INITIALIZER;
int getCurrPlantLevel();
int setCurrPlantLevel(int);
int g_curr_valve_level;
pthread_mutex_t mtx_valve_level = PTHREAD_MUTEX_INITIALIZER;
int getCurrValveLevel();
int setCurrValveLevel(int);

int getServerPlantLevel();
int decodePlantLevel(char *msg);

void *graphThreadFunction();

int main()
{
    if (initConnection() < 0)
        return -1;

    if (initPlantComm() < 0)
        return -1;

    srand(time(NULL));

    pthread_t control_thread;
    pthread_t graph_thread;

    pthread_create(&control_thread, NULL, controlThreadFunction, NULL);
    pthread_create(&graph_thread, NULL, graphThreadFunction, NULL);

    pthread_join(control_thread, NULL);
    pthread_join(graph_thread, NULL);

    return 0;
}

int initConnection()
{
    if ((g_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        return -1;
    }
    printf("Socket created!\n");

    memset(&g_server_addr, 0, sizeof(g_server_addr));      /* Clear struct */
    g_server_addr.sin_family = AF_INET;                    /* Internet/IP */
    g_server_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS); /* IP address */
    g_server_addr.sin_port = htons(PORT);                  /* server port */

    // while (connect(g_sock, (struct sockaddr *)&g_server_addr, sizeof(g_server_addr)) < 0)
    //{
    // }
    // printf("Connection established!\n");
}

int initPlantComm()
{
    bool connection_started = false;
    sendMsg("CommTest!", sizeof("CommTest!"));
    while (getAck("Comm#OK!") < 0)
    {
    }

    sendMsg("SetMax#100!", sizeof("SetMax#100!"));
    while (getAck("Max#100!") < 0)
    {
    }

    sendMsg("Start!", sizeof("Start!"));
    while (getAck("Start#OK!"))
    {
    }

    int seq = rand() % 10000;
    char cmd[MAX_CMD_SIZE];
    char ack[MAX_CMD_SIZE];
    snprintf(cmd, sizeof(cmd), "CloseValve#%d#%d!", seq, 100);
    snprintf(ack, sizeof(ack), "Close#%d!", seq);
    setCurrValveLevel(0);
    sendMsg(cmd, sizeof(cmd));
    if (getAck(ack) < 0)
    {
        printf("Received wrong ack!");
    }
    sleep(1); // tempo para fechar a valvula
}

int getAck(char *expected_msg)
{
    char ack[MAX_CMD_SIZE];
    receiveMsg(ack, sizeof(ack));
    if (strcmp(ack, expected_msg) != 0)
    {
        printf("Received ACK: %s, Expected: %s\n", ack, expected_msg);
        return -1;
    }
    return 0;
}

void *controlThreadFunction()
{
    int i = 0;
    bool is_valve_open = false;
    while (1)
    {
        int plant_level = setCurrPlantLevel(getServerPlantLevel());

        int seq = rand() % 10000;

        char cmd[MAX_CMD_SIZE];
        char ack[MAX_CMD_SIZE];
        bool send_cmd = false;
        if (plant_level < 80 && !is_valve_open)
        {
            is_valve_open = true;
            send_cmd = true;
            snprintf(cmd, sizeof(cmd), "OpenValve#%d#%d!", seq, VALVE_OPENING);
            snprintf(ack, sizeof(ack), "Open#%d!", seq);
            setCurrValveLevel(VALVE_OPENING);
        }
        else if (plant_level > 80 && is_valve_open)
        {
            is_valve_open = false;
            send_cmd = true;
            snprintf(cmd, sizeof(cmd), "CloseValve#%d#%d!", seq, VALVE_OPENING);
            snprintf(ack, sizeof(ack), "Close#%d!", seq);
            setCurrValveLevel(0);
        }

        if (send_cmd)
        {
            sendMsg(cmd, sizeof(cmd));
            if (getAck(ack) < 0)
            {
                printf("Received wrong ack!");
            }
        }

        usleep(500000);
    }
}

int sendMsg(char *msg, int msg_size)
{
    if (sendto(g_sock, msg, msg_size, 0, &g_server_addr, sizeof(g_server_addr)) < 0)
    {
        printf("Unable to send message\n");
        return -1;
    }

    printf("SENT: %s\n", msg);
}

int receiveMsg(char *out_msg, int msg_size)
{
    int addr_len = sizeof(g_client_addr);
    int received_bytes = recvfrom(g_sock, out_msg, msg_size, 0, &g_client_addr, &addr_len);
    if (received_bytes < 0)
    {
        printf("[ERROR] recv: %s", strerror(errno));
        return -1;
    }
    /* a msg pode vir sem \0 no final, entao é adicionado um \0 no final
        os que ja adicionam nao sao afetados */
    out_msg[received_bytes] = '\0';
    printf("RECEIVED: %s\n", out_msg);
    return 0;
}

int getCurrPlantLevel()
{
    pthread_mutex_lock(&mtx_plant_level);
    int val = g_curr_plant_level;
    pthread_mutex_unlock(&mtx_plant_level);
    return val;
}

int setCurrPlantLevel(int val)
{
    pthread_mutex_lock(&mtx_plant_level);
    g_curr_plant_level = val;
    pthread_mutex_unlock(&mtx_plant_level);
    return val;
}

int getCurrValveLevel()
{
    pthread_mutex_lock(&mtx_valve_level);
    int val = g_curr_valve_level;
    pthread_mutex_unlock(&mtx_valve_level);
    return val;
}

int setCurrValveLevel(int val)
{
    pthread_mutex_lock(&mtx_valve_level);
    g_curr_valve_level = val;
    pthread_mutex_unlock(&mtx_valve_level);
    return val;
}

int getServerPlantLevel()
{
    sendMsg("GetLevel!", sizeof("GetLevel!"));

    char resp[MAX_CMD_SIZE];
    receiveMsg(resp, sizeof(resp));

    return decodePlantLevel(resp);
}

int decodePlantLevel(char *msg)
{
    char resp[MAX_CMD_SIZE];
    snprintf(resp, sizeof(resp), "%s", msg); // isso é feito porque o strtok modifica a string

    char *ack = strtok(resp, "#");
    if (strcmp(ack, "Level") != 0)
        return -1;

    char *level_str = strtok(NULL, "#");
    int level = atoi(level_str);

    return level;
}

void *graphThreadFunction()
{
    Tdataholder *data;

    data = datainit(SCREEN_W, SCREEN_H, 300, 110, 0, 0, 0);

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    time_t start_time_s = start_time.tv_sec;

    while (true)
    {
        struct timespec curr_time;
        clock_gettime(CLOCK_MONOTONIC_RAW, &curr_time);
        time_t curr_time_s = curr_time.tv_sec;
        int curr_plant = getCurrPlantLevel();
        int curr_valve = getCurrValveLevel();
        datadraw(data, curr_time_s - start_time_s, curr_plant, curr_valve, 0);

        quitevent();
        usleep(50000);
    }
}