#pragma once

#include <esp_err.h>

typedef struct {
    esp_err_t err;
} watchdog_handle_t;

void watchdog_task_add(watchdog_handle_t *handler);
void watchdog_task_delete(watchdog_handle_t *handler);
