set(TELEMETRY_SOURCES
    src/components/ds18b20_sensor.c
    src/components/mqtt.c
    src/components/wifi.c
    src/util/backoff_timer.c
    src/util/watchdog.c
    src/main.c
)

set(TELEMETRY_HEADERS
    src/
)

idf_component_register(
    SRCS ${TELEMETRY_SOURCES}
    INCLUDE_DIRS ${TELEMETRY_HEADERS}
    PRIV_REQUIRES esp_wifi esp_event esp_netif esp_timer mqtt esp_driver_gpio nvs_flash
)
