#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "motor-driver.h"
#include <string.h>
#include <math.h>
#include "log_wrapper.h"

#define TAG "MOTOR_DRIVER"

#define MOTOR_TASK_NAME "motor_task"

// Forward declarations
static esp_err_t set_pwm(motor_handle_t *motor, float duty_cycle);
static esp_err_t set_dir(motor_handle_t *motor, motor_direction_t direction);
static esp_err_t init_motor(motor_handle_t *motor, const motor_config_t *config);
static void IRAM_ATTR fault_isr_handler(void *arg);

static uint8_t instance_cntr = 0;
uint8_t instance_nr = 0;

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
        motor->current_pwm = 0;
        motor->target_direction = MOTOR_DIRECTION_STOP;
        motor->target_pwm = 0;
        motor->last_update_ms = 0;
        LOGI(TAG, "Motor instance %d created", instance_cntr);
    }

    // Initialize motor
    esp_err_t ret = init_motor(motor, config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize motor");
        free(motor);
        return NULL;
    }

    instance_nr = instance_cntr;
    instance_cntr++;
    motor->initialized = true;
    LOGI(TAG, "Motor driver initialized successfully");

    return motor;
}

static esp_err_t init_motor(motor_handle_t *motor, const motor_config_t *config)
{
    // Store configuration
    memcpy(&motor->config, config, sizeof(motor_config_t));

    esp_err_t ret;
    // Configure + Init direction GPIO
    if (config->dir_gpio_num != GPIO_NUM_NC)
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << config->dir_gpio_num),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ret = gpio_config(&io_conf);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to configure direction GPIO %d", config->dir_gpio_num);
            return ret;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Direction GPIO not configured");
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize MCPWM GPIO
    if (config->pwm_gpio_num != GPIO_NUM_NC)
    {
        ret = mcpwm_gpio_init(config->mcpwm_unit, config->pwm_signal, config->pwm_gpio_num);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize MCPWM GPIO %d", config->pwm_gpio_num);
            return ret;
        }
    }
    else
    {
        ESP_LOGE(TAG, "MCPWM GPIO not configured");
        return ESP_ERR_INVALID_ARG;
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

    // Configure fault GPIO
    if (config->fault_gpio_num != GPIO_NUM_NC)
    {
        gpio_config_t fault_io_conf = {
            .pin_bit_mask = (1ULL << config->fault_gpio_num),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE};

        ret = gpio_config(&fault_io_conf);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to configure fault GPIO %d", config->fault_gpio_num);
            return ret;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Fault GPIO not configured");
        return ESP_ERR_INVALID_ARG;
    }

    if (instance_cntr == 0)
    {
        ret = gpio_install_isr_service(0);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to install ISR service");
            return ret;
        }
    }

    ret = gpio_isr_handler_add(config->fault_gpio_num, fault_isr_handler, motor);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add ISR handler for fault GPIO %d", config->fault_gpio_num);
        return ret;
    }

    // Configure fault LED GPIO
    if (config->fault_led_gpio_num != GPIO_NUM_NC)
    {
        gpio_config_t fault_led_io_conf = {
            .pin_bit_mask = (1ULL << config->fault_led_gpio_num),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE};

        ret = gpio_config(&fault_led_io_conf);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to configure fault LED GPIO %d", config->fault_led_gpio_num);
            return ret;
        }
        else
        {
            LOGI(TAG, "Fault LED GPIO %d configured", config->fault_led_gpio_num);
        }
        // gpio_set_level(config->fault_led_gpio_num, 0); // Turn off LED
    }
    else
    {
        ESP_LOGE(TAG, "Fault LED GPIO not configured");
        return ESP_ERR_INVALID_ARG;
    }

    // Set initial state
    set_dir(motor, motor->target_direction);
    set_pwm(motor, motor->target_pwm);

    return ESP_OK;
}

// Add this ISR handler
static void IRAM_ATTR fault_isr_handler(void *arg) {
    // motor_handle_t *motor = (motor_handle_t *)arg;

    // // Set fault flag
    // motor->fault_active = true;

    // // Turn on fault LED immediately from ISR
    // gpio_set_level(motor->config.fault_led_gpio_num, 0);
};

bool motor_driver_is_fault_active(motor_handle_t *motor)
{
    if (motor == NULL)
    {
        return false;
    }

    return motor->fault_active;
}

