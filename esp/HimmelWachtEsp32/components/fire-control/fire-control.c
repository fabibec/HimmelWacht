#include "fire-control.h"
#include "pca9685-driver.h"
#include "flywheel-driver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>

#define GUN_ARM_SERVO_START_VALUE 400
#define GUN_ARM_SERVO_STOP_VALUE 240

#define COMPONENT_TAG "Fire Control"

#define TRIGGER_SHOT (1 << 0)

static fire_control_channel_t gun_arm_channel = 0;
static EventGroupHandle_t fire_control_event_group = NULL;

static void fire_control_task(void* arg) {
    static char *TAG = "Fire Control Task";

    while (1) {
        // Wait for the event to trigger the shot
        xEventGroupWaitBits(fire_control_event_group, TRIGGER_SHOT, pdFALSE, pdFALSE, portMAX_DELAY);

        flywheel_start(); // Start the flywheel
        vTaskDelay(pdMS_TO_TICKS(500)); // Wait for the flywheel to start

        // Trigger the shot by moving the servo motor to the specified angle
        esp_err_t ret = pca9685_set_pwm_on_off(gun_arm_channel, 0, GUN_ARM_SERVO_STOP_VALUE);
        if (ret != ESP_OK) {
            ESP_LOGE(
                COMPONENT_TAG,
                "%s: Unable to trigger shot",
                TAG
            );
        }

        // Delay for a short period to allow the servo to move
        vTaskDelay(pdMS_TO_TICKS(140));

        // Stop the flywheel after the shot
        flywheel_stop();

        // Move the servo back to the starting position
        ret = pca9685_set_pwm_on_off(gun_arm_channel, 0, GUN_ARM_SERVO_START_VALUE);
        if (ret != ESP_OK) {
            ESP_LOGE(
                COMPONENT_TAG,
                "%s: Unable to move gun arm back to starting position",
                TAG
            );
        }

        // Delay for a short period to allow the servo to move
        vTaskDelay(pdMS_TO_TICKS(140));

        // Clear the event bit after the shot is triggered
        xEventGroupClearBits(fire_control_event_group, TRIGGER_SHOT);
    }
}

esp_err_t fire_control_init(fire_control_config_t *cfg) {
    esp_err_t ret;
    static char *TAG = "Init";

    // Set the gun arm servo motor into starting position (assumes that the PWM board is already initialized)
    gun_arm_channel = cfg->gun_arm_channel;

    ret = pca9685_set_pwm_on_off(cfg->gun_arm_channel, 0, 0); // Set the initial position to 0 degrees
    if (ret != ESP_OK) {
        ESP_LOGE(
            COMPONENT_TAG,
            "%s: Unable to set gun arm servo motor to starting position",
            TAG
        );
        return ret;
    }

    // Create the event group for fire control
    fire_control_event_group = xEventGroupCreate();
    if (fire_control_event_group == NULL) {
        ESP_LOGE(
            COMPONENT_TAG,
            "%s: Failed to create event group",
            TAG
        );
        return ESP_FAIL;
    }

    BaseType_t task_created = pdFALSE;

    // Create the fire control task
    task_created = xTaskCreatePinnedToCore(
        fire_control_task,   /* Function to implement the task */
        "firecontrol_task", /* Name of the task */
        4096,       /* Stack size in words */
        NULL,  /* Task input parameter */
        1,          /* Priority of the task */
        NULL,       /* Task handle. */
        0 /* Core where the task should run */
    );

    if (task_created != pdTRUE) {
        ESP_LOGE(
            COMPONENT_TAG,
            "%s: Failed to create bluepad32 task",
            TAG
        );
        return ESP_FAIL;
    }

    ESP_LOGI(
        COMPONENT_TAG,
        "%s: Fire control initialized successfully",
        TAG
    );

    // Delay for servo to reach the starting position
    vTaskDelay(pdMS_TO_TICKS(100));

    flywheel_init();

    return ESP_OK;
}

esp_err_t fire_control_trigger_shot() {
    const char* TAG = "Trigger Shot";

    if(xEventGroupGetBits(fire_control_event_group) & TRIGGER_SHOT) {
        ESP_LOGW(
            COMPONENT_TAG,
            "%s: Shot already triggered, ignoring request",
            TAG
        );
        return ESP_ERR_NOT_FINISHED;
    }

    xEventGroupSetBits(fire_control_event_group, TRIGGER_SHOT);

    return ESP_OK;
}


