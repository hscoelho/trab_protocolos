#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>

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

struct Tank
{
    double level;
    double max_flux;
    double in_angle;
    double out_angle;
};

/*
    Sobre o buffer de comandos:
    Seria melhor se esse g_cmd_buffer fosse um buffer "circular"
    Outra estrutura de dados que seria indicada é o std::queue do C++, que é um buffer FIFO

    Acho que não vai precisar um buffer de comandos
*/
// struct Command g_cmd_buffer[BUFFER_SIZE];
// int g_cmd_index_added = -1;
// pthread_mutex_t mtx_index_added = PTHREAD_MUTEX_INITIALIZER;
// int g_cmd_index_exec = -1;
// pthread_mutex_t mtx_index_added = PTHREAD_MUTEX_INITIALIZER;

int g_serversock, g_clientsock;
struct sockaddr_in g_server_addr, g_client_addr;

// int setLastAddedIndex(int new_value);
// int getLastAddedIndex();

int initConnection();

void *connectionThreadFunction();
void *plantThreadFunction();

void readMsg(char *msg, int msg_size);
struct Command decodeCmd(char *message, int message_size);
void sendAck(struct Command cmd);
bool hasReceived(int cmd_seq);

char *proccessCmd(struct Command cmd);
char *openValve(int value, int seq);
char *closeValve(int value, int seq);
char *getLevel();
char *commTest();
char *setMax(int value);
char *start();

void testDecode();

int main()
{
    if (initConnection() < 0)
    {
        return -1;
    }

    pthread_t connection_thread;
    pthread_t plant_thread;

    /* Create a thread

        int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
            void *(*start_routine)(void*), void *arg);

        The pthread_create() function creates a new thread, with the attributes specified in the
        thread attribute object attr. The created thread inherits the signal mask of the parent
        thread, and its set of pending signals is empty.

        thread
            NULL, or a pointer to a pthread_t object where the function can store the thread ID of
            the new thread.
        attr
            Is the thread attribute object specifying the attributes for the thread that is being
            created. If attr is NULL, the thread is created with default attributes.
        start_routine
            The routine where the thread begins, with arg as its only argument. If start_routine()
            returns, there's an implicit call to pthread_exit(), using the return value of
            start_routine() as the exit status.
            The thread in which main() was invoked behaves differently. When it returns from main(),
            there's an implicit call to exit(), using the return value of main() as the exit status.
        arg
            The argument to pass to start_routine.
        return
            If successful, pthread_create() returns 0. If unsuccessful, pthread_create() returns -1
            and sets errno to one of the following values: EAGAIN, EINVAL, ELEMULTITHREADFORK and
            ENOMEM.
    */
    pthread_create(&connection_thread, NULL, connectionThreadFunction, NULL);
    pthread_create(&plant_thread, NULL, plantThreadFunction, NULL);

    /* Wait for a thread to end

        int pthread_join(pthread_t thread, void **value_ptr);

        The pthread_join() function blocks the calling thread until the target thread thread
        terminates, unless thread has already terminated. If value_ptr is non-NULL and
        pthread_join() returns successfully, then the value passed to pthread_exit() by the target
        thread is placed in value_ptr. If the target thread has been canceled then value_ptr is set
        to PTHREAD_CANCELED.

        thread
            The target thread whose termination you're waiting for.
        value_ptr
            NULL, or a pointer to a location where the function can store the value passed to
            pthread_exit() by the target thread.
        return
            If successful, pthread_join() returns 0. If unsuccessful, pthread_join() returns -1 and
            sets errno to one of the following values: EDEADLK, EINVAL, ESRCH.
    */
    pthread_join(connection_thread, NULL);
    pthread_join(plant_thread, NULL);

    return 0;
}

