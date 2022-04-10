#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/select.h>
#include <libnotify/notify.h>

#include "config.h"

enum Urgency {
    /* >15% */
    DS4_HEALTHY,
    /* 15% */
    DS4_LOW,
    /* 10% */
    DS4_LOWER,
    /* 5% */
    DS4_DEPLETED,
};

static char *argv0;
static int ds4_fd = -1;

static void init(int argc, char *argv[]);
static void cleanup(void);
static void handlesig(int signo);
static void notify(enum Urgency u);
static const char *get_urgency_str(enum Urgency u);
static int get_urgency_timeout(enum Urgency u);
static int read_battery_level(void);
static enum Urgency get_battery_urgency(void);

/* max wait time between polls */
static struct timeval wait_time = {
    .tv_sec = 60,
    .tv_usec = 0,
};

int main(int argc, char *argv[]) 
{
    init(argc, argv);

    /* setup file descriptor */
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(ds4_fd, &rfds);

    for (;;) {
        const int ret = select(1, &rfds, NULL, NULL, &wait_time);

        switch (ret) {
        case -1:
        case 0:
            /* ignore no data or errors */
            break;
        }

        const enum Urgency u = get_battery_urgency();

        if (u != DS4_HEALTHY) {
            notify(u);
        }
    }
}

static void notify(enum Urgency u)
{
    NotifyNotification *n = notify_notification_new(
        "Dual Shock 4", 
        get_urgency_str(u),
        "input-gamepad"
    );

    notify_notification_set_timeout(n, get_urgency_timeout(u));
    notify_notification_set_urgency(n, NOTIFY_URGENCY_CRITICAL);

    if (!notify_notification_show(n, 0)) {
        /* well... what now?
         * abort, I suppose. */
        fprintf(stderr, "%s: failed to show notification to the user\n", argv0);
        exit(1);
    }
}

static const char *get_urgency_str(enum Urgency u)
{
    switch (u) {
    case DS4_LOW:
        return "Battery is low (10%-15%).";
    case DS4_LOWER:
        return "Battery is very low (5%-10%)!";
    case DS4_DEPLETED:
        return "Battery is almost depleted (<5%)!";
    }
}

static int get_urgency_timeout(enum Urgency u)
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

    argv0 = argv[0];

    if (!notify_init("ds4batlevel")) {
        fprintf(stderr, "%s: failed to initialize notifications\n", argv0);
        exit(1);
    }

    atexit(cleanup);

    ds4_fd = open(DS4_PATH, 0400, O_RDONLY);

    if (ds4_fd == -1) {
        fprintf(stderr, "%s: failed to open dual shock 4 path: %s\n", argv0, DS4_PATH);
        exit(1);
    }

    /* install signal handlers; ignore return value for now lol */
    signal(SIGINT, handlesig);
    signal(SIGHUP, handlesig);
    signal(SIGTERM, handlesig);
    signal(SIGQUIT, handlesig);
}

static void cleanup(void)
{
    notify_uninit();

    if (ds4_fd != -1) {
        close(ds4_fd);
    }
}

static int read_battery_level(void)
{
    static char buf[8];
    const ssize_t n = read(ds4_fd, buf, sizeof(buf));
    if (n == -1) {
        fprintf(stderr, "%s: failed to read battery level: %s\n", argv0, strerror(errno));
        exit(1);
    }
    lseek(ds4_fd, 0, SEEK_SET);
    buf[n] = '\0';
    return atoi(buf);
}

static enum Urgency get_battery_urgency(void)
{
    const int level = read_battery_level();

    if (level >= 15) {
        return DS4_HEALTHY;
    } else if (level >= 10 && level < 15) {
        return DS4_LOW;
    } else if (level >= 5 && level < 10) {
        return DS4_LOWER;
    } else {
        return DS4_DEPLETED;
    }
}
