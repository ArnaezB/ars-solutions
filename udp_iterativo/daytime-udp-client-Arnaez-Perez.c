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

#define BUF_SIZE 256

int main(int argc, char *argv[])
{
    if (argc < 2) // comprobacion de parametros
    {
        fprintf(stdout, "Usage: %s SERVER.ADDRESS [-p port].\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int opt;       // opciones de programa
    int port = -1; // puerto de aplicacion
    int sock;      // socket
    int n_recv;    // bytes recibidos

    unsigned int addr_len; // tamaño de struct

    struct in_addr ip;            // ip destino
    struct servent *serv;         // servicio
    struct sockaddr_in dest_addr; // socket direccion destino
    struct sockaddr_in org_addr;  // socket direccion origen

    char buffer[BUF_SIZE]; // buffer de recepcion

    if (inet_aton(argv[1], &ip) ==
        0) // transformacion ip notacion puntos a network byte order
    {
        perror("inet_aton(): ");
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "p:")) != -1) // extraccion de opciones del programa
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

    if (port == -1) // obetencion del puerto del servicio
    {
        serv = getservbyname("daytime", "udp");
        port = serv->s_port;
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) // creacion de socket
    {
        perror("socket(): ");
        exit(EXIT_FAILURE);
    }

    // parametros socket origen
    org_addr.sin_family = AF_INET;
    org_addr.sin_port = 0;
    org_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&org_addr, sizeof(org_addr)) == -1) // bind de socket a origen
    {
        perror("bind(): ");
        exit(EXIT_FAILURE);
    }

    addr_len = sizeof(dest_addr);

    // parametros socket destino
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = port;
    dest_addr.sin_addr = ip;

    if (sendto(sock, buffer, BUF_SIZE, 0, (struct sockaddr *)&dest_addr, addr_len) == -1) // envio de datagrama
    {
        perror("sendto(): ");
        exit(EXIT_FAILURE);
    }

    if ((n_recv = recvfrom(sock, buffer, BUF_SIZE, 0, (struct sockaddr *)&dest_addr, &addr_len)) == -1) // recepcion de datagrama
    {
        perror("recvfrom(): ");
        exit(EXIT_FAILURE);
    }

    if (close(sock) == -1) // cierre de socket
    {
        perror("close(): ");
        exit(EXIT_FAILURE);
    }

    printf("%s", buffer); // impresion de mensaje recibido
    return 0;
}