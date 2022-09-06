 #include <sys/timerfd.h>
 #include <time.h>
 #include <unistd.h>
 #include <inttypes.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include <stdint.h>
 #include <sys/select.h>
 #include <sys/socket.h>
 #include <signal.h>
 #include <netinet/in.h>
 #include <sys/types.h>
 #include <arpa/inet.h>
 
 timer_t varsel_id;
 timer_t quit_id;
 
 // Metode som oppretter ulike alarmer:
 static void timerHandler(int sig, siginfo_t *si, void *uc )
{
    timer_t *tidp;
    tidp = si->si_value.sival_ptr;

    if ( *tidp == varsel_id ) {
        printf("Are you there?\n");
    }
    else if (*tidp == quit_id) {
        printf("Disconnecting\n");
        // legg til disconnect() / avslutt brukeren
        exit(EXIT_SUCCESS);
    }
}

// Metode for aa opprette alarm / timer:
static int makeTimer( char *name, timer_t *timerID, int expireSeconds, int intervalSeconds )
{
    struct sigevent te;
    struct itimerspec its;
    struct sigaction sa;
    int sigNo = SIGRTMIN;

    // Setter alarm egenskaper:
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timerHandler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(sigNo, &sa, NULL) == -1) {
        printf("fail for %s", name);
        return(-1);
    }

    // Setter og aktiverer alarm
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = sigNo;
    te.sigev_value.sival_ptr = timerID;

    timer_create(CLOCK_REALTIME, &te, timerID);
    its.it_interval.tv_sec = intervalSeconds;
    its.it_interval.tv_nsec = 0;

    its.it_value.tv_sec = expireSeconds;
    its.it_value.tv_nsec = 0;
    timer_settime(*timerID, 0, &its, NULL);
    
    return 1;
}

// TEST
int main()
{
        makeTimer("a", &varsel_id, 10, 0); // 10 sek
        makeTimer("b", &quit_id, 30, 0); // 30 sek
        while(1);;

    

    return 0;
}