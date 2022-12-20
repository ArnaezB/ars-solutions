#include <asm-generic/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>

#define BUF_SIZE 256
#define LEN 128
#define MAX_QUEUE 10

int sock; // socket

void signal_handler(int signal);
void child_func();

int main(int argc, char *argv[])
{
    int port = -1; // puerto
    int opt;       // opciones de programa

    struct sockaddr_in org_addr;  // socket direccion origen
    struct sockaddr_in dest_addr; // socket direccion destino
    struct servent *serv;         // servicio

    signal(SIGINT, signal_handler);

    while ((opt = getopt(argc, argv, "p:")) != -1) // extraccion de opciones
    {
        switch (opt)
        {
        case 'p':
            port = htons(atoi(optarg));
            break;
        default:
            break;
        }
    }

    if (port == -1) // no se aporta puerto en la entrada
    {
        serv = getservbyname("daytime", "tcp");
        port = serv->s_port;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) // creacion de socket
    {
        perror("socket()");
        exit(EXIT_FAILURE);
    }
    // parametros de socket origen
    org_addr.sin_family = AF_INET;
    org_addr.sin_port = port;
    org_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&org_addr, sizeof(org_addr)) == -1) // bind de socket a origen
    {
        perror("bind()");
        exit(EXIT_FAILURE);
    }

    if (listen(sock, MAX_QUEUE) == -1)
    {
        perror("listen()");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        int sock2;
        pid_t pid;
        unsigned int dest_addr_len = sizeof(dest_addr); // longitud de struct, necesario para el paso de su puntero

        if ((sock2 = accept(sock, (struct sockaddr *)&dest_addr, &dest_addr_len)) == -1)
        {
            perror("accept()");
            exit(EXIT_FAILURE);
        }

        if ((pid = fork()) == -1)
        {
            perror("fork()");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) // child
        {
            child_func(sock2);
        }
        close(sock2);
    }
}

void signal_handler(int signal)
{
    char buffer[BUF_SIZE];
    printf("%d", sock);
    fflush(stdout);
    if (shutdown(sock, SHUT_RDWR) == -1)
    {
        perror("shutdown()");
        exit(EXIT_FAILURE);
    }
    recv(sock, buffer, BUF_SIZE, 0);
    exit(EXIT_SUCCESS);
}

void child_func(int socket)
{
    char hostname[HOST_NAME_MAX]; // hostname de maquina
    char date_out[LEN];           // output de comando date
    char msg[BUF_SIZE];           // mensaje de respuesta

    FILE *cmd_out; // fichero de salida de comando

    if ((cmd_out = popen("date", "r")) == NULL) // ejecucion de comando con popen y redireccion a cmd_out
    {
        perror("popen()");
        exit(EXIT_FAILURE);
    }

    if (fgets(date_out, LEN, cmd_out) == NULL) // lectura de la salida en cmd_out y paso a date_out
    {
        printf("fgets()");
        exit(EXIT_FAILURE);
    }

    if (gethostname(hostname, HOST_NAME_MAX) == -1)
    {
        fprintf(stderr, "gethostname() error\n");
        exit(EXIT_FAILURE);
    }

    sprintf(msg, "%s%s", hostname, date_out); // formateo de respuesta del servidor

    if (send(socket, msg, sizeof(msg), 0) == -1)
    {
        perror("send()");
        exit(EXIT_FAILURE);
    }

    if (recv(sock, msg, BUF_SIZE, 0) == -1)
    {
        perror("recv()");
        exit(EXIT_FAILURE);
    }

    if (shutdown(socket, SHUT_RDWR) == -1)
    {
        perror("shutdown()");
        exit(EXIT_FAILURE);
    }

    if (close(socket) == -1) // cierre de socket
    {
        perror("close()");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
