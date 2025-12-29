/**
 * @file signals.h
 * @brief Signal handling interface for NETTF file transfer tool
 *
 * Provides graceful Ctrl+C handling with double-press force exit.
 * First Ctrl+C prompts user, second Ctrl+C forces immediate exit.
 * POSIX-only; Windows uses stub implementations.
 */

#ifndef SIGNALS_H
#define SIGNALS_H

// Platform detection
#ifdef _WIN32
    #define SIGNALS_SUPPORTED 0
#else
    #define SIGNALS_SUPPORTED 1
    #include <stddef.h>
#endif

#if SIGNALS_SUPPORTED

/**
 * @brief Initialize signal handlers
 *
 * Sets up handler for SIGINT (Ctrl+C) using sigaction().
 * The handler uses an atomic counter to track the number of SIGINT signals received.
 *
 * @return 0 on success, -1 on failure (prints error to stderr)
 */
int signals_init(void);

/**
 * @brief Clean up signal handlers
 *
 * Resets signal handlers to default behavior.
 */
void signals_cleanup(void);

/**
 * @brief Check if graceful shutdown was requested
 *
 * Returns the current shutdown state based on number of Ctrl+C presses:
 * - 0: Continue normal operation
 * - 1: First Ctrl+C received - prompt user, continue after acknowledgment
 * - 2: Second Ctrl+C received - force immediate exit
 *
 * @return 0 = continue, 1 = prompt user, 2 = force exit
 */
int signals_should_shutdown(void);

/**
 * @brief Acknowledge the first Ctrl+C
 *
 * Keeps the signal counter at 1 after prompting the user.
 * This prevents the first Ctrl+C from triggering a force exit.
 */
void signals_acknowledge_shutdown(void);

/**
 * @brief Get the name of the last signal received
 *
 * Returns a string representation of the last signal for logging purposes.
 *
 * @return Signal name string (e.g., "SIGINT") or "none"
 */
const char* signals_get_last_signal_name(void);

#else  // Windows stubs

// Stub implementations for Windows
#define signals_init() 0
#define signals_cleanup()
#define signals_should_shutdown() 0
#define signals_acknowledge_shutdown()
#define signals_get_last_signal_name() "not_supported"

#endif  // SIGNALS_SUPPORTED

#endif // SIGNALS_H