void *plantThreadFunction()
{
    float delta = 0;
    float in_flux = 0;
    float out_flux = 0;

    int T = 0;          // TODO: contar tempo
    int dT = 10;
    
    struct Tank tank = {
        .level = 0.4, 
        .max_flux = 100, 
        .in_angle = 50,
        .out_angle = 0
    };

    struct Command cmd = { 
        .cmd_id = Unknown,
        .seq = 0,
        .value = 0
    };
    
    while(1)
    {
        //if (cmd != NULL)          // TODO: verificar se tem comando pra usar
        //{
            switch (cmd.cmd_id)     // talvez mudar para cmd_id = Unknown quando cmd for usado
            {
            case OpenValve:
                delta += cmd.value;
                break;

            case CloseValve:
                delta += cmd.value;
                break;

            case SetMax:
                delta += cmd.value;
                break;

            default:
                break;
            }
        //}

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
            if (delta > -0.01 & dT)
            {
                tank.in_angle += delta;
                delta = 0;
            }
            else{
                tank.in_angle -= 0.01 * dT;
                delta += 0.01 * dT;
            }
        }
        
        tank.out_angle = tankOutAngle(T);
        
        in_flux = sin(M_PI / 2 * tank.in_angle / 100);
        out_flux = (tank.max_flux / 100) * (tank.level / 1.25 + 0.2) * 
            sin(M_PI / 2 * tank.out_angle / 100);
        tank.level += 0.00002 * dT * (in_flux - out_flux);
    }
}

int tankOutAngle(int T)
{
    if(T <= 0)
    {
        return 50;
    }
    else if(T <= 20000)
    {
        return 50 + T/400;
    }
    else if(T <= 30000)
    {
        return 100;
    }
    else if(T <= 50000)
    {
        return 100 - (T - 30000) / 250;
    }
    else if(T <= 70000)
    {
        return 20 - (T - 50000) / 1000;
    }
    else if(T <= 100000)
    {
        return 40 + 20 * cos((T - 70000) * 2 * M_PI / 10000);
    }
}

int initConnection()
{
    /* Create an endpoint for communication

        int socket(int domain, int type, int protocol);

        The socket() function shall create an unbound socket in a communications domain, and return
        a file descriptor that can be used in later function calls that operate on sockets.

        domain
            Specifies the communications domain in which a socket is to be created.
        type
            Specifies the type of socket to be created.
        protocol
            Specifies a particular protocol to be used with the socket. Specifying a protocol of 0
            causes socket() to use an unspecified default protocol appropriate for the requested
            socket type.
        return
            Upon successful completion, socket() shall return a non-negative integer, the socket
            file descriptor. Otherwise, a value of -1 shall be returned and errno set to indicate
            the error.

    */
    if ((g_serversock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        return -1;
    }
    printf("Socket created!\n");

    /* Fills a memory block with a defined value

        Sets the first num bytes of the block of memory pointed by ptr to the specified value
        (interpreted as an unsigned char).

        void * memset ( void * ptr, int value, size_t num );

        ptr
            Pointer to the block of memory to fill.
        value
            Value to be set. The value is passed as an int, but the function fills the block of
            memory using the unsigned char conversion of this value.
        num
            Number of bytes to be set to the value.
            size_t is an unsigned integral type.
        return
            This function returns a pointer to the memory area str.
    */
    memset(&g_server_addr, 0, sizeof(g_server_addr));       /* Clear struct */
    g_server_addr.sin_family = AF_INET;                     /* Internet/IP */
    g_server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); /* Incoming addr */
    g_server_addr.sin_port = htons(PORT);                   /* server port */

    /* Bind a name to a socket

         int bind(int socket, const struct sockaddr *address, socklen_t address_len);

         The bind() function shall assign a local socket address address to a socket identified by
         descriptor socket that has no local socket address assigned. Sockets created with the
         socket() function are initially unnamed; they are identified only by their address family.

         socket
             Specifies the file descriptor of the socket to be bound.
         address
             Points to a sockaddr structure containing the address to be bound to the socket. The
             length and format of the address depend on the address family of the socket.
         address_len
             Specifies the length of the sockaddr structure pointed to by the address argument.
         return
             Upon successful completion, bind() shall return 0; otherwise, -1 shall be returned and
             errno set to indicate the error.
    */
    if (bind(g_serversock, (struct sockaddr *)&g_server_addr, sizeof(g_server_addr)) < 0)
    {
        return -1;
    }
    printf("Socket bound!\n");

    /* Prepare the server for incoming client requests

        The listen() function applies only to stream sockets. It indicates a readiness to accept
        client connection requests, and creates a connection request queue of length backlog to
        queue incoming connection requests. Once full, additional connection requests are rejected.

        int listen(int socket, int backlog);

        socket
            The socket descriptor.
        backlog
            Defines the maximum length for the queue of pending connections.
        return
            If successful, listen() returns 0. If unsuccessful, listen() returns -1 and sets errno
            to one of the following values: EBADF, EDESTADDRREQ, EINVAL, ENOBUFS, ENOTSOCK and
            EOPNOTSUPP.

    */
    listen(g_serversock, 5);
    int sock_len = sizeof(g_client_addr);
    /* Accept a new connection on a socket

        int accept(int socket, struct sockaddr *restrict address, socklen_t *restrict address_len);

        The accept() function shall extract the first connection on the queue of pending
        connections, create a new socket with the same socket type protocol and address family as
        the specified socket, and allocate a new file descriptor for that socket.

        socket
            Specifies a socket that was created with socket(), has been bound to an address with
            bind(), and has issued a successful call to listen().
        address
            Either a null pointer, or a pointer to a sockaddr structure where the address of the
            connecting socket shall be returned.
        address_len
            Points to a socklen_t structure which on input specifies the length of the supplied
            sockaddr structure, and on output specifies the length of the stored address.
        return
            If successful, accept() returns a nonnegative socket descriptor. If unsuccessful,
            accept() returns -1 and sets errno to one of the following values: EAGAIN, EBADF,
            EFAULT, EINTR, EINVAL, EIO, EMFILE, EMVSERR, ENFILE, ENOBUFS, ENOTSOCK, EOPNOTSUPP and
            EWOULDBLOCK.
    */
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
            // processCmd
        }
    }
}

