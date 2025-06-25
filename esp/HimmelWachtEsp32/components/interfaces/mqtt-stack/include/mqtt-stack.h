/**
 * @file mqtt-stack.h
 * @brief MQTT stack interface for turret control
 * 
 * This header file defines the MQTT stack interface for controlling the turret system.
 * It includes the necessary structures, function declarations, and configuration options.
 * 
 * Reference:
 *  - https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/mqtt.html
 *  - https://github.com/espressif/esp-idf/blob/v5.4.1/examples/protocols/mqtt/tcp/main/app_main.c
 * 
 * @author Michael Specht
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int8_t platform_x_angle;
    int8_t platform_y_angle;
    bool fire_command;
} mqtt_turret_cmd_t;

typedef struct
{
    char broker_uri[32];         // e.g., "mqtt://192.168.1.100:1883"
    char topic[64];
    char client_id[32];
    uint16_t keepalive;
    uint32_t network_timeout_ms;
    uint32_t reconnect_timeout_ms;
    uint8_t queue_timeout_ticks;
} mqtt_config_t;

/**
 * @brief Initialize MQTT component
 *
 * @param config MQTT configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mqtt_stack_init(const mqtt_config_t *config);

/**
 * @brief Stop MQTT client
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mqtt_stack_deinit(void);

/**
 * @brief Get the latest turret command
 *
 * @param cmd Pointer to store the command
 * @return esp_err_t ESP_OK if command received, ESP_ERR_TIMEOUT if timeout, ESP_ERR_INVALID_ARG if cmd is NULL
 */
esp_err_t mqtt_stack_get_turret_command(mqtt_turret_cmd_t *cmd);

/**
 * @brief Check if MQTT client is connected
 *
 * @return true if connected, false otherwise
 */
bool mqtt_stack_is_connected(void);

void set_discard_command_status(bool connected);
bool get_discard_command_status(void);