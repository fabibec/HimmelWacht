#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// MQTT turret command structure
typedef struct
{
    uint16_t platform_x;   // Horizontal angle/position
    uint16_t platform_y;   // Vertical angle/position
    uint16_t fire_command; // Fire command (>800 to fire)
    bool color_override;   // Whether to override DS4 lightbar color
    uint8_t override_r;    // Red component (0-255)
    uint8_t override_g;    // Green component (0-255)
    uint8_t override_b;    // Blue component (0-255)
} mqtt_turret_cmd_t;

// MQTT component configuration
typedef struct
{
    char broker_uri[32];         // MQTT broker URI (e.g., "mqtt://192.168.1.100:1883")
    char topic[64];              // Topic to subscribe to
    char client_id[32];          // MQTT client ID
    uint16_t keepalive;          // Keep alive interval in seconds
    uint32_t network_timeout_ms; // Network timeout in milliseconds
    uint32_t reconnect_timeout_ms; // Reconnect timeout in milliseconds
    uint8_t queue_timeout_ticks;   // Queue timeout in ticks
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
 * @return esp_err_t ESP_OK if command received, ESP_ERR_TIMEOUT if timeout
 */
esp_err_t mqtt_stack_get_turret_command(mqtt_turret_cmd_t *cmd);

/**
 * @brief Check if MQTT client is connected
 *
 * @return true if connected, false otherwise
 */
bool mqtt_stack_is_connected(void);