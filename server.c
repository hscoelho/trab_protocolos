#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "graphing.h"

#define SEQ_BUF_SIZE 100
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

struct Command command =
{
    .cmd_id = Unknown;
    .seq = 0;
    .value = 0;
};

struct Tank
{
    double level;
    double max_flux;
    double in_angle;
    double out_angle;
    double time;
};

struct Tank tank =
{
    .level = 0.4,
    .max_flux = 100,
    .in_angle = 50,
    .out_angle = 0,
    .time = 0
};

int g_serversock, g_clientsock;
struct sockaddr_in g_server_addr, g_client_addr;

int initConnection();

void *connectionThreadFunction();
void *plantThreadFunction();

int readMsg(char *msg, int msg_size);
struct Command decodeCmd(char *message, int message_size);
void sendAck(struct Command cmd);
bool findSeq(int cmd_seq, int *seq_buf, int buf_size);
void handleCmd(struct Command cmd, int *seq_buf, int *seq_buf_size);
int sendMsg(char *msg, int msg_size);

void testDecode();

void *graphThreadFunction();

int main()
{
    if (initConnection() < 0)
    {
        return -1;
    }

    pthread_t connection_thread;
    pthread_t plant_thread;
    pthread_t graph_thread;

    pthread_create(&connection_thread, NULL, connectionThreadFunction, NULL);
    pthread_create(&plant_thread, NULL, plantThreadFunction, NULL);
    pthread_create(&graph_thread, NULL, graphThreadFunction, NULL);

    pthread_join(connection_thread, NULL);
    pthread_join(plant_thread, NULL);
    pthread_join(graph_thread, NULL);

    return 0;
}

void *plantThreadFunction()
{
    float delta = 0;
    float in_flux = 0;
    float out_flux = 0;

    struct timespec start_time;
    struct timespec end_time;
    long loop_time;
    long sleep_time;
    int dT = 10;

    // TODO: tornar global
    struct Command cmd = {
        .cmd_id = Unknown,
        .seq = 0,
        .value = 0};

    while (1)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

        // if (cmd != NULL)          // TODO: verificar se tem comando pra usar
        //{
        switch (cmd.cmd_id) // talvez mudar para cmd_id = Unknown quando cmd for usado
        {
        case OpenValve:
            delta += cmd.value;
            break;

        case CloseValve:
            delta += cmd.value;
            break;

        case SetMax:
            tank.max_flux = cmd.value;
            break;

        default:
            break;
        }
        //}

        printf("cmd: %d", cmd.cmd_id);
        printf("delta: %f", delta);
        printf("in_angle: %f", tank.in_angle);

        if (delta > 0)
        {
            if (delta < 0.01 * dT)
            {
                tank.in_angle += delta;
                delta = 0;
            }
            else
            {
                tank.in_angle += 0.01 * dT;
                delta -= 0.01 * dT;
            }
        }
        else
        {
            if (delta > -0.01 * dT)
            {
                tank.in_angle += delta;
                delta = 0;
            }
            else
            {
                tank.in_angle -= 0.01 * dT;
                delta += 0.01 * dT;
            }
        }

        tank.out_angle = tankOutAngle(tank.time);

        in_flux = sin(M_PI / 2 * tank.in_angle / 100);
        out_flux = (tank.max_flux / 100) * (tank.level / 1.25 + 0.2) *
                   sin(M_PI / 2 * tank.out_angle / 100);
        tank.level += 0.00002 * dT * (in_flux - out_flux);

        tank.time += dT;

        printf("delta: %f", delta);
        printf("level: %f", tank.level);

        printf("in_angle: %f", tank.in_angle);
        printf("out_angle: %f", tank.out_angle);

        printf("in_flux: %f", in_flux);
        printf("out_flux: %f", out_flux);

        printf("start time: %ld", start_time.tv_nsec);
        printf("end time: %ld", end_time.tv_nsec);

        clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
        loop_time = (end_time.tv_nsec - start_time.tv_nsec) / 1000;
        sleep_time = dT * 1000 - loop_time;

        printf("loop time: %ld", loop_time);
        printf("sleep time: %ld", sleep_time);

        printf("cmd: %d", command.cmd_id);
        command.cmd_id = Unknown;
        printf("cmd: %d", command.cmd_id);

        // mudar para clock_nanosleep() posteriormente
        usleep(fmax(sleep_time, 0));
    }
}

