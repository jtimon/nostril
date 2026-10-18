#include <signal.h>

/* Signal handlers only set a flag; NNG teardown runs on the main thread. */
static volatile sig_atomic_t relay_stop = 0;

static void stop_relay(int signum) {
    (void)signum;
    relay_stop = 1;
}

int nostril_relay_signals(void) {
    relay_stop = 0;
    if (signal(SIGINT, stop_relay) == SIG_ERR) return -1;
    if (signal(SIGTERM, stop_relay) == SIG_ERR) return -1;
    return 0;
}

int nostril_relay_running(void) {
    return !relay_stop;
}
