#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netdb.h>

#include "send_packet.h"

#define BUFSIZE 1400
#define USER_LEN 20
/*
struct Blacklist {
    struct sockaddr_in addr;
    char username[USER_LEN];
    struct Client *next;
};
*/

struct Client {
    struct sockaddr_in addr;
    char username[USER_LEN];
    struct Client *next;
};

struct Client klientListe;

int sock, rc;
char msg[BUFSIZE];
char navn[USER_LEN];
char receive[BUFSIZE + USER_LEN];

// Metode som sjekker feil hvis return value er -1:
void check_error(int res, char *msg) {
    if (res == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

// Metode for aa registrere klient inn i struct lenkelisten:
int registrer(struct sockaddr_in addr, char *navn) {
    struct Client *element = &klientListe;

    //printf("PKT %d REG %s\n", rc, navn);

    int i = 0;
    while (navn[i] != '\0') {
        if (navn[i] < 33 || navn[i] > 126) {
            printf("ACK %d WRONG FORMAT\n", rc);
            exit(EXIT_FAILURE);
        }
        i++;
    }

    while (element != NULL) {
        if (strcmp(element->username, navn) == 0) {
            printf("ACK %d User already exists.\n", rc);
            return -1;
        }
        element = element->next;
    }

    element = &klientListe;
    while (element->next != NULL) {
        element = element->next;
    }
    element->next = malloc(sizeof(struct Client));
    element = element->next;

    element->addr = addr;
    strncpy(element->username, navn, USER_LEN);

    element->next = NULL;
    //printf("ACK %d OK\n", rc);
    return 0;
}

// Metode for aa lete etter bruker:
void oppslag(char *navn) {
    struct Client *element = &klientListe;
    printf("ACK %d LOOKUP %s\n", rc, navn);

    while (element != NULL) {
        if (strcmp(element->username, navn) == 0) {
            printf("ACK %d NICK %s %s PORT %d\n",
                    rc,
                    element->username,
                    inet_ntoa(element->addr.sin_addr),
                    ntohs(element->addr.sin_port));
        }
        else {
            printf("ACK %d NOT FOUND", rc);
        }
        element = element->next;
    }
}

int main(int argc, char const *argv[])
{
    struct sockaddr_in server_addr;
    struct in_addr ip;

    struct hostent *server_host;

    struct timeval timeout;
    fd_set fds, set;

    float loss;
    char *addr_ip;
    int port = atoi(argv[3]);

    if (argc < 6) {
        printf("usage: %s <nick> <adresse> <port> <timeout> <tapssansynlighet>\n", argv[0]);
        return EXIT_FAILURE;
    }

    strncpy(navn, argv[1], USER_LEN);

    loss = atof(argv[5]);
    set_loss_probability(loss);

    strcpy(addr_ip, argv[2]);
    rc = inet_aton(addr_ip, &ip);
    check_error(rc, "inet_aton");
    server_host = gethostbyaddr(&ip, sizeof(ip), AF_INET);
    if (server_host == NULL) {
        perror("error host");
        exit(EXIT_FAILURE);
    }

    // Oppretter socket:
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    check_error(sock, "socket");

    long int sec = atol(argv[4]);
    // Timeout:
    timeout.tv_sec = sec;
    timeout.tv_usec = 0;
    //Oppretter timeout (brukerinnput tid) for pakke:
    rc = setsockopt(sock, 
            SOL_SOCKET, 
            SO_RCVTIMEO, 
            (char*)&timeout,
            sizeof(timeout));
    if (rc < 0) {
        perror("connection timeout");
         exit(EXIT_FAILURE);
    }

    // Server nettverk egenskaper:
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy((char *)&server_addr.sin_addr, 
                    server_host->h_addr,
                    server_host->h_length);


    // Henter brukernavn, registrerer og sender til server:
    strncpy(msg, navn, USER_LEN);
    registrer(server_addr, navn);
    rc = send_packet(sock, 
                msg, 
                strlen(msg), 
                0,
                (struct sockaddr *)&server_addr,
                sizeof(struct sockaddr_in));
    check_error(rc, "send_packet");

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    FD_SET(STDIN_FILENO, &fds);

    
    while (1)
    {
        memcpy(&set, &fds, sizeof(fds));
        
        // Select for timeout:
        rc = select(FD_SETSIZE,
                    &set,
                    NULL,
                    NULL,
                    &timeout);
        check_error(rc, "select");

        if (rc == -1) {
            printf("error input");
            return -1;
        }

        // Receive:
        if (FD_ISSET(sock, &set)) {
            memset(receive, 0, BUFSIZE);
            rc = read(sock,
                    receive,
                    BUFSIZE - 1);
            check_error(rc, "read");
            receive[rc] = '\0';
            printf("%s", receive);
        }

        // Send:
        if (FD_ISSET(STDIN_FILENO, &set)) {
            // buffer zero melding + henter newline terminated string:
            bzero(msg, BUFSIZE);
            fgets(msg, BUFSIZE, stdin);

            // Sender melding til server:
            rc = send_packet(sock,
                    msg,
                    strlen(msg),
                    0,
                    (struct sockaddr *)&server_addr,
                    sizeof(struct sockaddr_in));
            check_error(rc, "send_packet");

            // Hvis brukerinnput har formatet "QUIT",
            // avslutter klienten:
            if (strcmp(msg, "QUIT\n") == 0) {
                return 1;
            }

            /*if (strcmp(msg, "BLOCK") == 0) {}*/
            
        }
    }
    
    close(sock);
    return EXIT_SUCCESS;
    
}
