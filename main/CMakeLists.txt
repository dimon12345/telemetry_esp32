set(TELEMETRY_SOURCES
    src/main.c
    src/sensor/ds18b20_sensor.c
    src/wifi/wifi.c
    src/mqtt/mqtt.c
)
set(TELEMETRY_HEADERS
    src/
)

idf_component_register(
    SRCS ${TELEMETRY_SOURCES}
    INCLUDE_DIRS ${TELEMETRY_HEADERS}
    PRIV_REQUIRES esp_wifi esp_event esp_netif mqtt nvs_flash
)
