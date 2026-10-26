#include "watchdog.h"

#include <esp_log.h>
#include <esp_task_wdt.h>

static const char *TAG = "TWDT";

void watchdog_init(uint32_t timeout_ms) {
    esp_task_wdt_config_t wdt_config = {.timeout_ms = timeout_ms,
            .idle_core_mask = 0,
            .trigger_panic = true};

    esp_err_t err = esp_task_wdt_init(&wdt_config);
    if (err == ESP_ERR_INVALID_STATE) {
        err = esp_task_wdt_reconfigure(&wdt_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Task WDT reconfigure error: %s (0x%X)",
                    esp_err_to_name(err), err);
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Task WDT init error: %s (0x%X)", esp_err_to_name(err),
                err);
    }

    err = esp_task_wdt_add(NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Watchdog add task error: %s (0x%X)",
                esp_err_to_name(err), err);
    }
}

void watchdog_feed() {
    esp_err_t err = esp_task_wdt_reset();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Task WDT reset error: %s (0x%X)", esp_err_to_name(err),
                err);
    }
}
