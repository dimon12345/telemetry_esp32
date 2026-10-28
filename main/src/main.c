#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/projdefs.h>
#include <freertos/task.h>

#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "components/ds18b20_sensor.h"
#include "components/mqtt.h"
#include "components/wifi.h"

#include "event_group_bits.h"

static const char *TAG = "MAIN";

#define ONE_SECOND_TICKS pdMS_TO_TICKS(1000U)
static const TickType_t MAIN_LOOP_DELAY_TICKS = ONE_SECOND_TICKS;
static const TickType_t BACKOFF_MAX_TICKS = 60 * ONE_SECOND_TICKS;
static const TickType_t BACKOFF_INITIAL_TICKS = ONE_SECOND_TICKS;
static const uint64_t RESTART_ON_ERROR_TICKS =
        30U * 60 * (uint64_t)ONE_SECOND_TICKS;

static mqtt_handle_t mqtt_s = {.restart_on_error_ticks = RESTART_ON_ERROR_TICKS,
        .backoff.initial_interval_ticks = BACKOFF_INITIAL_TICKS,
        .backoff.max_interval_ticks = BACKOFF_MAX_TICKS};

static wifi_handle_t wifi_s;

static sensor_handle_t sensor_s = {
        .backoff.initial_interval_ticks = BACKOFF_INITIAL_TICKS,
        .backoff.max_interval_ticks = BACKOFF_MAX_TICKS};

EventGroupHandle_t event_group_g;

void app_main(void) {
    ESP_LOGI(TAG, "Start");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    event_group_g = xEventGroupCreate();
    if (event_group_g == NULL) {
        ESP_LOGE(TAG, "No memory. Restart ...");
        esp_restart();
    }

    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(wifi_init(&wifi_s));

    ESP_LOGI(TAG, "All ready!");

    uint64_t restart_timeout_ticks = RESTART_ON_ERROR_TICKS;
    TickType_t last_time_ticks = xTaskGetTickCount();

    esp_err_t wdt_err = esp_task_wdt_add(NULL);
    if (wdt_err != ESP_OK) {
        ESP_LOGE(TAG, "WDT task add error: %s (0x%X)", esp_err_to_name(wdt_err),
                wdt_err);
    }

    for (;;) {
        esp_task_wdt_reset();

        TickType_t now = xTaskGetTickCount();
        TickType_t elapsed = now - last_time_ticks;
        last_time_ticks = now;

        EventBits_t bits = xEventGroupGetBits(event_group_g);
        if (bits & RESTART_TIMEOUT_BIT) {
            if (elapsed > restart_timeout_ticks) {
                ESP_LOGE(TAG, "Can't recover from errors. Restart ...");
                esp_restart();
            } else {
                restart_timeout_ticks -= (uint64_t)elapsed;
            }
        } else {
            xEventGroupSetBits(event_group_g, RESTART_TIMEOUT_BIT);
            restart_timeout_ticks = RESTART_ON_ERROR_TICKS;
        }

        sensor_update(&sensor_s, elapsed);
        mqtt_update(&mqtt_s, elapsed);

        float temperature = 0.f;
        err = sensor_read_temperature(&sensor_s, &temperature);

        if (err == ESP_OK) {
            mqtt_temperature_for_send(&mqtt_s, temperature);
        } else if (err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Read temperature error: %s (0x%X)",
                    esp_err_to_name(err), err);
        }

        if (mqtt_check_status(&mqtt_s) == ESP_OK &&
                sensor_check_status(&sensor_s) == ESP_OK) {
            xEventGroupClearBits(event_group_g, RESTART_TIMEOUT_BIT);
        }

        vTaskDelay(MAIN_LOOP_DELAY_TICKS);
    }
}
