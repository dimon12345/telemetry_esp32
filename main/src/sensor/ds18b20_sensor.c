#include "ds18b20_sensor.h"

#include "esp_check.h"
#include <sdkconfig.h>

#include "event_group_bits.h"

// 1byte ROM command + 8byte ROM number + 1byte device command
#define DS18B20_SCRATCHPAD_SIZE 10
static const char *TAG = "SENSOR";

esp_err_t sensor_init(sensor_handle_t *handle) {
    onewire_bus_config_t bus_config = {.bus_gpio_num = CONFIG_TELEMETRY_SENSOR_GPIO};

    onewire_bus_rmt_config_t rmt_config = {
            .max_rx_bytes = DS18B20_SCRATCHPAD_SIZE};
    onewire_device_iter_handle_t iter = NULL;
    ds18b20_config_t ds_cfg = {};
    onewire_device_t next_onewire_device;

    ESP_RETURN_ON_ERROR(
            onewire_new_bus_rmt(&bus_config, &rmt_config, &handle->bus), TAG,
            "New bus RMT error");

    ESP_RETURN_ON_ERROR(onewire_new_device_iter(handle->bus, &iter), TAG,
            "New device iterator error");
    ESP_RETURN_ON_ERROR(onewire_device_iter_get_next(iter, &next_onewire_device), TAG,
            "Next device error");
    ESP_RETURN_ON_ERROR(ds18b20_new_device_from_enumeration(
            &next_onewire_device, &ds_cfg, &handle->device), TAG, "New device error");
    ESP_RETURN_ON_ERROR(onewire_del_device_iter(iter), TAG, "Delete device iterator error");

    xEventGroupSetBits(handle->event_group, SENSOR_READY_BIT);

    return ESP_OK;
}

esp_err_t sensor_read_temperature(sensor_handle_t *handle, float *temperature) {
    EventBits_t bits = xEventGroupGetBits(handle->event_group);
    if ((bits & SENSOR_READY_BIT) == 0) {
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(
            ds18b20_trigger_temperature_conversion_for_all(handle->bus), TAG,
            "Trigger temperature conversion error");
    ESP_RETURN_ON_ERROR(ds18b20_get_temperature(handle->device, temperature),
            TAG, "Get temperature error");

    return ESP_OK;
}