void readMsg(char *msg, int msg_size)
{
    /* Receive a message from a connected socket

        ssize_t recv(int socket, void *buffer, size_t length, int flags);

        The recv() function shall receive a message from a connection-mode or connectionless-mode
        socket. It is normally used with connected sockets because it does not permit the
        application to retrieve the source address of received data.

        socket
            Specifies the socket file descriptor.
        buffer
            Points to a buffer where the message should be stored.
        length
            Specifies the length in bytes of the buffer pointed to by the buffer argument.
        flags
            Specifies the type of message reception.
        return
            Upon successful completion, recv() shall return the length of the message in bytes. If
            no messages are available to be received and the peer has performed an orderly shutdown,
            recv() shall return 0. Otherwise, -1 shall be returned and errno set to indicate the
            error.
    */
    if (recv(g_clientsock, msg, msg_size, 0) < 0)
    {
        printf("[ERROR recvfrom] %s\n", strerror(errno));
    }
}

struct Command decodeCmd(char *message, int message_size)
{
    // TODO: Testar pra quando a mensagem não estiver no formato padrão: OpenValve##⟨value⟩!;
    printf("Msg from client: %s\n", message);

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
        printf("%s\n", array[0]);
    }
    // Testa se é <keyword>#<value>!
    else if (array[2] == NULL)
    {
        array[1] = strtok(array[1], "!");
        for (i = 0; i < 2; ++i)
            printf("%s\n", array[i]);
    }
    // Senão, <keyword>#<seq>#<value>!
    else
    {
        array[2] = strtok(array[2], "!");
        for (i = 0; i < 3; ++i)
            printf("%s\n", array[i]);
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

bool hasReceived(int cmd_seq)
{
    // TODO: adicionar teste se seq esta em um array
    return false;
}

void sendAck(struct Command cmd)
{
    char ack_response[MAX_CMD_SIZE];
    snprintf(ack_response, sizeof(ack_response), "id=%d#seq=%d!value=%d", cmd.cmd_id, cmd.value,
             cmd.value);
    // char ack_response -> proccessCmd(cmd);
    printf(ack_response); // debug

    if (sendto(g_clientsock, ack_response, strlen(ack_response), 0,
               &g_client_addr, sizeof(g_client_addr)) < 0)
    {
        printf("Unable to send message\n");
        return;
    }
}

char *proccessCmd(struct Command cmd)
{
    // TODO: implementar
    switch (cmd.cmd_id)
    {
    case OpenValve:
        return openValve(cmd.value, cmd.seq);

    case CloseValve:
        return closeValve(cmd.value, cmd.seq);

    case GetLevel:
        return getLevel();

    case CommTest:
        return commTest();

    case SetMax:
        return setMax(cmd.value);

    case Start:
        return start();

    default:
        return "Err!";
    }
}

char *openValve(int value, int seq)
{
    // TODO: implementar
    return "Open#<seq>!";
}

char *closeValve(int value, int seq)
{
    // TODO: implementar
    return "Close#<seq>!";
}

char *getLevel()
{
    // TODO: implementar
    return "Level#<seq>!";
}

char *commTest()
{
    // TODO: implementar
    return "Comm#OK!";
}

char *setMax(int value)
{
    // TODO: implementar
    return "Max#<value>!";
}

char *start()
{
    // TODO: implementar
    return "Start#OK!";
}

// int getLastAddedIndex()
// {
/* Wait for a lock on a mutex object

    int pthread_mutex_lock(pthread_mutex_t* mutex);

    Locks a mutex object, which identifies a mutex. Mutexes are used to protect shared
    resources. If the mutex is already locked by another thread, the thread waits for the mutex
    to become available. The thread that has locked a mutex becomes its current owner and
    remains the owner until the same thread has unlocked it.

    When the mutex has the attribute of recursive, the use of the lock may be different. When
    this kind of mutex is locked multiple times by the same thread, then a count is incremented
    and no waiting thread is posted. The owning thread must call pthread_mutex_unlock() the same
    number of times to decrement the count to zero.

    mutex
        A pointer to the pthread_mutex_t object that you want to lock.
    return
        If successful, pthread_mutex_lock() returns 0. If unsuccessful, pthread_mutex_lock()
        returns -1 and sets errno.
*/
// pthread_mutex_lock(&mtx_index_added);
// int last_added = g_cmd_index_added;
/* Unlock a mutex object

    int pthread_mutex_unlock(pthread_mutex_t* mutex);

    Releases a mutex object. If one or more threads are waiting to lock the mutex,
    pthread_mutex_unlock() causes one of those threads to return from pthread_mutex_lock() with
    the mutex object acquired. If no threads are waiting for the mutex, the mutex unlocks with
    no current owner.

    When the mutex has the attribute of recursive the use of the lock may be different. When
    this kind of mutex is locked multiple times by the same thread, then unlock will decrement
    the count and no waiting thread is posted to continue running with the lock. If the count is
    decremented to zero, then the mutex is released and if any thread is waiting it is posted.

    mutex
        A pointer to the pthread_mutex_t object that you want to unlock.
    return
        If successful, pthread_mutex_unlock() returns 0. If unsuccessful, pthread_mutex_unlock()
        returns -1 and sets errno.
*/
// pthread_mutex_unlock(&mtx_index_added);
// return last_added;
// }

// int setLastAddedIndex(int new_value)
// {
/* Wait for a lock on a mutex object

    int pthread_mutex_lock(pthread_mutex_t* mutex);

    Locks a mutex object, which identifies a mutex. Mutexes are used to protect shared
    resources. If the mutex is already locked by another thread, the thread waits for the mutex
    to become available. The thread that has locked a mutex becomes its current owner and
    remains the owner until the same thread has unlocked it.

    When the mutex has the attribute of recursive, the use of the lock may be different. When
    this kind of mutex is locked multiple times by the same thread, then a count is incremented
    and no waiting thread is posted. The owning thread must call pthread_mutex_unlock() the same
    number of times to decrement the count to zero.

    mutex
        A pointer to the pthread_mutex_t object that you want to lock.
    return
        If successful, pthread_mutex_lock() returns 0. If unsuccessful, pthread_mutex_lock()
        returns -1 and sets errno.
*/
// pthread_mutex_lock(&mtx_index_added);
// g_cmd_index_added = new_value;
/* Unlock a mutex object

    int pthread_mutex_unlock(pthread_mutex_t* mutex);

    Releases a mutex object. If one or more threads are waiting to lock the mutex,
    pthread_mutex_unlock() causes one of those threads to return from pthread_mutex_lock() with
    the mutex object acquired. If no threads are waiting for the mutex, the mutex unlocks with
    no current owner.

    When the mutex has the attribute of recursive the use of the lock may be different. When
    this kind of mutex is locked multiple times by the same thread, then unlock will decrement
    the count and no waiting thread is posted to continue running with the lock. If the count is
    decremented to zero, then the mutex is released and if any thread is waiting it is posted.

    mutex
        A pointer to the pthread_mutex_t object that you want to unlock.
    return
        If successful, pthread_mutex_unlock() returns 0. If unsuccessful, pthread_mutex_unlock()
        returns -1 and sets errno.
*/
// pthread_mutex_unlock(&mtx_index_added);
// return new_value;
// }

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