esp_err_t motor_driver_clear_fault(motor_handle_t *motor)
{
    if (motor == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Only clear if fault is inactive at the GPIO level
    if (gpio_get_level(motor->config.fault_gpio_num) == 0)
    { // Assuming active low fault
        motor->fault_active = false;
        gpio_set_level(motor->config.fault_led_gpio_num, 1); // Turn off LED
        return ESP_OK;
    }
    else
    {
        return ESP_ERR_INVALID_STATE; // Fault still active at hardware level
    }
}

esp_err_t motor_driver_emergency_stop(motor_handle_t *motor)
{
    if (motor == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Set PWM to 0 and direction to stop
    set_pwm(motor, 0);
    set_dir(motor, MOTOR_DIRECTION_STOP);

    return ESP_OK;
}

inline bool motor_driver_is_update_necessary(motor_handle_t *motor)
{
    bool check;
    
    LOGI(TAG, "motor->target_pwm (%.2f) - motor->current_pwm (%.2f): %.2f", motor->target_pwm, motor->current_pwm, fabs(motor->target_pwm - motor->current_pwm));
    LOGI(TAG, "motor->target_direction != motor->current_direction: %d", motor->target_direction != motor->current_direction);

    // check if change in dir or pwm is detected
    if ((fabs(motor->target_pwm - motor->current_pwm) > motor->config.direction_hysteresis) || (motor->target_direction != motor->current_direction))
    {
        check = true;
    }
    else
    {
        check = false;
    }

    LOGI(TAG, "motor_driver_is_update_necessary: %d for instance: %d", check, motor->config.mynr);

    return check;
}

esp_err_t motor_driver_set_speed(motor_handle_t *motor, float duty_cycle, motor_direction_t direction)
{
    if (motor == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    motor->target_pwm = duty_cycle;
    motor->target_direction = direction;

    return ESP_OK;
}

static esp_err_t set_pwm(motor_handle_t *motor, float duty_cycle)
{
    if (duty_cycle < 0)
    {
        duty_cycle = 0;
    }
    if (duty_cycle > motor->config.pwm_duty_limit)
    {
        duty_cycle = motor->config.pwm_duty_limit;
    }

    mcpwm_set_duty(MCPWM_UNIT_0, motor->config.timer_num, motor->config.generator, duty_cycle);
    mcpwm_set_duty_type(MCPWM_UNIT_0, motor->config.timer_num, motor->config.generator, MCPWM_DUTY_MODE_0);

    return ESP_OK;
}

static esp_err_t set_dir(motor_handle_t *motor, motor_direction_t direction)
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

    if (motor->fault_active)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (now - motor->last_update_ms >= motor->config.ramp_intervall_ms)
    {
        motor->last_update_ms = now;

        if (motor->current_direction != motor->target_direction && motor->current_pwm > motor->config.direction_hysteresis)
        {
            LOGI(TAG, "update instance %d: motor->current_pwm (%.2f) -= motor->config.ramp_rate (%d) --> %.2f",
                     motor->config.mynr, motor->current_pwm, motor->config.ramp_rate, motor->current_pwm - motor->config.ramp_rate);
            motor->current_pwm -= motor->config.ramp_rate;
        }
        else if (motor->current_direction != motor->target_direction && motor->current_pwm < -motor->config.direction_hysteresis)
        {
            LOGI(TAG, "update instance %d: motor->current_pwm (%.2f) += motor->config.ramp_rate (%d) --> %.2f",
                     motor->config.mynr, motor->current_pwm, motor->config.ramp_rate, motor->current_pwm + motor->config.ramp_rate);
            motor->current_pwm += motor->config.ramp_rate;
        }
        else if (motor->current_direction != motor->target_direction)
        {
            LOGI(TAG, "update instance %d: motor->current_direction (%d) = motor->target_direction (%d)",
                     motor->config.mynr, motor->current_direction, motor->target_direction);
            motor->current_direction = motor->target_direction;
        }
        else if (motor->current_pwm < motor->target_pwm)
        {
            LOGI(TAG, "update instance %d: motor->current_pwm (%.2f) += motor->config.ramp_rate (%d) --> %.2f",
                     motor->config.mynr, motor->current_pwm, motor->config.ramp_rate, motor->current_pwm + motor->config.ramp_rate);
            motor->current_pwm += motor->config.ramp_rate;
        }
        else if (motor->current_pwm > motor->target_pwm)
        {
            LOGI(TAG, "update instance %d: motor->current_pwm (%.2f) -= motor->config.ramp_rate (%d) --> %.2f",
                     motor->config.mynr, motor->current_pwm, motor->config.ramp_rate, motor->current_pwm - motor->config.ramp_rate);
            motor->current_pwm -= motor->config.ramp_rate;
        }
        else
        {
            // stay in current state
            LOGI(TAG, "stay");
        }

        LOGI(TAG, "Instance %d: Current PWM: %.2f, Target PWM: %.2f, Current Direction: %d, Target Direction: %d",
                 motor->config.mynr, motor->current_pwm, motor->target_pwm, motor->current_direction, motor->target_direction);

        set_dir(motor, motor->current_direction);
        set_pwm(motor, motor->current_pwm);
    }

    return ESP_OK;
}

void motor_driver_print_all_parameters(motor_handle_t *motor)
{
    if (motor == NULL)
    {
        ESP_LOGE(TAG, "Motor is NULL");
        return;
    }

    LOGI(TAG, "Motor Parameters:");
    LOGI(TAG, "  Current PWM: %.2f", motor->current_pwm);
    LOGI(TAG, "  Target PWM: %.2f", motor->target_pwm);
    LOGI(TAG, "  Current Direction: %d", motor->current_direction);
    LOGI(TAG, "  Target Direction: %d", motor->target_direction);
    LOGI(TAG, "  Last Update Time: %ld ms", motor->last_update_ms);
    LOGI(TAG, "  Ramp Rate: %d", motor->config.ramp_rate);
    LOGI(TAG, "  Ramp Interval: %d ms", motor->config.ramp_intervall_ms);
    LOGI(TAG, "  Direction Hysteresis: %d", motor->config.direction_hysteresis);
    LOGI(TAG, "  PWM Duty Limit: %.2f", motor->config.pwm_duty_limit);
    LOGI(TAG, "  Fault Active: %d", motor->fault_active);
    LOGI(TAG, "  Instance Number: %d", instance_nr);
    LOGI(TAG, "  Instance Counter: %d", instance_cntr);
    LOGI(TAG, "  Mynr: %d", motor->config.mynr);
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

    motor_driver_emergency_stop(motor);

    gpio_isr_handler_remove(motor->config.fault_gpio_num);
    instance_cntr--;

    if (instance_cntr == 0)
    {
        gpio_uninstall_isr_service();
    }

    free(motor);
    motor = NULL;

    LOGI(TAG, "Motor deinitialized");

    return ESP_OK;
}