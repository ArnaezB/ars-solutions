// Practica tema 7, Arnaez Perez Borja
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <limits.h>

#define MAX_FILENAME_LEN 100    // Tamaño maximo del fichero
#define MAX_ERRSTRING_LEN 43    // Tamaño maximo errstring
#define MAX_BYTES_DATA 512      // Maximo de bytes recividos por paquete
#define MAX_BYTES_DATA_PKG 516  // Tamaño maximo de un paquete data (cabecera + data)

#define OPCODE_SIZE 2           // Bytes del campo de operacion
#define BLOCK_SIZE 2            // Bytes del campo numero bloque
#define LEN_OCTET strlen("octet")
#define DEF_MODE "octet"        // Modo por defecto
#define ACK_SIZE (OPCODE_SIZE + BLOCK_SIZE)     // Tamaño de un ack
#define MAX_RESPONSE_WRITE_SIZE (MAX_ERRSTRING_LEN + 1 + OPCODE_SIZE + BLOCK_SIZE)      // Tamaño maximo de una respuesta cualquiera del servidor

// Codigos de peticiones
#define OPC_READ 1
#define OPC_WRITE 2
#define OPC_DATA 3
#define OPC_ACK 4
#define OPC_ERROR 5

// Mensajes de error
#define ERR_MSG_1 "Fichero no encontrado."
#define ERR_MSG_2 "Violación de acceso."
#define ERR_MSG_3 "Espacio de almacenamiento lleno."
#define ERR_MSG_4 "Operación TFTP ilegal."
#define ERR_MSG_5 "Identificador de transferencia desconocido."
#define ERR_MSG_6 "El fichero ya existe."
#define ERR_MSG_7 "Usuario desconocido."

// Prepara req (request) para la lectura de file en modo mode
void set_req(char *req, char *file, int mode);
// Implementacion de lectura tftp
void read_tftp(int socket, int verbose_level, struct sockaddr_in address, char *filename);
// Implementacion de escritura tftp
void write_tftp(int socket, int verbose, struct sockaddr_in address, char *filename);
// Impresion de mensaje de error en el paquete pkg
void print_error(u_char *pkg);

