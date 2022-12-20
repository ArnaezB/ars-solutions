// Practica 8, Arnaez Perez Borja
#include "ip-icmp-ping.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

typedef enum
{
    false,
    true
} bool;

#define PAYLOAD "Testing payload."

/*
    Calcula el checksum de REQUEST y setea su campo Checksum a ese valor.
*/
void checksum(ECHORequest *request);

/*
    Obtiene el mensaje de control del protocolo ICMP según TYPE y CODE.
    Sólo se definen los mensajes correspondientes a TYPE 0 y 3. En el resto de caso de devuelve Unkown Type.
*/
char *control_msg(u_int8_t type, u_int8_t code);

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3) // Comprobación de parametros
    {
        fprintf(stderr, "Usage: %s direccion-ip [-v]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    bool verbose = false;

    if (argc == 3) // Mas compribacion de parametros
    {
        if (!strcmp(argv[2], "-v"))
        {
            verbose = true;
        }
        else
        {
            fprintf(stderr, "Usage: %s direccion-ip [-v]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    int16_t sock;                                 // descriptor de socket
    u_int32_t addr_len = sizeof(struct sockaddr); // tamaño de struc sockaddr

    struct in_addr ip; // ip de maquina remota

    struct sockaddr_in dest_addr; // direccion de destino

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    if (verbose)
    {
        printf("-> Generando cabecera ICMP.\n");
        fflush(stdout);
    }

    ECHORequest ping_request;                       // solicitud de echo ICMP
    memset(&ping_request, 0, sizeof(ping_request)); // inicializacion a 0 de la estructura

    // Asignacion de campos de la estructura
    ping_request.icmpHeader.Type = 8;
    ping_request.ID = getpid();
    strncpy(ping_request.payload, PAYLOAD, sizeof(ping_request.payload));

    checksum(&ping_request);

    if (verbose)
    {
        printf("-> Type: %d.\n"
               "-> Code: %d.\n"
               "-> Identifier (pid): %d.\n"
               "-> Seq. number: %d.\n"
               "-> Cadena a enviar: \"%s\"\n"
               "-> Checksum: 0x%x.\n"
               "-> Tamaño total del paquete ICMP: %d.\n",
               ping_request.icmpHeader.Type,
               ping_request.icmpHeader.Code,
               ping_request.ID,
               ping_request.SeqNumber,
               ping_request.payload,
               ping_request.icmpHeader.Checksum,
               (u_int32_t)sizeof(ping_request));
        fflush(stdout);
    }

    ECHOResponse ping_response; // respuesta echo IP ICMP

    if (inet_aton(argv[1], &ip) == 0) // transformacion ip notacion puntos a network byte order
    {
        perror("inet_aton()");
        exit(EXIT_FAILURE);
    }

    // Asignacion estructura destino
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = 0;
    dest_addr.sin_addr = ip;

    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) // creacion de socket
    {
        if (getuid() != 0)
        {
            fprintf(stderr, "You may need root privileges.\n");
            exit(EXIT_FAILURE);
        }
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval)) == -1)  // Temporizador a
    {
        perror("setsock()");
        fprintf(stderr, "Failed setting timeout to socket. Continuing...\n");
    }

    if (sendto(sock, &ping_request, sizeof(ping_request), 0, (struct sockaddr *)&dest_addr, addr_len) == -1) // envio de datagrama
    {
        perror("sendto()");
        exit(EXIT_FAILURE);
    }

    printf("Paquete ICMP enviado a %s\n", argv[1]);
    fflush(stdout);

    if (recvfrom(sock, &ping_response, sizeof(ping_response), 0, (struct sockaddr *)&dest_addr, &addr_len) == -1) // recepcion de datagrama
    {
        perror("recvfrom()");
        exit(EXIT_FAILURE);
    }

    printf("Respuesta recibida desde %s\n", argv[1]);

    if (verbose)
    {
        printf("-> Tamaño de la respuesta: %d.\n"
               "-> Cadena recibida: \"%s\"\n"
               "-> Identifier (pid): %d.\n"
               "-> TTL: %d.\n",
               (u_int32_t)sizeof(ping_response),
               ping_response.payload,
               ping_response.ID,
               ping_response.ipHeader.TTL);
    }

    printf("Descripción de la respuesta: %s (type %d, code %d).\n",
           control_msg(ping_response.icmpHeader.Type, ping_response.icmpHeader.Code),
           ping_response.icmpHeader.Type,
           ping_response.icmpHeader.Code);

    exit(EXIT_SUCCESS);
}

void checksum(ECHORequest *request)
{
    u_int16_t *ptr = (u_int16_t *)request;       // puntero (16 bits)
    u_int16_t n_bytes = sizeof(ECHORequest);    // bytes de request
    u_int32_t sum = 0;                           // suma

    while (n_bytes > 1) // se suma mientras queden bytes por leer
    {
        sum += *ptr++;
        n_bytes -= 2;
    }

    if (n_bytes == 1) // se suma el byte que quede, podria resultar en Segmentation fault si se hiciese arriba
    {
        sum += *ptr;
    }

    /*
    Se hace shift a la derecha (16 bits mientras sea diferente de 0 el resultado del shift.
    Dos iteraciones serán las máximas necesarias (32 bits -> 16 bits).
    Se suman los 8 bits superiores con los dos inferiores de sum
    */
    while (sum >> 16)
    {
        sum = (sum & 0x0000ffff) + (sum >> 16);
    }

    request->icmpHeader.Checksum = (u_int16_t)~sum; // se asigna not bitwise sum al checksum de la estrucutra
}

char *control_msg(u_int8_t type, u_int8_t code)
{
    switch (type)
    {
    case 0:
        return "Echo reply";

    case 3:
        switch (code)
        {
        case 0:
            return "Destination network unreachable";
        case 1:
            return "Destination host unreachable";
        case 2:
            return "Destination protocol unreachable";
        case 3:
            return "Destination port unreachable";
        case 4:
            return "Fragmentation required, and DF flag set";
        case 5:
            return "Source route failed";
        case 6:
            return "Destination network unknown";
        case 7:
            return "Destination host unknown";
        case 8:
            return "Source host isolated";
        case 9:
            return "Network administratively prohibited";
        case 10:
            return "Host administratively prohibited";
        case 11:
            return "Network unreachable for ToS";
        case 12:
            return "Host unreachable for ToS";
        case 13:
            return "Communication administratively prohibited";
        case 14:
            return "Host Precedence Violation";
        case 15:
            return "Precedence cutoff in effect";
        default:
            return "Destination Unreachable. Unkown Code.";
        }
    default:
        return "Unkown Type";
    }
}