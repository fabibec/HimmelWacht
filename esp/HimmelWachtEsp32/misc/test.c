#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "motor-driver.h"

#define TAG "DUAL_MOTOR_TEST"

// LEFT Motor Pin Configuration
#define LEFT_MOTOR_PWM_GPIO   23
#define LEFT_MOTOR_DIR_GPIO   22
#define LEFT_MOTOR_FAULT_GPIO 21
#define LEFT_MOTOR_FAULT_LED  19

// RIGHT Motor Pin Configuration
#define RIGHT_MOTOR_PWM_GPIO   27
#define RIGHT_MOTOR_DIR_GPIO   26
#define RIGHT_MOTOR_FAULT_GPIO 25
#define RIGHT_MOTOR_FAULT_LED  32

#define TEST_PWM_FREQ 20000

// Global handles
motor_handle_t *left_motor = NULL;
motor_handle_t *right_motor = NULL;

static void test_dual_motor_speed_control(void);
static void test_task(void *pvParameters);

void app_main4(void)
{
    ESP_LOGI(TAG, "Starting Dual Motor Test Suite");
    xTaskCreate(test_task, "dual_motor_test_task", 8192, NULL, 5, NULL);
}

static void test_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Init LEFT motor
    motor_config_t left_config = {
        .mcpwm_unit = MCPWM_UNIT_0,
        .timer_num = MCPWM_TIMER_0,
        .generator = MCPWM_OPR_A,
        .pwm_signal = MCPWM0A,
        .pwm_gpio_num = LEFT_MOTOR_PWM_GPIO,
        .dir_gpio_num = LEFT_MOTOR_DIR_GPIO,
        .fault_gpio_num = LEFT_MOTOR_FAULT_GPIO,
        .fault_led_gpio_num = LEFT_MOTOR_FAULT_LED,
        .pwm_frequency_hz = TEST_PWM_FREQ,
        .ramp_rate = 10,
        .ramp_intervall_ms = 50,
        .direction_hysteresis = 5,
        .pwm_duty_limit = 50.0
    };

    left_motor = motor_driver_init(&left_config);
    if (!left_motor) {
        ESP_LOGE(TAG, "Failed to initialize LEFT motor");
        vTaskDelete(NULL);
    }

    // Init RIGHT motor
    motor_config_t right_config = {
        .mcpwm_unit = MCPWM_UNIT_0,  // Use a different MCPWM unit if available
        .timer_num = MCPWM_TIMER_1,
        .generator = MCPWM_OPR,
        .pwm_signal = MCPWM1A,
        .pwm_gpio_num = RIGHT_MOTOR_PWM_GPIO,
        .dir_gpio_num = RIGHT_MOTOR_DIR_GPIO,
        .fault_gpio_num = RIGHT_MOTOR_FAULT_GPIO,
        .fault_led_gpio_num = RIGHT_MOTOR_FAULT_LED,
        .pwm_frequency_hz = TEST_PWM_FREQ,
        .ramp_rate = 10,
        .ramp_intervall_ms = 50,
        .direction_hysteresis = 5,
        .pwm_duty_limit = 50.0
    };

    right_motor = motor_driver_init(&right_config);
    if (!right_motor) {
        ESP_LOGE(TAG, "Failed to initialize RIGHT motor");
        motor_driver_deinit(left_motor);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "--- Running Dual Motor Speed Control Test ---");
    test_dual_motor_speed_control();

    motor_driver_deinit(left_motor);
    motor_driver_deinit(right_motor);

    ESP_LOGI(TAG, "--- Dual Motor Test Complete ---");
    vTaskDelete(NULL);
}

static void test_dual_motor_speed_control(void)
{
    float speeds[] = {0, 30, 60, 90, 0};
    int count = sizeof(speeds) / sizeof(speeds[0]);

    for (int i = 0; i < count; i++)
    {
        ESP_LOGI(TAG, "Setting LEFT motor to %.0f%% FORWARD", speeds[i]);
        motor_driver_set_speed(left_motor, speeds[i], MOTOR_DIRECTION_FORWARD);

        ESP_LOGI(TAG, "Setting RIGHT motor to %.0f%% BACKWARD", speeds[i]);
        motor_driver_set_speed(right_motor, speeds[i], MOTOR_DIRECTION_BACKWARD);

        for (int j = 0; j < 20; j++)
        {
            motor_driver_update(left_motor);
            motor_driver_update(right_motor);

            ESP_LOGI(TAG, "LEFT: PWM %.2f | RIGHT: PWM %.2f",
                     left_motor->current_pwm, right_motor->current_pwm);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "Stopping both motors");
    motor_driver_set_speed(left_motor, 0, MOTOR_DIRECTION_STOP);
    motor_driver_set_speed(right_motor, 0, MOTOR_DIRECTION_STOP);

    for (int i = 0; i < 20; i++)
    {
        motor_driver_update(left_motor);
        motor_driver_update(right_motor);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