int main(int argc, char *argv[])
{
    int opt;
    int port;
    int mode = -1;      // 0 read 1 write
    int verbose = 0;
    int sock;

    char filename[MAX_FILENAME_LEN + 1];

    struct in_addr ip;
    struct sockaddr_in dest_addr; // socket direccion destino
    struct sockaddr_in org_addr;  // socket direccion origen

    
    ICMPHeader icmp_header;    
    ECHORequest req;
    ECHOResponse resp;

    if (argc < 4 || argc > 5)   // Comprobacion argumentos
    {
        fprintf(stderr, "Bad usage.\nUsage: %s SERVER.ADDRESS {-r|-w} filename [-v]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (inet_aton(argv[1], &ip) == 0) // transformacion ip notacion puntos a network byte order
    {
        perror("inet_aton(): ");
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "w:r:v")) != -1) // extraccion de opciones del programa
    {
        switch (opt)
        {
        case 'w':
        case 'r':
            if (mode != -1 || !strcmp(optarg, "-w") || !strcmp(optarg, "-r")) // se pasaron -w y -r como argumentos (ambos)
            {
                fprintf(stderr, "Both w and r found as arguments. Only one is valid.\nUsage: %s SERVER.ADDRESS {-r|-w} filename [-v]\n", argv[0]);
                exit(EXIT_FAILURE);
            }
            mode = (opt == 'w');
            if (strlen(optarg) > MAX_FILENAME_LEN)  // longitud del nombre del fichero
            {
                fprintf(stderr, "Filename length must be less or equal to 100 characters\n");
                exit(EXIT_FAILURE);
            }
            strncpy((char *)filename, optarg, MAX_FILENAME_LEN);
            if (strlen(optarg) == MAX_FILENAME_LEN) // \0 al final del nombre si este es justo de la longitud maxima (strncpy no lo hace en este caso)
            {
                filename[sizeof(filename) / sizeof(filename[0]) - 1] = 0;
            }
            break;
        case 'v':
            verbose = 1;
            break;
        default:
            break;
        }
    }

    if (mode == -1)     // no se determino lectura o escritura
    {
        fprintf(stderr, "No -w or -r given.\nUsage: %s SERVER.ADDRESS {-r|-w} filename [-v]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port = getservbyname("tftp", "udp")->s_port;    // obtencion del puerto del servicio tftp

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) // creacion de socket
    {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    // parametros socket origen
    org_addr.sin_family = AF_INET;
    org_addr.sin_port = 0;
    org_addr.sin_addr.s_addr = INADDR_ANY;

    // parametros socket destino
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = port;
    dest_addr.sin_addr = ip;

    if (bind(sock, (struct sockaddr *)&org_addr, sizeof(org_addr)) == -1) // bind de socket a origen
    {
        perror("bind()");
        exit(EXIT_FAILURE);
    }

    if (mode)
    {
        write_tftp(sock, verbose, dest_addr, filename);
    }
    else
    {
        read_tftp(sock, verbose, dest_addr, filename);
    }

    close(sock);

    return 0;
}

void read_tftp(int socket, int verbose, struct sockaddr_in address, char *file)
{
    u_int req_size;     // tamaño peticion
    u_int addr_size;    // tamaño de address
    u_int n_data_recv = MAX_BYTES_DATA;     // bytes de datos recividos en la anterior llamada
    u_int n_recv;       // bytes totales recibidos en la anterior llamada
    u_int block_recv;   // ultimo bloque recibido
    u_int block_at = 1; // bloque actual

    FILE *file_ptr;     // fichero de escritura local

    char *readable_ip;  // ip en formato legible
    char *req;          // peticion tftp
    char *ack;

    u_char buffer[MAX_BYTES_DATA_PKG];      // buffer de recepcion

    readable_ip = inet_ntoa(address.sin_addr);

    req_size = (OPCODE_SIZE + strlen(file) + LEN_OCTET + 2);
    addr_size = sizeof(address);

    req = (char *)malloc(req_size);
    ack = (char *)malloc(ACK_SIZE);

    if ((file_ptr = fopen(file, "w")) == NULL)   // apertura de archivo de copiado local
    {
        perror("fopen()");
        exit(EXIT_FAILURE);
    }

    set_req(req, file, OPC_READ);   // request de lectura

    if (sendto(socket, req, req_size, 0, (struct sockaddr *)&address, addr_size) == -1) // envio de datagrama
    {
        perror("sendto()");
        exit(EXIT_FAILURE);
    }

    if (verbose)
    {
        printf("Enviada solicitud de lectura de %s a servidor tftp en %s\n", file, readable_ip);
        fflush(stdout);
    }

    free(req);

    while (n_data_recv == MAX_BYTES_DATA)   // mientras no hayamos recibido todos los datos
    {

        if ((n_recv = recvfrom(socket, buffer, MAX_BYTES_DATA_PKG, 0, (struct sockaddr *)&address, (__socklen_t *)&addr_size)) == -1) // recepcion de datagrama
        {
            perror("recvfrom()");
            exit(EXIT_FAILURE);
        }

        n_data_recv = n_recv - OPCODE_SIZE - BLOCK_SIZE;

        if (verbose)
        {
            printf("Recibido paquete %d desde servidor tftp\n", buffer[3]);
            fflush(stdout);
        }

        if (buffer[1] == OPC_ERROR)     // servidor devolvio error
        {
            print_error(buffer);
            exit(EXIT_FAILURE);
        }

        if (buffer[1] == OPC_DATA)  // servidor devolvio datos
        {
            block_recv = (buffer[2] << 8) | buffer[3];  // dos chars a un int
            if (block_at == block_recv)     // orden correcto
            {
                if (verbose)
                {
                    printf("Recibido bloque %d\n", block_at);
                    fflush(stdout);
                }

                // escritura de datos obtenidos
                fwrite(&buffer[OPCODE_SIZE + BLOCK_SIZE], sizeof(char), n_data_recv, file_ptr);

                // construccion ack
                ack[0] = 0;
                ack[1] = OPC_ACK;
                // un int a dos chars
                ack[2] = block_recv >> 8;
                ack[3] = block_recv & 0x00ff;

                if (sendto(socket, ack, ACK_SIZE, 0, (struct sockaddr *)&address, addr_size) == -1) // envio de datagrama
                {
                    perror("sendto()");
                    exit(EXIT_FAILURE);
                }

                if (verbose)
                {
                    printf("Enviado ACK de bloque %d\n", block_at);
                    fflush(stdout);
                }

                block_at++;
            }
        }
    }

    free(ack);

    if (verbose)
    {
        printf("El bloque %d era el final. Fichero cerrado.\n", block_at - 1);
        fflush(stdout);
    }
    fclose(file_ptr);
}

void write_tftp(int socket, int verbose, struct sockaddr_in address, char *file)
{
    u_int req_size;     // tamaño de peticion
    u_int addr_size;    // tamaño de address
    u_int block_at = 0; // bloque actual
    u_int n_read = MAX_BYTES_DATA;      // datos leidos en la ultima lectura del fichero
    u_int ack_recv;     // ultimo ack recivido

    FILE *file_ptr;     // fichero de lectura local

    char *readable_ip;  // ip en forma legible
    char *req;          // peticion

    u_char ack[MAX_RESPONSE_WRITE_SIZE];
    u_char buffer[MAX_BYTES_DATA];      // buffer de envio

    readable_ip = inet_ntoa(address.sin_addr);

    req_size = (OPCODE_SIZE + strlen(file) + LEN_OCTET + 2);
    addr_size = sizeof(address);
    req = (char *)malloc(req_size);

    if ((file_ptr = fopen(file, "r")) == NULL)      // apertura fichero en lectura
    {
        perror("fopen()");
        exit(EXIT_FAILURE);
    }

    set_req(req, file, OPC_WRITE);

    if (sendto(socket, req, req_size, 0, (struct sockaddr *)&address, addr_size) == -1) // envio de datagrama
    {
        perror("sendto()");
        exit(EXIT_FAILURE);
    }

    free(req);

    if (verbose)
    {
        printf("Enviada solicitud de escritura de %s a servidor tftp en %s\n", file, readable_ip);
        fflush(stdout);
    }

    while (n_read == MAX_BYTES_DATA)    // mientras no hayamos leido todos
    {
        if (recvfrom(socket, ack, MAX_RESPONSE_WRITE_SIZE, 0, (struct sockaddr *)&address, (__socklen_t *)&addr_size) == -1) // recepcion de datagrama
        {
            perror("recvfrom()");
            exit(EXIT_FAILURE);
        }

        if (verbose)
        {
            printf("Recibida respuesta del servidor\n");
            fflush(stdout);
        }

        if (ack[1] == OPC_ERROR)    // servidor devolvio error
        {
            print_error(ack);
            exit(EXIT_FAILURE);
        }

        if (ack[1] == OPC_ACK)      // servidor devolvio ack
        {
            ack_recv = (ack[2] << 8) | ack[3];      // dos chars a un int

            if (verbose)
            {
                printf("Recibido ack del bloque %d.\n", ack_recv);
                fflush(stdout);
            }

            if (block_at == ack_recv)      // orden correcto
            {
                n_read = fread(buffer, sizeof(char), MAX_BYTES_DATA, file_ptr);

                char data[OPCODE_SIZE + BLOCK_SIZE + n_read];

                data[0] = 0;
                data[1] = OPC_DATA;
                data[2] = (++block_at >> 8);
                data[3] = block_at & 0x00ff;
                memcpy(&data[4], buffer, n_read);

                if (sendto(socket, data, sizeof(data), 0, (struct sockaddr *)&address, addr_size) == -1) // envio de datagrama
                {
                    perror("sendto()");
                    exit(EXIT_FAILURE);
                }

                if (verbose)
                {
                    printf("Enviado paquete de bloque %d.\n", block_at);
                    fflush(stdout);
                }
            }
        }
    }
    fclose(file_ptr);
    if (verbose)
    {
        printf("El bloque %d era el ultimo. Fichero cerrado.\n", block_at);
        fflush(stdout);
    }
}

void set_req(char *req, char *file, int mode)
{
    req[0] = 0;
    req[1] = mode;
    strncpy(&req[OPCODE_SIZE], file, strlen(file) + 1);
    strncpy(&req[OPCODE_SIZE + strlen(file) + 1], DEF_MODE, LEN_OCTET + 1);
}

void print_error(u_char *pkg)
{
    switch (pkg[3])
    {
    case 0:
        fprintf(stderr, "Error: %s\n", &pkg[4]);
        break;
    case 1:
        fprintf(stderr, "Error: %s\n", ERR_MSG_1);
        break;
    case 2:
        fprintf(stderr, "Error: %s\n", ERR_MSG_2);
        break;
    case 3:
        fprintf(stderr, "Error: %s\n", ERR_MSG_3);
        break;
    case 4:
        fprintf(stderr, "Error: %s\n", ERR_MSG_4);
        break;
    case 5:
        fprintf(stderr, "Error: %s\n", ERR_MSG_5);
        break;
    case 6:
        fprintf(stderr, "Error: %s\n", ERR_MSG_6);
        break;
    case 7:
        fprintf(stderr, "Error: %s\n", ERR_MSG_7);
        break;
    }
}