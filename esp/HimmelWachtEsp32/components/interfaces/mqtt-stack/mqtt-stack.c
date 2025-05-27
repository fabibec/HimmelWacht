#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <string.h>
#include "mqtt-stack.h"

static const char *TAG = "MQTT_STACK";

// MQTT client handle
static esp_mqtt_client_handle_t mqtt_client = NULL;

// Queue for turret commands
static QueueHandle_t turret_cmd_queue = NULL;

// MQTT configuration storage
static mqtt_config_t mqtt_cfg;

// Connection status
static bool mqtt_connected = false;
static SemaphoreHandle_t connection_mutex = NULL;

// Function prototypes
static void set_connection_status(bool connected);
static bool get_connection_status(void);
esp_err_t start(void);
static void stack_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static bool parse_turret_command(const char *data, int data_len, mqtt_turret_cmd_t *cmd);
void destroy(void);

// Function to set connection status safely
static void set_connection_status(bool connected) {
    if (connection_mutex) {
        xSemaphoreTake(connection_mutex, portMAX_DELAY);
        mqtt_connected = connected;
        xSemaphoreGive(connection_mutex);
    }
}

// Function to get connection status safely
static bool get_connection_status(void) {
    bool status = false;
    if (connection_mutex) {
        xSemaphoreTake(connection_mutex, portMAX_DELAY);
        status = mqtt_connected;
        xSemaphoreGive(connection_mutex);
    }
    return status;
}

// Parse JSON message and extract turret command
static bool parse_turret_command(const char *data, int data_len, mqtt_turret_cmd_t *cmd) {
    cJSON *json = cJSON_ParseWithLength(data, data_len);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return false;
    }

    bool success = true;
    
    // Parse platform_x (required)
    cJSON *platform_x_json = cJSON_GetObjectItem(json, "platform_x");
    if (cJSON_IsNumber(platform_x_json)) {
        cmd->platform_x = (uint16_t)platform_x_json->valueint;
    } else {
        ESP_LOGE(TAG, "Missing or invalid platform_x");
        success = false;
    }

    // Parse platform_y (required)
    cJSON *platform_y_json = cJSON_GetObjectItem(json, "platform_y");
    if (cJSON_IsNumber(platform_y_json)) {
        cmd->platform_y = (uint16_t)platform_y_json->valueint;
    } else {
        ESP_LOGE(TAG, "Missing or invalid platform_y");
        success = false;
    }

    // Parse fire_command (required)
    cJSON *fire_cmd_json = cJSON_GetObjectItem(json, "fire_command");
    if (cJSON_IsNumber(fire_cmd_json)) {
        cmd->fire_command = (uint16_t)fire_cmd_json->valueint;
    } else {
        ESP_LOGE(TAG, "Missing or invalid fire_command");
        success = false;
    }

    // Parse optional color override
    cJSON *color_override_json = cJSON_GetObjectItem(json, "color_override");
    if (cJSON_IsBool(color_override_json)) {
        cmd->color_override = cJSON_IsTrue(color_override_json);
        
        if (cmd->color_override) {
            cJSON *r_json = cJSON_GetObjectItem(json, "override_r");
            cJSON *g_json = cJSON_GetObjectItem(json, "override_g");
            cJSON *b_json = cJSON_GetObjectItem(json, "override_b");
            
            if (cJSON_IsNumber(r_json) && cJSON_IsNumber(g_json) && cJSON_IsNumber(b_json)) {
                cmd->override_r = (uint8_t)r_json->valueint;
                cmd->override_g = (uint8_t)g_json->valueint;
                cmd->override_b = (uint8_t)b_json->valueint;
            } else {
                ESP_LOGW(TAG, "Color override enabled but RGB values missing or invalid");
                cmd->color_override = false;
            }
        }
    } else {
        cmd->color_override = false;
    }

    cJSON_Delete(json);
    return success;
}

// MQTT event handler
static void stack_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            set_connection_status(true);
            
            // Subscribe to the configured topic
            int msg_id = esp_mqtt_client_subscribe(mqtt_client, mqtt_cfg.topic, 1);
            ESP_LOGI(TAG, "Subscribed to topic %s, msg_id=%d", mqtt_cfg.topic, msg_id);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            set_connection_status(false);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
            
            // Parse and queue the turret command
            if (turret_cmd_queue != NULL) {
                mqtt_turret_cmd_t cmd;
                if (parse_turret_command(event->data, event->data_len, &cmd)) {
                    if (xQueueSend(turret_cmd_queue, &cmd, 0) != pdTRUE) {
                        ESP_LOGW(TAG, "Turret command queue full, dropping command");
                    }
                }
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            set_connection_status(false);
            break;

        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

esp_err_t mqtt_stack_init(const mqtt_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Copy configuration
    memcpy(&mqtt_cfg, config, sizeof(mqtt_config_t));

    // Create connection mutex
    connection_mutex = xSemaphoreCreateMutex();
    if (connection_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create connection mutex");
        return ESP_ERR_NO_MEM;
    }

    // Create turret command queue (capacity of 5 commands)
    turret_cmd_queue = xQueueCreate(5, sizeof(mqtt_turret_cmd_t));
    if (turret_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create turret command queue");
        vSemaphoreDelete(connection_mutex);
        return ESP_ERR_NO_MEM;
    }

    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = mqtt_cfg.broker_uri,
        .session.keepalive = mqtt_cfg.keepalive,
        .credentials.client_id = mqtt_cfg.client_id,
        .network.reconnect_timeout_ms = mqtt_cfg.reconnect_timeout_ms,
        .network.timeout_ms = mqtt_cfg.network_timeout_ms,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        vQueueDelete(turret_cmd_queue);
        vSemaphoreDelete(connection_mutex);
        return ESP_ERR_NO_MEM;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, stack_event_handler, NULL);

    ESP_LOGI(TAG, "MQTT component initialized");
    ESP_LOGI(TAG, "Broker: %s", mqtt_cfg.broker_uri);
    ESP_LOGI(TAG, "Topic: %s", mqtt_cfg.topic);
    ESP_LOGI(TAG, "Client ID: %s", mqtt_cfg.client_id);

    if(start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client");
        mqtt_stack_deinit();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MQTT client started successfully");

    return ESP_OK;
}

void destroy(void) {
    if (mqtt_client != NULL) {
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }
    
    if (turret_cmd_queue != NULL) {
        vQueueDelete(turret_cmd_queue);
        turret_cmd_queue = NULL;
    }
    
    if (connection_mutex != NULL) {
        vSemaphoreDelete(connection_mutex);
        connection_mutex = NULL;
    }
    
    set_connection_status(false);
}

esp_err_t start(void) {
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MQTT client started");
    return ESP_OK;
}

esp_err_t mqtt_stack_deinit(void) {
    if (mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_mqtt_client_stop(mqtt_client);
    
    destroy();
    
    return ret;
}

esp_err_t mqtt_stack_get_turret_command(mqtt_turret_cmd_t *cmd) {
    if (cmd == NULL || turret_cmd_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    //pdMS_TO_TICKS    
    if (xQueueReceive(turret_cmd_queue, cmd, mqtt_cfg.queue_timeout_ticks) == pdTRUE) {
        return ESP_OK;
    } else {
        return ESP_ERR_TIMEOUT;
    }
}

bool mqtt_stack_is_connected(void) {
    return get_connection_status();
}