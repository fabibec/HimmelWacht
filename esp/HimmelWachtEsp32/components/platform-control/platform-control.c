#include "platform-control.h"

#include <math.h>
#include <esp_err.h>
#include <esp_log.h>

static uint8_t platform_x_channel = 0;
static uint8_t platform_y_channel = 0;
static int8_t platform_x_start_angle = 0;
static int8_t platform_y_start_angle = 0;
static int8_t platform_x_left_stop_angle = 0;
static int8_t platform_y_left_stop_angle = 0;
static int8_t platform_x_right_stop_angle = 0;
static int8_t platform_y_right_stop_angle = 0;

const uint16_t MINUS_NINETY_DEGREES = 125;
const uint16_t ZERO_DEGREES = 335;
const uint16_t NINETY_DEGREES = 545;

#define PLATFORM_COMPONENT_TAG "Platform Control"

esp_err_t platform_set_angle(uint8_t channel, int8_t angle);

/*
    Initializes the platform the configuring the PWM Board and turning both motors into starting position.

    @param cfg Configuration data

    @return ESP_OK on success, ESP error code on failure

    @note the Configured angles will not be checked, only rotation outside of [-90,90] will be prevented, if invalid values are supplied

    @author Fabian Becker
*/
esp_err_t platform_init(platform_config_t *cfg){
    esp_err_t ret;
    static char *TAG = "Init";

    // Initialize the PWM Board
    if (pca9685_init(&cfg->pwm_board_config) != ESP_OK){
        return ESP_FAIL;
    }

    platform_x_channel = cfg->platform_x_channel;
    platform_y_channel = cfg->platform_y_channel;
    platform_x_start_angle = cfg->platform_x_start_angle;
    platform_y_start_angle = cfg->platform_y_start_angle;
    platform_x_left_stop_angle = cfg->platform_x_left_stop_angle;
    platform_y_left_stop_angle = cfg->platform_y_left_stop_angle;
    platform_x_right_stop_angle = cfg->platform_x_right_stop_angle;
    platform_y_right_stop_angle = cfg->platform_y_right_stop_angle;

    // Set the platform motors to their starting positions
    ret = platform_set_angle(platform_x_channel, platform_x_start_angle);
    if(ret != ESP_OK){
        ESP_LOGE(
            PLATFORM_COMPONENT_TAG,
            "%s: Unable to set platform x to starting position",
            TAG
        );
        return ret;
    }
    ret = platform_set_angle(platform_y_channel, platform_y_start_angle);
    if(ret != ESP_OK){
        ESP_LOGE(
            PLATFORM_COMPONENT_TAG,
            "%s: Unable to set platform y to starting position",
            TAG
        );
        return ret;
    }

    return ESP_OK;
}

/*
    Rotate x motor to a certain angle. The supplied angle will be validated w.r.t. the configured stop angles.

    @param angle the target position

    @return ESP_OK on success, ESP error code on failure

    @note The speed of the motor is roughly 200ms per 60 degrees. The motor doesn't have a 1 degree precision, so for fine rotation you need to turn 2-3 degrees at once.

    @author Fabian Becker
*/
esp_err_t platform_x_set_angle(int8_t angle){
    const char* TAG = "Platform X set angle:";

    // Check if the angle is in the range [left-stop,right-stop], otherwise clip value
    if(angle < platform_x_left_stop_angle){
        ESP_LOGW(
            PLATFORM_COMPONENT_TAG,
            "%s: Angle %d smaller than the left stop angle. Clipping the value.",
            TAG,
            angle
        );
        angle = platform_x_left_stop_angle;
    } else if (angle > platform_x_right_stop_angle){
        ESP_LOGW(
            PLATFORM_COMPONENT_TAG,
            "%s: Angle %d greater than the right stop angle. Clipping the value.",
            TAG,
            angle
        );
        angle = platform_x_right_stop_angle;
    }
    return platform_set_angle(platform_x_channel, angle);
}

/*
    Rotate y motor to a certain angle. The supplied angle will be validated w.r.t. the configured stop angles.

    @param angle the target position

    @return ESP_OK on success, ESP error code on failure

    @note The speed of the motor is roughly 200ms per 60 degrees. The motor doesn't have a 1 degree precision, so for fine rotation you need to turn 2-3 degrees at once.

    @author Fabian Becker
*/
esp_err_t platform_y_set_angle(int8_t angle){
    const char* TAG = "Platform Y set angle:";

    // Check if the angle is in the range [left-stop,right-stop], otherwise clip value
    if(angle < platform_y_left_stop_angle){
        ESP_LOGW(
            PLATFORM_COMPONENT_TAG,
            "%s: Angle %d smaller than the left stop angle. Clipping the value.",
            TAG,
            angle
        );
        angle = platform_y_left_stop_angle;
    } else if (angle > platform_y_right_stop_angle){
        ESP_LOGW(
            PLATFORM_COMPONENT_TAG,
            "%s: Angle %d greater than the right stop angle. Clipping the value.",
            TAG,
            angle
        );
        angle = platform_y_right_stop_angle;
    }
    return platform_set_angle(platform_y_channel, angle);
}

