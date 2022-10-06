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

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define SEQ_BUF_SIZE 5000
#define MAX_CMD_SIZE 100

#define IP_ADDRESS "127.0.0.1"

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
        .cmd_id = Unknown,
        .seq = 0,
        .value = 0,
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
        .time = 0};

int g_socket;
struct sockaddr_in g_server_addr;
struct sockaddr_in g_client_addr;
bool g_plant_started = false;

int initConnection(int server_port);

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

float clamp(float value, float min, float max);
double tankOutAngle(double T);
void delayMsec(int msec);
int getMsecDiff(struct timespec end, struct timespec begin);

int main(int argc, char *argv[])
{
    int server_port;
    
    if (argc != 2) {
        printf("usage: %s <port>\n", argv[0]);
        exit(1);
    }

    server_port = atoi(argv[1]);
    if (initConnection(server_port) < 0)
    {
        return -1;
    }

    pthread_t connection_thread;
    pthread_create(&connection_thread, NULL, connectionThreadFunction, NULL);
    pthread_t plant_thread;
    pthread_create(&plant_thread, NULL, plantThreadFunction, NULL);
    pthread_t graph_thread;
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
    int dT = 10;

    while (1)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

        switch (command.cmd_id)
        {
        case OpenValve:
            delta += command.value;
            break;

        case CloseValve:
            delta -= command.value;
            break;

        case SetMax:
            tank.max_flux = command.value;
            break;

        default:
            break;
        }

        if (delta > 0)
        {
            if (delta < 0.01 * dT)
            {
                tank.in_angle = clamp(tank.in_angle + delta, 0, 100);
                delta = 0;
            }
            else
            {
                tank.in_angle = clamp(tank.in_angle + 0.01 * dT, 0, 100);
                delta -= 0.01 * dT;
            }
        }
        else
        {
            if (delta > -0.01 * dT)
            {
                tank.in_angle = clamp(tank.in_angle + delta, 0, 100);
                delta = 0;
            }
            else
            {
                tank.in_angle = clamp(tank.in_angle - 0.01 * dT, 0, 100);
                delta += 0.01 * dT;
            }
        }

        tank.out_angle = tankOutAngle(tank.time);

        in_flux = sin(M_PI / 2 * tank.in_angle / 100);
        out_flux = (tank.max_flux / 100) * (tank.level / 1.25 + 0.2) *
                   sin(M_PI / 2 * tank.out_angle / 100);
        tank.level += 0.00002 * dT * (in_flux - out_flux);
        tank.level = clamp(tank.level + 0.00002 * dT * (in_flux - out_flux), 0, 1);

        tank.time += dT;

        command.cmd_id = Unknown;

        clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

        // struct timespec diff = { .tv_sec = end_time.tv_sec - start_time.tv_sec,
        //     .tv_nsec = end_time.tv_nsec - start_time.tv_nsec };

        delayMsec(MAX(0, 10 - getMsecDiff(end_time, start_time)));
    }
}

void delayMsec(int msec)
{
    struct timespec sleep_time = {0};
    if (msec >= 1000)
    {
        sleep_time.tv_sec = msec / 1000;
        msec = msec % 1000;
    }
    sleep_time.tv_nsec = msec * 1000000;
    clock_nanosleep(CLOCK_MONOTONIC, 0, &sleep_time, NULL);
}

int getMsecDiff(struct timespec end, struct timespec begin)
{
    int sec = end.tv_sec - begin.tv_sec;
    int nsec = end.tv_nsec - begin.tv_nsec;
    return sec * 1000 + nsec / 1000000;
}

double tankOutAngle(double T)
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
    else
    {
        return 100;
    }
}

int initConnection(int server_port)
{
    if ((g_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        return -1;
    }
    printf("Socket created!\n");

    memset(&g_server_addr, 0, sizeof(g_server_addr));       /* Clear struct */
    g_server_addr.sin_family = AF_INET;                     /* Internet/IP */
    g_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);      /* Incoming addr */
    g_server_addr.sin_port = htons(server_port);            /* server port */

    if (bind(g_socket, (struct sockaddr *)&g_server_addr, sizeof(g_server_addr)) < 0)
    {
        return -1;
    }
    printf("Socket bound!\n");

    while (!g_plant_started)
    {
        char client_message[MAX_CMD_SIZE];
        readMsg(client_message, sizeof(client_message));
        struct Command cmd = decodeCmd(client_message, sizeof(client_message));
        handleCmd(cmd, NULL, 0);
    }
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

        delayMsec(5);
    }
}

int readMsg(char *msg, int msg_size)
{
    int client_addr_len = sizeof(g_client_addr);
    int received_bytes = recvfrom(g_socket, msg, msg_size, 0, (struct sockaddr *)&g_client_addr, &client_addr_len);
    if (received_bytes < 0)
    // if (recv(g_socket, msg, msg_size, 0) < 0)
    {
        printf("[ERROR recvfrom] %s\n", strerror(errno));
        return -1;
    }
    /* a msg pode vir sem \0 no final, entao é adicionado um \0 no final
        os que ja adicionam nao sao afetados */
    msg[received_bytes] = '\0';
    printf("RECEIVED: %s\n", msg);
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

void handleCmd(struct Command cmd, int *seq_buf, int *seq_buf_size)
{
    char ack_msg[MAX_CMD_SIZE];
    snprintf(ack_msg, sizeof(ack_msg), "");
    switch (cmd.cmd_id)
    {
    case OpenValve:
        if (g_plant_started)
        {
            seq_buf[(*seq_buf_size)++] = cmd.seq;
            snprintf(ack_msg, sizeof(ack_msg), "Open#%d!", cmd.seq);
        }
        break;
    case CloseValve:
        if (g_plant_started)
        {
            seq_buf[(*seq_buf_size)++] = cmd.seq;
            snprintf(ack_msg, sizeof(ack_msg), "Close#%d!", cmd.seq);
        }
        break;

    case GetLevel:
        if (g_plant_started)
        {
            snprintf(ack_msg, sizeof(ack_msg), "Level#%d!", (int)round(tank.level * 100.0));
        }
        break;

    case CommTest:
        snprintf(ack_msg, sizeof(ack_msg), "Comm#OK!");
        break;

    case SetMax:
        snprintf(ack_msg, sizeof(ack_msg), "Max#%d!", cmd.value);
        break;

    case Start:
        snprintf(ack_msg, sizeof(ack_msg), "Start#OK!");
        g_plant_started = true;
        break;

    default:
        snprintf(ack_msg, sizeof(ack_msg), "Err!");
        break;
    }

    sendMsg(ack_msg, sizeof(ack_msg));
}

int sendMsg(char *msg, int msg_size)
{
    if (sendto(g_socket, msg, msg_size, 0, (struct sockaddr *)&g_client_addr, sizeof(g_client_addr)) < 0)
    // if (send(g_socket, msg, msg_size, 0) < 0)
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

    struct timespec start_time, curr_time, end_time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

    while (true)
    {

        clock_gettime(CLOCK_MONOTONIC_RAW, &curr_time);
        double graph_time = (curr_time.tv_sec - start_time.tv_sec) +
                            (curr_time.tv_nsec / 1000000 - start_time.tv_nsec / 100000) / 1000.0;
        datadraw(data, graph_time, tank.level * 100.0, tank.in_angle, tank.out_angle);

        quitevent();
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
        delayMsec(MAX(0, 50 - getMsecDiff(end_time, curr_time)));
    }
}

float clamp(float value, float min, float max)
{
    if (value >= max)
    {
        return max;
    }
    else if (value <= min)
    {
        return min;
    }

    return value;
}