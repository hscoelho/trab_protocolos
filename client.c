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
    pthread_create(&control_thread, NULL, controlThreadFunction, NULL);
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
    pthread_join(control_thread, NULL);

    return 0;
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
            Specifies a particular protocol to be used with the socket. Specifying a protocol of 0 causes socket() to use an unspecified default protocol appropriate for the requested socket type. 

    */
    if ((g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
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
    g_server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); /* IP address */
    g_server_addr.sin_port = htons(PORT);                   /* server port */

    /* Connect a socket

        int connect(int socket, const struct sockaddr *address, socklen_t address_len);

        For stream sockets, the connect() call attempts to establish a connection between two 
        sockets. For datagram sockets, the connect() call specifies the peer for a socket. The 
        socket parameter is the socket used to originate the connection request. The connect() call 
        performs two tasks when called for a stream socket. First, it completes the binding 
        necessary for a stream socket (in case it has not been previously bound using the bind() 
        call). Second, it attempts to make a connection to another socket.

        socket
            Specifies the file descriptor associated with the socket.
        address
            Points to a sockaddr structure containing the peer address. The length and format of the 
            address depend on the address family of the socket.
        address_len
            Specifies the length of the sockaddr structure pointed to by the address argument. 
        return
            If successful, connect() returns 0. If unsuccessful, connect() returns -1 and sets 
            errno.
    */
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
    /* Send data on a socket

    ssize_t sendto(int socket, const void *message, size_t length, int flags, 
        const struct sockaddr *dest_addr, socklen_t dest_len);

        The sendto() function shall send a message through a connection-mode or connectionless-mode 
        socket. If the socket is connectionless-mode, the message shall be sent to the address 
        specified by dest_addr. If the socket is connection-mode, dest_addr shall be ignored.
        
        socket
            Specifies the socket file descriptor.
        message
            Points to a buffer containing the message to be sent.
        length
            Specifies the size of the message in bytes.
        flags
            Specifies the type of message transmission.
        dest_addr
            Points to a sockaddr structure containing the destination address. The length and format 
            of the address depend on the address family of the socket.
        dest_len
            Specifies the length of the sockaddr structure pointed to by the dest_addr argument. 
        return
            If successful, sendto() returns the number of characters sent.
            A value of 0 or greater indicates the number of bytes sent, however, this does not 
            assure that data delivery was complete. A connection can be dropped by a peer socket and 
            a SIGPIPE signal generated at a later time if data delivery is not complete.
            No indication of failure to deliver is implied in the return value of this call when 
            used with datagram sockets.
            If unsuccessful, sendto() returns -1 and sets errno.
    */
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
    if (recv(g_sock, ack_msg, sizeof(ack_msg), 0) < 0)
    {
        printf("[ERROR] recv: %s", strerror(errno));
        return -1;
    }
    printf("MESSAGE RECEIVED: '%s'\n", ack_msg);

    // TODO: TESTAR SE O ACK ESTA CERTO
    return 0;
}