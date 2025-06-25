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
#include "log_wrapper.h"

static const char *TAG = "MQTT_STACK";

static void set_connection_status(bool connected);
static bool get_connection_status(void);
static bool parse_turret_command(const char *data, int data_len, mqtt_turret_cmd_t *cmd);
static void stack_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void destroy(void);
static esp_err_t start(void);
esp_err_t mqtt_stack_init(const mqtt_config_t *config);
esp_err_t mqtt_stack_deinit(void);
esp_err_t mqtt_stack_get_turret_command(mqtt_turret_cmd_t *cmd);
bool mqtt_stack_is_connected(void);

static esp_mqtt_client_handle_t mqtt_client = NULL;
static QueueHandle_t turret_cmd_queue = NULL;
static mqtt_config_t mqtt_cfg;
static bool mqtt_connected = false;
static bool discard_commands = true;
static SemaphoreHandle_t connection_mutex = NULL;
static SemaphoreHandle_t discard_command_mutex = NULL;

// Function prototypes
static void set_connection_status(bool connected);
static bool get_connection_status(void);
void set_discard_command_status(bool connected);
bool get_discard_command_status(void);
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

static bool get_connection_status(void) {
    bool status = false;
    if (connection_mutex) {
        xSemaphoreTake(connection_mutex, portMAX_DELAY);
        status = mqtt_connected;
        xSemaphoreGive(connection_mutex);
    }
    return status;
}

void set_discard_command_status(bool discard) {
    if (discard_command_mutex) {
        xSemaphoreTake(discard_command_mutex, portMAX_DELAY);
        discard_commands = discard;
        if(turret_cmd_queue != NULL) {
            xQueueReset(turret_cmd_queue);
            ESP_LOGI(TAG, "Turret command queue reset due to discard command status change");
        }
        xSemaphoreGive(discard_command_mutex);
    }
}

bool get_discard_command_status(void) {
    bool status = false;
    if (discard_command_mutex) {
        xSemaphoreTake(discard_command_mutex, portMAX_DELAY);
        status = discard_commands;
        xSemaphoreGive(discard_command_mutex);
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
    
    // Parse platform_x_angle (required)
    cJSON *platform_x_angle_json = cJSON_GetObjectItem(json, "platform_x_angle");
    if (cJSON_IsNumber(platform_x_angle_json)) {
        cmd->platform_x_angle = (int8_t)platform_x_angle_json->valueint;
    } else {
        ESP_LOGE(TAG, "Missing or invalid platform_x_angle");
        success = false;
    }

    // Parse platform_y_angle (required)
    cJSON *platform_y_angle_json = cJSON_GetObjectItem(json, "platform_y_angle");
    if (cJSON_IsNumber(platform_y_angle_json)) {
        cmd->platform_y_angle = (int8_t)platform_y_angle_json->valueint;
    } else {
        ESP_LOGE(TAG, "Missing or invalid platform_y_angle");
        success = false;
    }

    // Parse fire_command (required)
    cJSON *fire_cmd_json = cJSON_GetObjectItem(json, "fire_command");
    if (cJSON_IsBool(fire_cmd_json)) {
        cmd->fire_command = cJSON_IsTrue(fire_cmd_json);
    } else {
        ESP_LOGE(TAG, "Missing or invalid fire_command");
        success = false;
    }

    cJSON_Delete(json);
    return success;
}

static void stack_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            LOGI(TAG, "MQTT_EVENT_CONNECTED");
            set_connection_status(true);
            
            // Subscribe to the configured topic
            int msg_id = esp_mqtt_client_subscribe(mqtt_client, mqtt_cfg.topic, 1);
            LOGI(TAG, "Subscribed to topic %s, msg_id=%d", mqtt_cfg.topic, msg_id);
            break;

        case MQTT_EVENT_DISCONNECTED:
            LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            set_connection_status(false);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            LOGI(TAG, "MQTT_EVENT_DATA");
            LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            LOGI(TAG, "DATA=%.*s", event->data_len, event->data);

            if(get_discard_command_status()) {
                LOGI(TAG, "Discarding command due to discard_commands flag");
                break;
            }else{
                LOGI(TAG, "Processing command");
            }
            
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
            LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            set_connection_status(false);
            break;

        default:
            LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

esp_err_t mqtt_stack_init(const mqtt_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&mqtt_cfg, config, sizeof(mqtt_config_t));

    connection_mutex = xSemaphoreCreateMutex();
    if (connection_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create connection mutex");
        return ESP_ERR_NO_MEM;
    }

    // Create discard command mutex
    discard_command_mutex = xSemaphoreCreateMutex();
    if (discard_command_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create discard command mutex");
        vSemaphoreDelete(connection_mutex);
        return ESP_ERR_NO_MEM;
    }

    // Create turret command queue (capacity of 5 commands)
    turret_cmd_queue = xQueueCreate(5, sizeof(mqtt_turret_cmd_t));
    if (turret_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create turret command queue");
        vSemaphoreDelete(connection_mutex);
        vSemaphoreDelete(discard_command_mutex);
        return ESP_ERR_NO_MEM;
    }

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

    LOGI(TAG, "MQTT component initialized");
    LOGI(TAG, "Broker: %s", mqtt_cfg.broker_uri);
    LOGI(TAG, "Topic: %s", mqtt_cfg.topic);
    LOGI(TAG, "Client ID: %s", mqtt_cfg.client_id);

    if(start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client");
        mqtt_stack_deinit();
        return ESP_FAIL;
    }

    LOGI(TAG, "MQTT client started successfully");

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

    LOGI(TAG, "MQTT client started");
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

    if (xQueueReceive(turret_cmd_queue, cmd, mqtt_cfg.queue_timeout_ticks) == pdTRUE) {
        return ESP_OK;
    } else {
        return ESP_ERR_TIMEOUT;
    }
}

bool mqtt_stack_is_connected(void) {
    return get_connection_status();
}