int tankOutAngle(int T)
{
    if (T <= 0)
    {
        return 50;
    }
    else if (T <= 20000)
    {
        return 50 + T / 400;
    }
    else if (T <= 30000)
    {
        return 100;
    }
    else if (T <= 50000)
    {
        return 100 - (T - 30000) / 250;
    }
    else if (T <= 70000)
    {
        return 20 - (T - 50000) / 1000;
    }
    else if (T <= 100000)
    {
        return 40 + 20 * cos((T - 70000) * 2 * M_PI / 10000);
    }
}

int initConnection()
{
    if ((g_serversock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        return -1;
    }
    printf("Socket created!\n");

    memset(&g_server_addr, 0, sizeof(g_server_addr));       /* Clear struct */
    g_server_addr.sin_family = AF_INET;                     /* Internet/IP */
    g_server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); /* Incoming addr */
    g_server_addr.sin_port = htons(PORT);                   /* server port */

    if (bind(g_serversock, (struct sockaddr *)&g_server_addr, sizeof(g_server_addr)) < 0)
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
    int seq_buf[SEQ_BUF_SIZE] = {0};
    int seq_buff_size;

    while (1)
    {
        char client_message[MAX_CMD_SIZE];

        readMsg(client_message, sizeof(client_message));

        struct Command cmd = decodeCmd(client_message, sizeof(client_message));

        if (!findSeq(cmd.seq, seq_buf, seq_buff_size))
        {
            command = cmd;
            handleCmd(cmd, seq_buf, &seq_buff_size);
        }
    }
}

int readMsg(char *msg, int msg_size)
{
    if (recv(g_clientsock, msg, msg_size, 0) < 0)
    {
        printf("[ERROR recvfrom] %s\n", strerror(errno));
        return -1;
    }
    printf("RECEIVED: %s\n", msg);
    return 0;
}

struct Command decodeCmd(char *message, int message_size)
{
    // TODO: Testar pra quando a mensagem não estiver no formato padrão: OpenValve##⟨value⟩!;

    // char buf[] = "GetLevel!"
    // char buf[] = "SetMax#⟨value⟩!";
    char buf[MAX_CMD_SIZE];
    snprintf(buf, sizeof(buf), "%s", message); // é feito isso porque o strtok é destrutivo
    int i = 0;
    char *array[3];
    memset(array, NULL, sizeof(array));

    char *p = strtok(buf, "#");
    while (p != NULL)
    {
        array[i++] = p;
        p = strtok(NULL, "#");
    }

    // Testa se é <keyword>!
    if (array[1] == NULL)
    {
        array[0] = strtok(array[0], "!");
    }
    // Testa se é <keyword>#<value>!
    else if (array[2] == NULL)
    {
        array[1] = strtok(array[1], "!");
    }
    // Senão, <keyword>#<seq>#<value>!
    else
    {
        array[2] = strtok(array[2], "!");
    }

    // transforma array em struct Command
    // se tiver erro ele não modifica e mantém o Unknown
    struct Command cmd = {0};
    cmd.cmd_id = Unknown;
    if (strcmp(array[0], "OpenValve") == 0)
    {
        if (array[1] != NULL &&
            array[2] != NULL)
        {
            cmd.cmd_id = OpenValve;
            cmd.seq = atoi(array[1]);
            cmd.value = atoi(array[2]);
        }
    }
    else if (strcmp(array[0], "CloseValve") == 0)
    {
        if (array[1] != NULL &&
            array[2] != NULL)
        {
            cmd.cmd_id = CloseValve;
            cmd.seq = atoi(array[1]);
            cmd.value = atoi(array[2]);
        }
    }
    else if (strcmp(array[0], "GetLevel") == 0)
    {
        if (array[1] == NULL &&
            array[2] == NULL)
        {
            cmd.cmd_id = GetLevel;
        }
    }
    else if (strcmp(array[0], "CommTest") == 0)
    {
        if (array[1] == NULL &&
            array[2] == NULL)
        {
            cmd.cmd_id = CommTest;
        }
    }
    else if (strcmp(array[0], "SetMax") == 0)
    {
        if (array[1] != NULL &&
            array[2] == NULL)
        {
            cmd.cmd_id = SetMax;
            cmd.value = atoi(array[1]);
        }
    }
    else if (strcmp(array[0], "Start") == 0)
    {
        if (array[1] == NULL &&
            array[2] == NULL)
        {
            cmd.cmd_id = Start;
        }
    }

