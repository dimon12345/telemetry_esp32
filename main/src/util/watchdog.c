#include "watchdog.h"

#include <esp_log.h>
#include <esp_task_wdt.h>

static const char *TAG = "WDT";

void watchdog_task_add(watchdog_handle_t *handler) {
    handler->err = esp_task_wdt_add(NULL);
    if (handler->err != ESP_OK) {
        ESP_LOGE(TAG, "WDT task add error: %s (0x%X)",
                esp_err_to_name(handler->err), handler->err);
    }
}

void watchdog_task_delete(watchdog_handle_t *handler) {
    if (handler->err == ESP_OK) {
        handler->err = esp_task_wdt_delete(NULL);
        if (handler->err != ESP_OK) {
            ESP_LOGE(TAG, "WDT task delete error: %s (0x%X)",
                    esp_err_to_name(handler->err), handler->err);
        }
    }
}