/*
    Rotate x motor to its starting position.

    @return ESP_OK on success, ESP error code on failure

    @note The speed of the motor is roughly 200ms per 60 degrees. The motor doesn't have a 1 degree precision, so for fine rotation you need to turn 2-3 degrees at once.

    @author Fabian Becker
*/
esp_err_t platform_x_to_start(){
    return platform_set_angle(platform_x_channel, platform_x_start_angle);
}

/*
    Rotate y motor to its starting position.

    @return ESP_OK on success, ESP error code on failure

    @note The speed of the motor is roughly 200ms per 60 degrees. The motor doesn't have a 1 degree precision, so for fine rotation you need to turn 2-3 degrees at once.

    @author Fabian Becker
*/
esp_err_t platform_y_to_start(){
    return platform_set_angle(platform_y_channel, platform_y_start_angle);
}

/*
    Private function that is used to turn degrees in to PWM cycles.

    @param angle the target position

    @return ESP_OK on success, ESP error code on failure

    @note The speed of the motor is roughly 200ms per 60 degrees. The motor doesn't have a 1 degree precision, so for fine rotation you need to turn 2-3 degrees at once.

    @author Fabian Becker
*/
esp_err_t platform_set_angle(uint8_t channel, int8_t angle){
    uint16_t off_period = 0;
    const char* TAG = "Set angle";

    switch (angle){
        // Use precalculated values if possible
        case 0:
            off_period = ZERO_DEGREES;
            break;
        case -90:
            off_period = MINUS_NINETY_DEGREES;
            break;
        case 90:
            off_period = NINETY_DEGREES;
            break;
        default:
            if(angle > 90 || angle < -90){
                ESP_LOGW(
                    PLATFORM_COMPONENT_TAG,
                    "%s: Angle %d not in range [-90,90]! Value will be clipped to closest number.",
                    TAG,
                    angle
                );
                if(angle < 0){
                    off_period = MINUS_NINETY_DEGREES;
                } else {
                    off_period = NINETY_DEGREES;
                }
            } else {
                /*
                    If we divide the range from 0 to 90 in to equal parts, we get 2.3333.
                    Therefor we turn 2 and for every third degree we turn 3 to not introduce mismatch over time.
                */
                uint8_t abs_angle = abs(angle);
                uint8_t three_steps = abs_angle / 3;  // cutoff wanted!
                uint8_t two_steps = abs_angle - three_steps;

                int16_t step_amount = two_steps * 2 + three_steps * 3;
                step_amount = (angle > 0) ? step_amount : (step_amount * -1);
                off_period = ZERO_DEGREES + step_amount;
            }
            break;
    }

    // Error checking happens inside of the function
    return pca9685_set_pwm_on_off(channel, 0, off_period);
}

/*
    Get the configured value for the left stop angle of the x motor.

    @return the configured value

    @author Fabian Becker
*/
int8_t platform_get_x_left_stop_angle(){
    return platform_x_left_stop_angle;
}

/*
    Get the configured value for the right stop angle of the x motor.

    @return the configured value

    @author Fabian Becker
*/
int8_t platform_get_x_right_stop_angle(){
    return platform_x_right_stop_angle;
}

/*
    Get the configured value for the left stop angle of the y motor.

    @return the configured value

    @author Fabian Becker
*/
int8_t platform_get_y_left_stop_angle(){
    return platform_y_left_stop_angle;
}

/*
    Get the configured value for the right stop angle of the y motor.

    @return the configured value

    @author Fabian Becker
*/
int8_t platform_get_y_right_stop_angle(){
    return platform_y_right_stop_angle;
}

/*
    Get the configured value for the starting angle of the x motor.

    @return the configured value

    @author Fabian Becker
*/
int8_t platform_get_x_start_angle(){
    return platform_x_start_angle;
}

/*
    Get the configured value for the starting angle of the y motor.

    @return the configured value

    @author Fabian Becker
*/
int8_t platform_get_y_start_angle(){
    return platform_y_start_angle;
}
