/**
 * @file signals.c
 * @brief Signal handling implementation for NETTF file transfer tool
 *
 * Implements graceful Ctrl+C handling using POSIX sigaction().
 * First Ctrl+C prompts user, second Ctrl+C forces immediate exit.
 */

#define _GNU_SOURCE  // Enable strdup() on Linux systems
#include "signals.h"

#if SIGNALS_SUPPORTED

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>

// Global atomic counter for SIGINT signals
static volatile atomic_int sigint_count = 0;

// Track last signal for logging
static volatile sig_atomic_t last_signal = 0;

// Original signal action for restoration
static struct sigaction original_sigaction;

/**
 * @brief Signal handler function for SIGINT
 *
 * This function is called asynchronously when SIGINT (Ctrl+C) is received.
 * It atomically increments the signal counter and records the signal type.
 *
 * @param signo The signal number received
 */
static void signal_handler(int signo) {
    if (signo == SIGINT) {
        atomic_fetch_add(&sigint_count, 1);
        last_signal = signo;
    }
}

/**
 * @brief Initialize signal handlers
 */
int signals_init(void) {
    struct sigaction sa;

    // Initialize signal counter
    atomic_store(&sigint_count, 0);
    last_signal = 0;

    // Set up signal handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // Restart interrupted system calls

    // Install signal handler for SIGINT
    if (sigaction(SIGINT, &sa, &original_sigaction) == -1) {
        perror("sigaction");
        return -1;
    }

    return 0;
}

/**
 * @brief Clean up signal handlers
 */
void signals_cleanup(void) {
    // Restore original signal handler
    sigaction(SIGINT, &original_sigaction, NULL);

    // Reset counters
    atomic_store(&sigint_count, 0);
    last_signal = 0;
}

/**
 * @brief Check if graceful shutdown was requested
 */
int signals_should_shutdown(void) {
    int count = atomic_load(&sigint_count);

    if (count >= 2) {
        return 2;  // Force exit
    } else if (count == 1) {
        return 1;  // Prompt user
    }

    return 0;  // Continue
}

/**
 * @brief Acknowledge the first Ctrl+C
 */
void signals_acknowledge_shutdown(void) {
    // Keep count at 1 - don't reset to 0
    // This prevents the first Ctrl+C from triggering a force exit
    // The count stays at 1 until a second Ctrl+C arrives
}

/**
 * @brief Get the name of the last signal received
 */
const char* signals_get_last_signal_name(void) {
    switch (last_signal) {
        case SIGINT:
            return "SIGINT";
        case 0:
            return "none";
        default:
            return "unknown";
    }
}

#endif  // SIGNALS_SUPPORTED
