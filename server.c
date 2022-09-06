#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "send_packet.h"

#define BUFSIZE 1400
#define USER_LEN 20

typedef unsigned char unchar;

struct Client {
    struct sockaddr_in addr;
    char username[USER_LEN];
    struct Client *next;
};

struct Client klientListe;

int fd, rc;
char msg_buf[BUFSIZE];
char navn_buf[USER_LEN];

char response_buf[BUFSIZE + USER_LEN];

// Metode som gir feilmelding om de har return value -1:
void check_error(int res, char *msg) {
    if (res == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

// Metode som sjekker om 2 klienter har identiske internet addresser
// (addr, port og family)
// Hvis ja: returnerer 1 (true)
// Hvis nei: returnerer 0 (false)
int sammenlikning(struct sockaddr_in c1, struct sockaddr_in c2) {
    if (strncmp((char *)&c1.sin_addr.s_addr,
                (char *)&c2.sin_addr.s_addr,
                sizeof(unsigned long)) == 0) {
                    if (strncmp((char *)&c1.sin_port,
                                (char *)&c2.sin_port,
                                sizeof(unsigned short)) == 0) {
                                    if (strncmp((char *)&c1.sin_family,
                                                (char *)&c2.sin_family,
                                                sizeof(unsigned short)) == 0) {
                                                    return 1; //true
                                                }
                                }
                }
                return 0; //false
}

// Metode for aa sjekke om en klient er koblet til:
int erConnected(struct sockaddr_in ny) {
    struct Client *element = &klientListe;

    while (element != NULL)
    {
        // Hvis koblet til:
        if(sammenlikning(element->addr, ny)) {
            strncpy(navn_buf, element->username, USER_LEN);
            //printf("Client already connected.\n");
            return 1;
        }
        element = element->next;
    }
    
    //printf("Client not connected.\n");
    return 0;
}

// Metode for aa registrere en klient i serverens lenkeliste (struct):
int registrer(struct sockaddr_in addr, char *navn) {
    struct Client *element = &klientListe;

    printf("PKT %d REG %s\n", rc, navn);

    int i = 0;
    // Sjekker om brukernavnet inneholder gyldige ASCII tegn:
    while (navn[i] != '\0') {
        // Hvis ikke: feilmelding
        if (navn[i] < 33 || navn[i] > 126) {
            printf("ACK %d WRONG FORMAT\n", rc);
        }
        i++;
    }

    while (element != NULL) {
        if (strcmp(element->username, navn) == 0) {
            printf("ACK %d User already exists.\n", rc);
            strcpy(response_buf, "");
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
    printf("ACK %d OK\n", rc);
    return 0;
}

// Metode som soeker gjennom om en bruker finnes via
// bruker-input gitt av en klient:
void oppslag(char *navn) {
    struct Client *element = &klientListe;
    printf("ACK %d LOOKUP %s\n", rc, navn);

    // Hvis klientlista ikke er tom:
    while (element != NULL) {
        // Comparer argumentet med brukernavn fra lista:
        if (strcmp(element->username, navn) == 0) {
            // Hvis funnet, printer ut:
            printf("ACK %d NICK %s %s PORT %d\n",
                    rc,
                    element->username,
                    inet_ntoa(element->addr.sin_addr),
                    ntohs(element->addr.sin_port));
        }
        // Hvis ikke funnet:
        else {
            printf("ACK %d NOT FOUND\n", rc);
        }
        element = element->next;
    }
}

// Metode for aa fjerne en bruker fra lista som
// ikke lenger er i chatten:
int disconnect(struct sockaddr_in gammel) {
    //printf("Disconnecter klient.\n");
    struct Client *temp;
    struct Client *element = &klientListe;

    // Fjerner klienten dynamisk fra serverens lenkeliste (struct):
    while (element->next != NULL) {
        if (sammenlikning(gammel, element->next->addr)) {
            temp = element->next->next;
            free(element->next);
            element->next = temp;
            printf("Client disconnected.\n");
            return 0;
        }
        element = element->next;
    }
    printf("Error disconnecting\n");
    return -1;
}

// Metode for aa sende melding globalt (sende til alle klienter):
void broadcast(struct sockaddr_in s, int x) {
    struct Client *cl = klientListe.next;

    // Hvis klientlista ikke er tom:
    while (cl != NULL) {
        // Hvis klienten / brukernavnet ikke er den samme:
        if (sammenlikning(s, cl->addr) == 0 || x) {
            // Sender ut melding fra serveren:
            printf("TO %s\n", cl->username);
            
            // Sender melding til klienten:
            rc = send_packet(fd, 
                        response_buf, 
                        strlen(response_buf),
                        0,
                        (struct sockaddr *)&cl->addr,
                        sizeof(struct sockaddr));
            check_error(rc, "send_packet");
        }
        cl = cl->next;
    }
}

// Metode som printer ut liste av alle klienter som
// er registrert i serveren:
void printKlienter() {
    struct Client *cl = klientListe.next;
    int teller = 1;

    while(cl != NULL) {
        printf("Client %d\n", teller);
        printf("Bruker: %s\n", cl->username);
        printf("ip: %s\n", inet_ntoa(cl->addr.sin_addr));
        printf("port: %d\n", ntohs(cl->addr.sin_port));
        cl = cl->next;
        teller++;
    }
}


// Metode som erstatter mellomrom med null-termination:
// (fjerner alt som kommer etter en nickname)
void fjern(char *navn) {
    int i = 0;
    while (navn[i] != '\0') {
        if (navn[i] == ' ') {
            navn[i] = '\0';
        }
        i++;
    }
}

// Metode for aa motta og sende mld til klient(er):
void recv_packet(int fd, struct sockaddr_in client) {
    bzero(response_buf, BUFSIZE + USER_LEN);
    int len = sizeof(client);
    
    //Henter melding fra klient:
    rc = recvfrom(fd, 
                    msg_buf, 
                    BUFSIZE - 1, 
                    0, 
                    (struct sockaddr*)&client, 
                    &len);
        check_error(rc, "recvfrom");
        //Sikrer at meldingen er null terminated (unngaa spesielle tegn):
        msg_buf[rc] = '\0';
        //printf("msg buf: %s\n", msg_buf);
        //printf("Packet: %d", rc);

        //Hvis en klient er koblet til:
        if (erConnected(client)) {
            //Konkatenerer brukernavn med melding:
            strcat(response_buf, navn_buf);
            //Hvis brukeren skriver QUIT
            if (strcmp("QUIT\n", msg_buf) == 0) {
                //Brukeren blir disconnected:
                if (disconnect(client) == 0) {
                    //Konkatenerer brukernavn med system melding:
                    strcat(response_buf, " disconnected\n");
                    // Globalt (alle klienter faar meldingen):
                    broadcast(client, 1);
                }
                //printKlienter();
            }
            //Hvis brukeren skriver @ for aa lete etter bruker:
            else if (strchr(msg_buf, '@')) {
                // Char peker blir opprettet som fjerner foerste
                //character i meldingen:
                char *ny = msg_buf+1;
                char ping[USER_LEN];

                //Kopierer char pekern inni ping (char array)
                strcpy(ping, ny);

                //Fjerner melding (kun brukernavn skal vaere igjen)
                fjern(ping);
                //Kaller paa oppslag:
                oppslag(ping);
                // Sender blir ikke vist:
                broadcast(client, 0);
            }
            else if (strcmp("BLOCK", msg_buf) == 0) {
                //broadcast: 0
            }
            // Hvordan responsen skal see ut:
            // Konkatenering av bruker og deres respons:
            else {
                strcat(response_buf, ": ");
                strcat(response_buf, msg_buf);
                printf("PKT %d FROM %s", rc, response_buf);
                broadcast(client, 0);
            }
        }
        // Hvis brukeren ikke er registrert i chatsystemet:
        else {
            // Registrerer brukeren
            if (registrer(client, msg_buf) == 0) {
                // Konkatenerer melding:
                strcat(response_buf, msg_buf);
                strcat(response_buf, " connected\n");
                broadcast(client, 1);
            }
            //printKlienter();
        }
}

int main(int argc, char const *argv[])
{
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;
    fd_set fds, set;

    float loss;

    if (argc < 3) {
        printf("usage: %s <port> <tapssansynlighet>\n", argv[0]);
        return EXIT_FAILURE;
    }

    loss = atof(argv[2]);
    set_loss_probability(loss);

    // Minne fra både server og client satt konstant byte til 0
    memset((char *)&server_addr, 0, sizeof(server_addr));
    memset((char *)&client_addr, 0, sizeof(client_addr));

    // Oppretter socket:
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    check_error(fd, "socket");

    // Server egenskaper:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Assigner local socket addresse:
    rc = bind(fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
    check_error(rc, "bind");

    //Initialiserer til å ha 0 bits:
    FD_ZERO(&fds);
    //Setter bit for socket fd:
    FD_SET(fd, &fds);
    //Setter bit for standard input file (som er 0):
    FD_SET(STDIN_FILENO, &fds);

    printf("**SERVER LOG**\n\n");

    while (1) {
        //bzero(response_buf, BUFSIZE + USER_LEN);
        //Kopierer fds inni set:
        memcpy(&set, &fds, sizeof(fds));

        //Select for tillate timeout:
        rc = select(FD_SETSIZE,
                    &set,
                    NULL,
                    NULL,
                    NULL);
        check_error(rc, "select");

        // Receive:
        if (FD_ISSET(fd, &set)) {
            recv_packet(fd, client_addr);
        }

        /*
        // Send:
        if (FD_ISSET(STDIN_FILENO, &set)) {
            memset(msg_buf, 0, BUFSIZE);
            rc = sendto(fd,
                    msg_buf,
                    BUFSIZE,
                    0,
                    (struct sockaddr *)&server_addr,
                    sizeof(struct sockaddr_in));
            check_error(rc, "sendto");

            if (strcmp(msg_buf, "EXIT") == 0) {
                return EXIT_SUCCESS;
            }
        }
        */
    }

    close(fd);
    return EXIT_SUCCESS;
}
