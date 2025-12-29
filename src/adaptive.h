/**
 * @file adaptive.h
 * @brief Adaptive chunk sizing for NETTF file transfer tool
 *
 * Dynamically adjusts transfer chunk size (8KB-2MB) based on network conditions.
 * Uses aggressive adaptation with 2-3 second intervals for quick response.
 */

#ifndef ADAPTIVE_H
#define ADAPTIVE_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/**
 * @brief Minimum chunk size (8 KB)
 */
#define MIN_CHUNK_SIZE    (8 * 1024)

/**
 * @brief Maximum chunk size (2 MB)
 */
#define MAX_CHUNK_SIZE    (2 * 1024 * 1024)

/**
 * @brief Initial chunk size (64 KB)
 */
#define INITIAL_CHUNK_SIZE (64 * 1024)

/**
 * @brief Seconds between chunk size adjustments
 */
#define ADJUSTMENT_INTERVAL 2

/**
 * @brief Number of speed samples to maintain for rolling average
 */
#define SPEED_SAMPLES      5

/**
 * @brief Adaptive chunk size state tracker
 *
 * Maintains state for adaptive chunk sizing including current chunk size,
 * speed tracking, and transfer statistics.
 */
typedef struct {
    size_t current_chunk_size;        // Current chunk size in bytes
    time_t last_adjustment_time;      // Time of last size adjustment
    uint64_t bytes_transferred;       // Bytes since last adjustment
    time_t transfer_start_time;       // Start of current transfer

    // Speed tracking (rolling window)
    double speed_samples[SPEED_SAMPLES];
    int sample_count;
    int sample_index;

    // Statistics
    uint64_t total_bytes;             // Total bytes in transfer
    uint64_t bytes_sent_or_received;  // Bytes transferred so far
} AdaptiveState;

/**
 * @brief Initialize adaptive state for a new transfer
 *
 * Sets up the adaptive state with initial chunk size and resets all counters.
 *
 * @param state Pointer to AdaptiveState structure to initialize
 * @param total_bytes Total size of file being transferred (0 for unknown)
 */
void adaptive_init(AdaptiveState *state, uint64_t total_bytes);

/**
 * @brief Get current chunk size
 *
 * Returns the current chunk size to use for the next transfer operation.
 * The chunk size is automatically adjusted based on network conditions.
 *
 * @param state Pointer to AdaptiveState structure
 * @return Current chunk size in bytes (between MIN_CHUNK_SIZE and MAX_CHUNK_SIZE)
 */
size_t adaptive_get_chunk_size(AdaptiveState *state);

/**
 * @brief Update state after transferring a chunk
 *
 * Records transfer speed and adjusts chunk size if enough time has elapsed.
 * Should be called after each chunk is transferred.
 *
 * @param state Pointer to AdaptiveState structure
 * @param bytes_transferred Actual number of bytes sent/received
 * @param elapsed_time Time taken for this chunk (in seconds)
 */
void adaptive_update(AdaptiveState *state, size_t bytes_transferred, double elapsed_time);

/**
 * @brief Calculate and return current transfer speed
 *
 * Returns the average transfer speed based on recent samples.
 *
 * @param state Pointer to AdaptiveState structure
 * @return Current speed in bytes per second
 */
double adaptive_get_current_speed(const AdaptiveState *state);

/**
 * @brief Reset state for a new transfer
 *
 * Resets counters and speeds while preserving the current chunk size.
 *
 * @param state Pointer to AdaptiveState structure
 */
void adaptive_reset(AdaptiveState *state);

/**
 * @brief Format chunk size for display
 *
 * Converts chunk size to human-readable format (e.g., "64 KB", "1.5 MB").
 *
 * @param bytes Chunk size in bytes
 * @param buffer Output buffer for formatted string
 * @param buffer_size Size of output buffer
 */
void adaptive_format_chunk_size(size_t bytes, char *buffer, size_t buffer_size);

#endif // ADAPTIVE_H
