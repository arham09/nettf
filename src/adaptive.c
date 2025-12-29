/**
 * @file adaptive.c
 * @brief Adaptive chunk sizing implementation for NETTF file transfer tool
 *
 * Dynamically adjusts transfer chunk size based on network conditions.
 * Aggressive adaptation: 8KB-2MB range, adjusts every 2-3 seconds.
 */

#define _GNU_SOURCE  // Enable snprintf() on older systems
#include "adaptive.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/**
 * @brief Initialize adaptive state for a new transfer
 */
void adaptive_init(AdaptiveState *state, uint64_t total_bytes) {
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(AdaptiveState));

    state->current_chunk_size = INITIAL_CHUNK_SIZE;
    state->last_adjustment_time = time(NULL);
    state->transfer_start_time = time(NULL);
    state->bytes_transferred = 0;
    state->total_bytes = total_bytes;
    state->bytes_sent_or_received = 0;
    state->sample_count = 0;
    state->sample_index = 0;

    // Initialize speed samples to 0
    for (int i = 0; i < SPEED_SAMPLES; i++) {
        state->speed_samples[i] = 0.0;
    }
}

/**
 * @brief Get current chunk size
 */
size_t adaptive_get_chunk_size(AdaptiveState *state) {
    if (state == NULL) {
        return INITIAL_CHUNK_SIZE;
    }

    // Ensure chunk size is within bounds
    if (state->current_chunk_size < MIN_CHUNK_SIZE) {
        state->current_chunk_size = MIN_CHUNK_SIZE;
    } else if (state->current_chunk_size > MAX_CHUNK_SIZE) {
        state->current_chunk_size = MAX_CHUNK_SIZE;
    }

    return state->current_chunk_size;
}

/**
 * @brief Calculate new chunk size based on average speed
 *
 * Aggressive adaptation algorithm:
 * - < 1 MB/s   → 8 KB   (MIN_CHUNK_SIZE)
 * - < 10 MB/s  → 64 KB  (INITIAL_CHUNK_SIZE)
 * - < 50 MB/s  → 256 KB
 * - < 100 MB/s → 1 MB
 * - ≥ 100 MB/s → 2 MB   (MAX_CHUNK_SIZE)
 */
static size_t calculate_new_chunk_size(double avg_speed) {
    const double MB = 1024.0 * 1024.0;

    if (avg_speed < 1.0 * MB) {
        return MIN_CHUNK_SIZE;  // 8 KB
    } else if (avg_speed < 10.0 * MB) {
        return 64 * 1024;  // 64 KB
    } else if (avg_speed < 50.0 * MB) {
        return 256 * 1024;  // 256 KB
    } else if (avg_speed < 100.0 * MB) {
        return 1 * 1024 * 1024;  // 1 MB
    } else {
        return MAX_CHUNK_SIZE;  // 2 MB
    }
}

/**
 * @brief Update state after transferring a chunk
 */
void adaptive_update(AdaptiveState *state, size_t bytes_transferred, double elapsed_time) {
    if (state == NULL || elapsed_time <= 0.0) {
        return;
    }

    // Calculate speed for this chunk (bytes/second)
    double chunk_speed = (double)bytes_transferred / elapsed_time;

    // Add to rolling window
    state->speed_samples[state->sample_index] = chunk_speed;
    state->sample_index = (state->sample_index + 1) % SPEED_SAMPLES;

    if (state->sample_count < SPEED_SAMPLES) {
        state->sample_count++;
    }

    // Update counters
    state->bytes_transferred += bytes_transferred;
    state->bytes_sent_or_received += bytes_transferred;

    // Check if it's time to adjust chunk size
    time_t current_time = time(NULL);
    double time_since_adjustment = difftime(current_time, state->last_adjustment_time);

    if (time_since_adjustment >= ADJUSTMENT_INTERVAL) {
        // Calculate average speed from samples
        double avg_speed = 0.0;
        for (int i = 0; i < state->sample_count; i++) {
            avg_speed += state->speed_samples[i];
        }
        if (state->sample_count > 0) {
            avg_speed /= state->sample_count;
        }

        // Calculate and apply new chunk size
        size_t new_chunk_size = calculate_new_chunk_size(avg_speed);

        if (new_chunk_size != state->current_chunk_size) {
            char old_str[32], new_str[32];
            adaptive_format_chunk_size(state->current_chunk_size, old_str, sizeof(old_str));
            adaptive_format_chunk_size(new_chunk_size, new_str, sizeof(new_str));
            // Note: Could log this change if logging is available
            // printf("Chunk size adjusted: %s → %s (speed: %.2f MB/s)\n",
            //        old_str, new_str, avg_speed / (1024.0 * 1024.0));
        }

        state->current_chunk_size = new_chunk_size;
        state->last_adjustment_time = current_time;
        state->bytes_transferred = 0;  // Reset for next interval
    }
}

/**
 * @brief Calculate and return current transfer speed
 */
double adaptive_get_current_speed(const AdaptiveState *state) {
    if (state == NULL || state->sample_count == 0) {
        return 0.0;
    }

    double avg_speed = 0.0;
    for (int i = 0; i < state->sample_count; i++) {
        avg_speed += state->speed_samples[i];
    }

    return avg_speed / state->sample_count;
}

/**
 * @brief Reset state for a new transfer
 */
void adaptive_reset(AdaptiveState *state) {
    if (state == NULL) {
        return;
    }

    // Preserve current chunk size but reset everything else
    size_t saved_chunk_size = state->current_chunk_size;

    memset(state, 0, sizeof(AdaptiveState));

    state->current_chunk_size = saved_chunk_size;
    state->last_adjustment_time = time(NULL);
    state->transfer_start_time = time(NULL);

    // Reset speed samples
    for (int i = 0; i < SPEED_SAMPLES; i++) {
        state->speed_samples[i] = 0.0;
    }
}

/**
 * @brief Format chunk size for display
 */
void adaptive_format_chunk_size(size_t bytes, char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    const double KB = 1024.0;
    const double MB = 1024.0 * 1024.0;

    if (bytes < MB) {
        snprintf(buffer, buffer_size, "%.0f KB", bytes / KB);
    } else {
        snprintf(buffer, buffer_size, "%.1f MB", bytes / MB);
    }
}
