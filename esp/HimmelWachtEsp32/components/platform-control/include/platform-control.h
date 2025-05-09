/**
 * @file
 * @brief Platform Control for the shooting platform
 *
 * This code uses the underlying servos to control the platform in x and y directions and prevents turning to angles that will possibly break parts.
 * The source code was entirely written by me and I came up with the formula by myself.
 *
 * This makes the assumption that two MG996R motors controlled by a PCA9685 PWM Board are used.
 *
 * @author Fabian Becker
 */
#ifndef _PLATFORM_CONTROL_H_
#define _PLATFORM_CONTROL_H_

#include "pca9685-driver.h"

// Available channels on the PWM Board
typedef enum{
    channel_0 = 0,
    channel_1,
    channel_2,
    channel_3,
    channel_4,
    channel_5,
    channel_6,
    channel_7,
    channel_8,
    channel_9,
    channel_10,
    channel_11,
    channel_12,
    channel_13,
    channel_14,
    channel_15,
} platform_motor_channel_t;

// Platform control configuration
typedef struct{
    pca9685_config_t pwm_board_config; // PCA9685 config
    platform_motor_channel_t platform_x_channel; // Channel on the PWM board for the x axis motor
    int8_t platform_x_start_angle; // X Starting position
    int8_t platform_x_left_stop_angle; // X left stop (prevent breaking parts)
    int8_t platform_x_right_stop_angle; // X right stop (prevent breaking parts)
    platform_motor_channel_t platform_y_channel; // Channel on the PWM board for the y axis motor
    int8_t platform_y_start_angle; // Y Starting position
    int8_t platform_y_left_stop_angle; // Y left stop (prevent breaking parts)
    int8_t platform_y_right_stop_angle; // Y left stop (prevent breaking parts)
} platform_config_t;

esp_err_t platform_init(platform_config_t *cfg);

esp_err_t platform_x_set_angle(int8_t angle);
esp_err_t platform_y_set_angle(int8_t angle);
esp_err_t platform_x_to_start();
esp_err_t platform_y_to_start();

#endif //_PLATFORM_CONTROL_H_
