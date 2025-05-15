#include "fire-control.h"
#include "pca9685-driver.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <driver/gpio.h>
#include <esp_log.h>

#define GUN_ARM_SERVO_START_VALUE 400
#define GUN_ARM_SERVO_STOP_VALUE 240

#define FIRE_CONTROL_TAG "Fire Control"

#define TRIGGER_SHOT (1 << 0)

static int8_t gun_arm_channel = 0;
static EventGroupHandle_t fire_control_event_group = NULL;
static uint64_t flywheel_gpio_port = 0;

/*
    Task that controls the firing mechanism.
    This task is controlled by the event bit TRIGGER_SHOT, which it will unset when the shot was executed fully.

    @param arg: unused

    @author Fabian Becker
*/
static void fire_control_task(void* arg) {
    const char* TAG = "Fire Control Task";

    while (1) {
        // Wait for the event to trigger the shot
        xEventGroupWaitBits(fire_control_event_group, TRIGGER_SHOT, pdFALSE, pdFALSE, portMAX_DELAY);

        // Start the flywheel motors
        gpio_set_level(flywheel_gpio_port, 1);
        vTaskDelay(pdMS_TO_TICKS(500));

        // Trigger the shot by moving the servo motor to the specified angle
        esp_err_t ret = pca9685_set_pwm_on_off(gun_arm_channel, 0, GUN_ARM_SERVO_STOP_VALUE);
        if (ret != ESP_OK) {
            ESP_LOGE(
                FIRE_CONTROL_TAG,
                "%s: Unable to trigger shot",
                TAG
            );
        }

        // Delay for a short period to allow the servo to move
        vTaskDelay(pdMS_TO_TICKS(140));

        // Stop the flywheels after the shot
        gpio_set_level(flywheel_gpio_port, 0);

        // Move the servo back to the starting position
        ret = pca9685_set_pwm_on_off(gun_arm_channel, 0, GUN_ARM_SERVO_START_VALUE);
        if (ret != ESP_OK) {
            ESP_LOGE(
                FIRE_CONTROL_TAG,
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

/*
    Initializes the fire control system by configuring the GPIO,
    driving the gun arm to the starting position and creating the task for event handling.

    @param cfg Configuration data

    @return ESP_OK on success, ESP_ERR_INVALID_ARG if configuration is invalid, ESP error code on failure

    @note This needs to be called after pca9685_init() to ensure the PCA9685 is initialized.

    @author Fabian Becker
*/
esp_err_t fire_control_init(fire_control_config_t *cfg) {
    const char* TAG = "Init";
    esp_err_t ret;

    // Check if the configuration is valid
    if(cfg == NULL) {
        ESP_LOGE(
            FIRE_CONTROL_TAG,
            "%s: Invalid configuration",
            TAG
        );
        return ESP_ERR_INVALID_ARG;
    }
    if(cfg->run_on_core > 1) {
        ESP_LOGE(
            FIRE_CONTROL_TAG,
            "%s: Invalid core number (%d), must be 0 or 1",
            TAG,
            cfg->run_on_core
        );
        return ESP_ERR_INVALID_ARG;
    };
    if(cfg->gun_arm_channel > 15) {
        ESP_LOGE(
            FIRE_CONTROL_TAG,
            "%s: Invalid gun arm channel (%d), must be between 0 and 15",
            TAG,
            cfg->gun_arm_channel
        );
        return ESP_ERR_INVALID_ARG;
    }
    if(cfg->flywheel_control_gpio_port > 39) {
        ESP_LOGE(
            FIRE_CONTROL_TAG,
            "%s: Invalid flywheel control GPIO port (%d), must be between 0 and 39",
            TAG,
            cfg->flywheel_control_gpio_port
        );
        return ESP_ERR_INVALID_ARG;
    }

    gun_arm_channel = cfg->gun_arm_channel;
    flywheel_gpio_port = cfg->flywheel_control_gpio_port;

    // Configure the GPIO for flywheel control
    gpio_config_t flywheel_cfg = {
        .pin_bit_mask = (1ULL << flywheel_gpio_port),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&flywheel_cfg);

    // Turn flywheel off initially
    gpio_set_level(flywheel_gpio_port, 0);

    // Set the initial position of gun arm servo motor
    ret = pca9685_set_pwm_on_off(cfg->gun_arm_channel, 0, GUN_ARM_SERVO_START_VALUE);
    if (ret != ESP_OK) {
        ESP_LOGE(
            FIRE_CONTROL_TAG,
            "%s: Unable to set gun arm servo motor to starting position",
            TAG
        );
        return ret;
    }

    // Create the event group for fire control
    fire_control_event_group = xEventGroupCreate();
    if (fire_control_event_group == NULL) {
        ESP_LOGE(
            FIRE_CONTROL_TAG,
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
        cfg->run_on_core /* Core where the task should run */
    );

    if (task_created != pdTRUE) {
        ESP_LOGE(
            FIRE_CONTROL_TAG,
            "%s: Failed to create bluepad32 task",
            TAG
        );
        return ESP_FAIL;
    }

    ESP_LOGI(
        FIRE_CONTROL_TAG,
        "%s: Fire control initialized successfully",
        TAG
    );

    // Delay for servo to reach the starting position
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

/*
    Triggers the shot by setting the TRIGGER_SHOT event bit.
    This will start the fire control task to execute the shot.

    @return ESP_OK on success, ESP_ERR_NOT_FINISHED if the shot is already triggered

    @note The function will return ESP_ERR_NOT_FINISHED without executing the shot, if the shot is already triggered

    @author Fabian Becker
*/
esp_err_t fire_control_trigger_shot() {
    const char* TAG = "Trigger Shot";

    if(xEventGroupGetBits(fire_control_event_group) & TRIGGER_SHOT) {
        ESP_LOGW(
            FIRE_CONTROL_TAG,
            "%s: Shot already triggered, ignoring request",
            TAG
        );
        return ESP_ERR_NOT_FINISHED;
    }

    xEventGroupSetBits(fire_control_event_group, TRIGGER_SHOT);

    return ESP_OK;
}
