#include <ql_oe.h>
#include "ql_qcmap_client_api.h"
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#define CMD_V3 0x3
#define P2P_CMD_MAGIC 0x2a87cf3a

typedef struct
{
    int magic;
    int datalen;
    int protocol;
    int transid;
    int checksum;
}p2p_head_t;

static int wakelock_timer;
static int wakelock_sock;
static int sockfd = 0;
static timer_t timerid;

uint64_t get_current_time_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}


static inline void power_save(int fd)
{
    if(Ql_SLP_WakeLock_Unlock(fd) != 0)
        printf("Unlock wakelock failed \n");
}

static inline void wakeup(int fd)
{
    if(Ql_SLP_WakeLock_Lock(fd) !=0)
        printf("lock wakelock failed \n");
}

/*********************** signal handler *************************/
static void ql_sighandler(int signal)
{
    switch(signal)
    {
    case SIGTERM:
        if(sockfd > 0)
            close(sockfd);
        exit(1);
        break;
    case SIGINT:
        wakeup(wakelock_sock);
        exit(0);
        break;

    default:
        printf("Receiver unexpected signal %d\n",signal);
        break;
    }
}

static void start_timer(int ms, timer_t timerid);
static void timer_thread(union sigval v)
{
    unsigned char    buf[1024];
    int ret, len;

    wakeup(wakelock_timer);

    len = snprintf((char *)buf, sizeof(buf), "TEST0000068C0KTH+keepalive");
    ret = send(sockfd, buf, len, MSG_NOSIGNAL);
    if (ret < 0)
    {
        printf("Message '%s' failed to send ! "
                "The error code is %d, error message '%s'\n",
                buf, errno, strerror(errno));
    }
    printf("timeout got. send msg to server\n");

    start_timer(9*60 * 1000, timerid);

    power_save(wakelock_timer);
}

static void start_timer(int ms, timer_t timerid)
{
    struct itimerspec it;

    it.it_interval.tv_sec = ms / 1000;
    it.it_interval.tv_nsec = (ms % 1000) * 1000;
    it.it_value.tv_sec = ms / 1000;
    it.it_value.tv_nsec = (ms % 1000) * 1000;

    if (timer_settime(timerid, 0, &it, NULL) == -1)
    {
        perror("fail to timer_settime");
        exit(-1);
    }
}

/****** after dial apn, we connect to server, and then module enter sleep mode,
 ****** waiting server send msg to wakeup module ******/
static void client_rev_process(void *params)
{
	struct  sigevent evp;
    struct  pollfd   entry;
    int     len      = 0;
    unsigned char    buf[1024];
	int     ret, hb = 0, wp = 0;

    len = snprintf((char *)buf, sizeof(buf), "TEST0000068C0KTH+keepalive");
    ret = send(sockfd, buf, len, MSG_NOSIGNAL);
    if (ret != len)
    {
        perror("send msg to server error!\n");
        return;
    }
	
	//1. Create 1st timer
    memset(&evp, 0, sizeof(struct sigevent));

    evp.sigev_value.sival_int = 111;	// The timer id that developer specify. it can be passed into callback
    evp.sigev_notify = SIGEV_THREAD;	// The thread notifying way -- dispatch a new thread.
    evp.sigev_notify_function = timer_thread;	// The thread address

    ret = 	timer_create(CLOCK_BOOTTIME_ALARM, &evp, &timerid);
    printf("< timer_create(%d,..)=%d, timer id=%u >\n", CLOCK_BOOTTIME_ALARM, ret, *((unsigned int*)(&timerid)));
    if (ret < 0)
    {
        perror("fail to timer_create");
        exit(-1);
    }

    start_timer(20 * 1000, timerid);

    while(1)
    {
        entry.fd = sockfd;
        entry.events = POLLIN;

        power_save(wakelock_sock);
        ret = poll(&entry, 1, 10 * 1000);
        wakeup(wakelock_sock);

        if(ret <= 0) 
        { 
            continue;
        }

        if(entry.revents & POLLIN)
        {
            ret = recv(sockfd, buf, sizeof(buf) - 1, 0);

            if(ret > 0)
            {
                buf[ret] = '\0';

                if (strstr((const char *)buf, "R2D_HB_R") != NULL)
                    hb++;
                else if (strstr((const char *)buf, "R2D_WKUP") != NULL)
                    wp++;

                printf("###Msg from server: %s, hb times: %d, wakeup times: %d\n", buf, hb, wp);
            } 
            else if((ret < 0 && errno != EINTR) || ret == 0) 
            {
                printf("socket closed!!!\n");
                close(sockfd);
                exit(0);
            }
        } 
        else if(entry.revents & (POLLERR | POLLHUP))
        {
            printf("socket closed!!!\n");
            close(sockfd);
            exit(0);
        }
    }
}