    return cmd;
}

bool findSeq(int cmd_seq, int *seq_buf, int buf_size)
{
    int i = 0;
    while (i < buf_size)
    {
        if (seq_buf[i] == cmd_seq)
        {
            printf("SEQ %d ALREADY RECEIVED!\n", seq_buf[i]);
            return true;
        }
        i++;
    }
    return false;
}

handleCmd(struct Command cmd, int *seq_buf, int *seq_buf_size)
{
    char ack_msg[MAX_CMD_SIZE];
    snprintf(ack_msg, sizeof(ack_msg), "");
    switch (cmd.cmd_id)
    {
    case OpenValve:
        seq_buf[(*seq_buf_size)++] = cmd.seq;
        snprintf(ack_msg, sizeof(ack_msg), "Open#%d!", cmd.seq);
        break;
    case CloseValve:
        seq_buf[(*seq_buf_size)++] = cmd.seq;
        snprintf(ack_msg, sizeof(ack_msg), "Close#%d!", cmd.seq);
        break;

    case GetLevel:
        snprintf(ack_msg, sizeof(ack_msg), "Level#%d!", tank.level);
        break;

    case CommTest:
        snprintf(ack_msg, sizeof(ack_msg), "Comm#OK!");
        break;

    case SetMax:
        snprintf(ack_msg, sizeof(ack_msg), "Max#%d!", cmd.value);
        break;

    case Start:
        snprintf(ack_msg, sizeof(ack_msg), "Start#OK!");
        break;

    default:
        snprintf(ack_msg, sizeof(ack_msg), "Err!");
        break;
    }

    sendMsg(ack_msg, sizeof(ack_msg));
}

int sendMsg(char *msg, int msg_size)
{
    if (sendto(g_clientsock, msg, msg_size, 0, &g_client_addr, sizeof(g_client_addr)) < 0)
    {
        printf("Unable to send message\n");
        return -1;
    }
    printf("SENT: %s\n", msg);
    return 0;
}

void testDecode()
{

    struct Command cmd = {0};

    cmd = decodeCmd("ERROR", sizeof("ERROR"));
    printf("CMD STRUCT: id: %d, seq: %d, val: %d\n", cmd.cmd_id, cmd.seq, cmd.value);
    cmd = decodeCmd("OpenValve#99#99!", sizeof("OpenValve#99#99!"));
    printf("CMD STRUCT: id: %d, seq: %d, val: %d\n", cmd.cmd_id, cmd.seq, cmd.value);
    cmd = decodeCmd("GetLevel!", sizeof("GetLevel!"));
    printf("CMD STRUCT: id: %d, seq: %d, val: %d\n", cmd.cmd_id, cmd.seq, cmd.value);
    cmd = decodeCmd("SetMax#99!", sizeof("SetMax#99!"));
    printf("CMD STRUCT: id: %d, seq: %d, val: %d", cmd.cmd_id, cmd.seq, cmd.value);
}

void *graphThreadFunction()
{
    Tdataholder *data;

    data = datainit(SCREEN_W, SCREEN_H, 300, 110, tank.level, tank.in_angle, tank.out_angle);

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    time_t start_time_s = start_time.tv_sec;

    while (true)
    {
        struct timespec curr_time;
        clock_gettime(CLOCK_MONOTONIC_RAW, &curr_time);
        time_t curr_time_s = curr_time.tv_sec;
        datadraw(data, curr_time_s - start_time_s, tank.level, tank.in_angle, tank.out_angle);

        quitevent();
        sleep(1);
    }
}