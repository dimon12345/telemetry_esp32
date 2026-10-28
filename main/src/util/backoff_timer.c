#include "backoff_timer.h"

#include <assert.h>

#include <esp_log.h>

static const char *TAG = "BACKOFF";

esp_err_t backoff_update(backoff_handle_t *handle, uint64_t elapsed) {
    assert(handle != NULL);

    if (handle->current_interval_ticks == 0ULL) {
        return ESP_OK;
    }

    if (elapsed < handle->remaining_ticks) {
        handle->remaining_ticks -= elapsed;
        return ESP_ERR_INVALID_STATE;
    } else {
        handle->remaining_ticks = 0ULL;
        return ESP_OK;
    }
}

void backoff_start(backoff_handle_t *handle) {
    assert(handle != NULL);
    assert(handle->initial_interval_ticks > 0ULL);
    assert(handle->max_interval_ticks > handle->initial_interval_ticks);

    handle->remaining_ticks = handle->initial_interval_ticks;
    handle->current_interval_ticks = handle->initial_interval_ticks;
}

void backoff_on_error(backoff_handle_t *handle) {
    assert(handle != NULL);

    if (handle->current_interval_ticks == 0) {
        backoff_start(handle);
    } else {
        handle->remaining_ticks = handle->current_interval_ticks;
        handle->current_interval_ticks *= 2;
        if (handle->current_interval_ticks > handle->max_interval_ticks) {
            handle->current_interval_ticks = handle->max_interval_ticks;
        }
    }
}

void backoff_on_success(backoff_handle_t *handle) {
    assert(handle != NULL);
    handle->current_interval_ticks = 0ULL;
}
