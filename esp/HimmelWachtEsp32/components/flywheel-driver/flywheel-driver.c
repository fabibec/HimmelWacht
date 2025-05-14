#include "flywheel-driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define FLYWHEEL1_GPIO      5
#define FLYWHEEL2_GPIO      17
#define PWM_FREQ_HZ         20000
#define PWM_RESOLUTION      LEDC_TIMER_10_BIT
#define FLYWHEEL_DUTY       1023  // 676 ~66% of 1023 for 3.3V avg from 5V

#define PWM_TIMER           LEDC_TIMER_0
#define FLYWHEEL1_CHANNEL   LEDC_CHANNEL_0
#define FLYWHEEL2_CHANNEL   LEDC_CHANNEL_1

static const char *TAG = "FLYWHEEL";

void flywheel_init(void) {
    // Configure PWM timer
    ledc_timer_config_t timer_conf = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .timer_num        = PWM_TIMER,
        .duty_resolution  = PWM_RESOLUTION,
        .freq_hz          = PWM_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    // Configure channel for Flywheel 1
    ledc_channel_config_t fw1 = {
        .channel    = FLYWHEEL1_CHANNEL,
        .gpio_num   = FLYWHEEL1_GPIO,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_sel  = PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&fw1);

    // Configure channel for Flywheel 2
    ledc_channel_config_t fw2 = {
        .channel    = FLYWHEEL2_CHANNEL,
        .gpio_num   = FLYWHEEL2_GPIO,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_sel  = PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&fw2);

    ESP_LOGI(TAG, "Flywheel motors initialized.");
}

void flywheel_start(void) {
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, FLYWHEEL1_CHANNEL, FLYWHEEL_DUTY);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, FLYWHEEL1_CHANNEL);

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, FLYWHEEL2_CHANNEL, FLYWHEEL_DUTY);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, FLYWHEEL2_CHANNEL);

    ESP_LOGI(TAG, "Flywheels started.");
}

void flywheel_stop(void) {
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, FLYWHEEL1_CHANNEL, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, FLYWHEEL1_CHANNEL);

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, FLYWHEEL2_CHANNEL, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, FLYWHEEL2_CHANNEL);

    ESP_LOGI(TAG, "Flywheels stopped.");
}
