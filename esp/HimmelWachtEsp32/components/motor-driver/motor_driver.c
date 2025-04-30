/**
 * NAMING CONVENTIONS:
 * motor_driver --> public API
 * motor_ --> internal
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "motor_driver.h"
#include <string.h>
#include <math.h>

#define TAG "MOTOR_DRIVER"

#define MOTOR_TASK_NAME "motor_task"
#define MOTOR_UPDATE_INTERVAL 10 // ms

// Forward declarations
static esp_err_t motor_set_pwm(motor_handle_t *motor, int duty_cycle);
static esp_err_t motor_set_dir(motor_handle_t *motor, motor_direction_t direction);
static esp_err_t motor_init(motor_handle_t *motor, const motor_config_t *config);

motor_handle_t *motor_driver_init(const motor_config_t *config)
{
    // Input validation
    if (config == NULL)
    {
        ESP_LOGE(TAG, "Motor config is NULL");
        return NULL;
    }

    // Create handle
    motor_handle_t *motor = (motor_handle_t *)calloc(1, sizeof(motor_handle_t));
    if (!motor)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for motor handle");
        return NULL;
    }
    else
    {
        motor->current_direction = MOTOR_DIRECTION_STOP;
        motor->current_speed = 0;
        motor->target_direction = MOTOR_DIRECTION_STOP;
        motor->target_speed = 0;
        motor->state = MOTOR_STATE_IDLE;
    }

    // Initialize motor
    esp_err_t ret = motor_init(motor, config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize motor");
        return NULL;
    }

    motor->initialized = true;
    ESP_LOGI(TAG, "Motor driver initialized successfully");

    return motor;
}

static esp_err_t motor_init(motor_handle_t *motor, const motor_config_t *config)
{
    // Store configuration
    memcpy(&motor->config, config, sizeof(motor_config_t));

    // Configure + Init direction GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->dir_gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure direction GPIO %d", config->dir_gpio_num);
        return ret;
    }

    // Initialize MCPWM GPIO
    ret = mcpwm_gpio_init(config->mcpwm_unit, config->pwm_signal, config->pwm_gpio_num);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize MCPWM GPIO %d", config->pwm_gpio_num);
        return ret;
    }

    // Configure MCPWM timer
    mcpwm_config_t pwm_config = {
        .frequency = config->pwm_frequency_hz,
        .cmpr_a = 0,
        .cmpr_b = 0,
        .duty_mode = MCPWM_DUTY_MODE_0,
        .counter_mode = MCPWM_UP_COUNTER,
    };

    ret = mcpwm_init(config->mcpwm_unit, config->timer_num, &pwm_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize MCPWM timer %d", config->timer_num);
        return ret;
    }

    // Set initial state
    motor_set_dir(motor, motor->target_direction);
    motor_set_pwm(motor, motor->target_speed);

    return ESP_OK;
}

esp_err_t motor_driver_set_speed(motor_handle_t *motor, unsigned char duty_cycle, motor_direction_t direction)
{
    if (motor == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    motor->target_speed = duty_cycle;
    motor->target_direction = direction;

    if (motor->current_direction != direction)
    {
        if (motor->state != MOTOR_STATE_CHANGING_DIRECTION)
        {
            motor_set_pwm(motor, 0); // Immediately stop PWM
            motor->state = MOTOR_STATE_CHANGING_DIRECTION;
            motor->state_start_time = xTaskGetTickCount();
        }
    }
    else
    {
        // No direction change needed → ramp toward target speed
        motor->state = MOTOR_STATE_RUNNING;
    }

    return ESP_OK;
}

static esp_err_t motor_set_pwm(motor_handle_t *motor, int duty_cycle)
{
    if (duty_cycle < 0)
    {
        duty_cycle = 0;
    }
    if (duty_cycle > 100)
    {
        duty_cycle = 100;
    }

    // Convert percentage to actual duty cycle
    float duty = (float)duty_cycle;

    mcpwm_set_duty(MCPWM_UNIT_0, motor->config.timer_num, motor->config.generator, duty);
    mcpwm_set_duty_type(MCPWM_UNIT_0, motor->config.timer_num, motor->config.generator, MCPWM_DUTY_MODE_0);

    return ESP_OK;
}

static esp_err_t motor_set_dir(motor_handle_t *motor, motor_direction_t direction)
{
    switch (direction)
    {
    case MOTOR_DIRECTION_FORWARD:
        gpio_set_level(motor->config.dir_gpio_num, 1);
        break;
    case MOTOR_DIRECTION_BACKWARD:
        gpio_set_level(motor->config.dir_gpio_num, 0);
        break;
    case MOTOR_DIRECTION_STOP:
        // For stop, direction doesn't matter as PWM will be zero
        break;
    default:
        ESP_LOGE(TAG, "Invalid direction: %d", direction);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t motor_driver_update(motor_handle_t *motor)
{
    if (motor == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    switch (motor->state)
    {
    case MOTOR_STATE_CHANGING_DIRECTION:
        if (xTaskGetTickCount() - motor->state_start_time >= pdMS_TO_TICKS(10))
        {
            // After 10ms, safe to switch direction
            motor_set_dir(motor, motor->target_direction);
            motor->current_direction = motor->target_direction;
            motor->state = MOTOR_STATE_RUNNING;
        }
        break;

    case MOTOR_STATE_RUNNING:
        if (motor->current_speed != motor->target_speed)
        {
            // Ramp current speed toward target
            if (motor->current_speed < motor->target_speed)
            {
                motor->current_speed += motor->config.ramp_rate;
                if (motor->current_speed > motor->target_speed)
                    motor->current_speed = motor->target_speed;
            }
            else
            {
                motor->current_speed -= motor->config.ramp_rate;
                if (motor->current_speed < motor->target_speed)
                    motor->current_speed = motor->target_speed;
            }

            motor_set_pwm(motor, motor->current_speed);
        }
        break;

        /*
        case MOTOR_STATE_DEINITIALIZING:
        if (motor->speed > 0)
        {
            // Ramp speed down toward 0
            motor->speed -= MOTOR_RAMP_STEP;
            if (motor->speed < 0)
                motor->speed = 0;

            motor_set_pwm(motor, motor->speed);
        }
        else
        {
            // Fully stopped: set STOP direction
            motor_set_dir(motor, MOTOR_DIRECTION_STOP);

            // Clear flags
            motor_driver.initialized = false;
            motor->state = MOTOR_STATE_IDLE;

            ESP_LOGI(TAG, "Motor driver deinitialized cleanly");
        }
        break;
        */

    case MOTOR_STATE_IDLE:
    default:
        // Do nothing
        break;
    }

    return ESP_OK;
}

esp_err_t motor_driver_deinit(motor_handle_t *motor)
{
    if (motor == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!motor->initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    motor_set_pwm(motor, 0);
    motor_set_dir(motor, MOTOR_DIRECTION_STOP);

    free(motor);

    ESP_LOGI(TAG, "Motor deinitialized");

    return ESP_OK;

    /**
    if (!motor_driver.initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Request motor to stop smoothly
    motor_driver.motor.target_speed = 0;
    motor_driver.motor.target_direction = MOTOR_DIRECTION_STOP;
    motor_driver.motor.state = MOTOR_STATE_DEINITIALIZING;

    ESP_LOGI(TAG, "Motor driver deinitializing...");

    return ESP_OK;
     */
}