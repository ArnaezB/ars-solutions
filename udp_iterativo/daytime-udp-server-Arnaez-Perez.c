// Practica tema 5, Arnaez Perez Borja
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#define BUF_SIZE 256
#define LEN 128

int main(int argc, char *argv[])
{
    int port = -1; // puerto
    int opt;       // opciones de programa
    int sock;      // socket

    struct sockaddr_in org_addr;  // socket direccion origen
    struct sockaddr_in dest_addr; // socket direccion destino
    struct servent *serv;         // servicio

    char buffer[BUF_SIZE];        // buffer de recepcion
    char msg[BUF_SIZE];           // mensaje de respuesta
    char date_out[LEN];           // output de comando date
    char hostname[HOST_NAME_MAX]; // hostname de maquina

    FILE *cmd_out; // fichero de salida de comando

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
        serv = getservbyname("daytime", "udp");
        port = serv->s_port;
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) // creacion de socket
    {
        perror("socket(): ");
        exit(EXIT_FAILURE);
    }

    // parametros de socket origen
    org_addr.sin_family = AF_INET;
    org_addr.sin_port = port;
    org_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&org_addr, sizeof(org_addr)) == -1) // bind de socket a origen
    {
        perror("bind(): ");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        unsigned int dest_addr_len = sizeof(dest_addr); // longitud de struct, necesario para el paso de su puntero
        if (recvfrom(sock, buffer, BUF_SIZE, 0, (struct sockaddr *)&dest_addr,
                     &dest_addr_len) == -1)
        {
            perror("recvfrom(): ");
            exit(EXIT_FAILURE);
        }

        if ((cmd_out = popen("date", "r")) == NULL) // ejecucion de comando con popen y redireccion a cmd_out
        {
            perror("popen(): ");
            exit(EXIT_FAILURE);
        }

        if (fgets(date_out, LEN, cmd_out) == NULL) // lectura de la salida en cmd_out y paso a date_out
        {
            printf("fgets(): ");
            exit(EXIT_FAILURE);
        }

        if (gethostname(hostname, HOST_NAME_MAX) == -1)
        {
            fprintf(stderr, "gethostname() error\n");
            exit(EXIT_FAILURE);
        }

        sprintf(msg, "%s: %s", hostname, date_out); // formateo de respuesta del servidor

        if (sendto(sock, msg, sizeof(msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1) // envio de datagrama
        {
            perror("sendto(): ");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}