static int connect_server(int sockfd, struct sockaddr *addr, int len)
{
    struct pollfd entry;
    int ret;
    socklen_t optlen;

    while((ret = connect(sockfd, addr, len)))
    {
        switch(errno)
        {
            case EINTR:
                continue;
            case EAGAIN:
            case EINPROGRESS:
                {
                    uint64_t start_time = get_current_time_ms();
                    while(get_current_time_ms() < start_time + 5000ull)
                    {
                        entry.fd = sockfd;
                        entry.events = POLLOUT;

                        ret = poll(&entry, 1, 1000);
                        if(ret < 0) 
                        {
                            if(errno == EINTR)
                                continue;
                            return ret;
                        }
                        else if(ret > 0)
                        {
                            /* socket connect request finished */
                            optlen = sizeof(ret);
                            getsockopt (sockfd, SOL_SOCKET, SO_ERROR, &ret, &optlen);
                            if(ret == 0)
                                return 0;
                            else
                                return -1;
                        }
                    } 

                    break;
                }
            default:
                return ret;
        }
    }

    return ret;
}

static int tcp_client(int argc, char **argv)
{
    char *default_server[] = {
        NULL,
        "113.116.60.219",
        "15432"
    };
    struct addrinfo hints, *curai, *ai;
    int ret;

    if(argc != 3 )
    {
        printf("use default server %s:%s\n", default_server[1], default_server[2]);
        argv = default_server;
    }
 
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((ret = getaddrinfo(argv[1], argv[2], &hints, &ai)) < 0)
    {
        printf("ip %s is invalid, error:%s\n", argv[1], gai_strerror(ret));
        return -1;
    }

    for(curai = ai; curai != NULL; curai = curai->ai_next)
    {
        if ((sockfd = socket(curai->ai_family, curai->ai_socktype, curai->ai_protocol)) < 0)
        {
            printf("Socket error\n");
            continue;
        }    

        fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

        if((ret = connect_server(sockfd, curai->ai_addr, curai->ai_addrlen)) == 0)
        {
            /* connect success */
            printf("######## Connect server %s %s OK #########\n", argv[1], argv[2]);
            break;
        }

        close(sockfd);
        sockfd = -1;
    }

    if(sockfd < 0)
        ret = -1;

    freeaddrinfo(ai);
    return ret;
}

/*************************** Main ***********************************/
int main(int argc, char **argv)
{
    signal(SIGTERM, ql_sighandler);
    signal(SIGINT, ql_sighandler);
    signal(SIGALRM, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    wakelock_timer = Ql_SLP_WakeLock_Create("timer", sizeof("timer"));
    wakelock_sock = Ql_SLP_WakeLock_Create("sock", sizeof("sock"));
    /* user also could integrate sms, voicecall, data */
    if(tcp_client(argc, argv) < 0)
        return 0; 
    
    wakeup(wakelock_sock);
    Ql_Autosleep_Enable(1);

    while(1) {
        client_rev_process(NULL);
    }

    return 0;
}
