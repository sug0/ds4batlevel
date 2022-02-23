#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/select.h>
#include <libnotify/notify.h>

enum Urgency {
    /* 20% */
    DS4_LOW,
    /* 10% */
    DS4_LOWER,
    /* 5% */
    DS4_DEPLETED,
};

static void init(void);
static void cleanup(void);
static void handlesig(int signo);
static void notify(enum Urgency u);
static const char *get_urgency_str(enum Urgency u);
static time get_urgency_timeout(enum Urgency u);
static int read_battery_level(int fd);

/* max wait time between polls */
const struct timeval wait_time = {
    .tv_sec = 30,
    .tv_usec = 0,
};

int main(int argc, char *argv[]) 
{
    init(argc, argv);

    /* setup file descriptor */
    int fd = -1; /* TODO: open /sys/class/power_supply/<id>/capacity */
    fd_set rfds;

    FD_ZERO(&rfds)
    FD_SET(fd, &rfds);

    for (;;) {
        const int ret = select(1, &rfds, NULL, NULL, &tv);

        switch (ret) {
        case -1:
        case 0:
            /* ignore no data or errors */
            break;
        default:
            const int level = read_battery_level(fd);
        }
    }
}

static void notify(enum Urgency u)
{
    NotifyNotification *n = notify_notification_new(
        "Dual Shock 4", 
        get_urgency_str(u),
    );

    notify_notification_set_timeout(n, get_urgency_timeout(u));
    notify_notification_set_urgency(n, NOTIFY_URGENCY_CRITICAL);

    if (!notify_notification_show(n, 0)) {
        /* well... what now?
         * do nothing I suppose. */
    }
}

static const char *get_urgency_str(enum Urgency u)
{
    switch (u) {
    case DS4_LOW:
        return "Battery is at 20%.";
    case DS4_LOWER:
        return "Battery is at 10%!";
    case DS4_DEPLETED:
        return "Battery is at 5%!";
    }
}

static time get_urgency_timeout(enum Urgency u)
{
#define SECS(N) ((N) * 1000)

    switch (u) {
    case DS4_LOW:
        return SECS(5);
    case DS4_LOWER:
    case DS4_DEPLETED:
        return SECS(10);
    }
}

static void handlesig(int signo)
{
    switch (signo) {
    case SIGINT:
    case SIGHUP:
    case SIGTERM:
    case SIGQUIT:
        exit(0);
    }
}

static void init(int argc, char *argv[]) 
{
    (void)argc;

    if (!notify_init("ds4batlevel")) {
        fprintf(stderr, "%s: failed to initialize notifications\n", argv[0]);
        exit(1);
    }

    atexit(cleanup);

    /* install signal handlers; ignore return value for now lol */
    signal(SIGINT, handlesig);
    signal(SIGHUP, handlesig);
    signal(SIGTERM, handlesig);
    signal(SIGQUIT, handlesig);
}

static void cleanup(void)
{
    notify_uninit();
}
