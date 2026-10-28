#pragma once

#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_err.h>

typedef struct {
    uint64_t remaining_ticks;
    uint64_t current_interval_ticks;
    uint64_t initial_interval_ticks;
    uint64_t max_interval_ticks;
} backoff_handle_t;

esp_err_t backoff_update(backoff_handle_t *handle, uint64_t elapsed);
void backoff_on_error(backoff_handle_t *handle);
void backoff_on_success(backoff_handle_t *handle);
