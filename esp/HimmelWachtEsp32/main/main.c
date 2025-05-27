#include "sdkconfig.h"

// wifi_tutorial.c
#include <stdio.h>

#include "esp_log.h"
#include "esp_wifi.h"

#include "wifi-stack.h"
#include "mqtt-stack.h"

#include "freertos/task.h"

#define TAG "main"

// Enter the Wi-Fi credentials here
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PW"

void app_main(void)
{
    ESP_LOGI(TAG, "Starting tutorial...");

    esp_err_t ret = wifi_stack_init(WIFI_SSID, WIFI_PASSWORD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi network");
    }

    wifi_ap_record_t ap_info;
    ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_ERR_WIFI_CONN) {
        ESP_LOGE(TAG, "Wi-Fi station interface not initialized");
    }
    else if (ret == ESP_ERR_WIFI_NOT_CONNECT) {
        ESP_LOGE(TAG, "Wi-Fi station is not connected");
    } else {
        ESP_LOGI(TAG, "--- Access Point Information ---");
        ESP_LOG_BUFFER_HEX("MAC Address", ap_info.bssid, sizeof(ap_info.bssid));
        ESP_LOG_BUFFER_CHAR("SSID", ap_info.ssid, sizeof(ap_info.ssid));
        ESP_LOGI(TAG, "Primary Channel: %d", ap_info.primary);
        ESP_LOGI(TAG, "RSSI: %d", ap_info.rssi);
    }

    ESP_LOGI(TAG, "Wi-Fi stack successfully initialized");

        // Configure MQTT component
    mqtt_config_t mqtt_config = {
        .broker_uri = "mqtt://192.168.178.33:1883",  // Replace with your broker IP
        .topic = "vehicle/turret/cmd",               // Configurable topic
        .client_id = "esp32_vehicle_01",             // Unique client ID
        .keepalive = 60                              // Keep alive interval
    };
    
    // Initialize MQTT component
    ret = mqtt_stack_init(&mqtt_config);
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to start MQTT component: %s", esp_err_to_name(ret));
        return;
    }
}
