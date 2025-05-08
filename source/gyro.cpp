#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cmath>

#define MPU6050_ADDR 0x68 // I2C address of the MPU6050
#define MPU6050_REG_PWR_MGMT_1 0x6B

#define MPU6050_AXOFFSET 158
#define MPU6050_AYOFFSET 9
#define MPU6050_AZOFFSET -91
#define MPU6050_GXOFFSET 19
#define MPU6050_GYOFFSET -42
#define MPU6050_GZOFFSET -26

void complementary_filter(float &angle, float accel_angle, float gyro_rate, float dt, float alpha = 0.98) {
    angle = alpha * (angle + gyro_rate * dt) + (1 - alpha) * accel_angle;
}

uint8_t get_gyro_scaling(int &file){
    uint8_t MPU6050_REG_GYRO_CONFIG = 0x1B;
    write(file, &MPU6050_REG_GYRO_CONFIG, 1); // Set gyro to ±250 degrees/s
    uint8_t scaling;
    read(file, &scaling, 1); // Read the scaling value

    uint8_t fs_sel = (scaling >> 3) & 0x03; // Get the FS_SEL bits
    switch (fs_sel) {
        case 0: return 131; // ±250 degrees/s
        case 1: return 65.5; // ±500 degrees/s
        case 2: return 32.8; // ±1000 degrees/s
        case 3: return 16.4; // ±2000 degrees/s
        default: return 0; // Invalid scaling
    }
}

uint16_t get_accel_scaling(int &file){
    uint8_t MPU6050_REG_ACCEL_CONFIG = 0x1C;
    write(file, &MPU6050_REG_ACCEL_CONFIG, 1); // Set accel to ±16384 degrees/s
    uint8_t scaling;
    read(file, &scaling, 1); // Read the scaling value

    uint8_t fs_sel = (scaling >> 3) & 0x03; // Get the FS_SEL bits
    switch (fs_sel) {
        case 0: return 16384; // ±2 g
        case 1: return 8192; // ±4 g
        case 2: return 4096; // ±8 g
        case 3: return 2048; // ±16 g
        default: return 0; // Invalid scaling
    }
}

uint8_t convert_data(int& file, int8_t data[14], int16_t scaled_data[6]){
    // Convert the data to appropriate values
    scaled_data[0] = (data[0] << 8) | data[1]; // X-axis accelerometer data
    scaled_data[1] = (data[2] << 8) | data[3]; // Y-axis accelerometer data
    scaled_data[2] = (data[4] << 8) | data[5]; // Z-axis accelerometer data

    // skip temperature data

    // Convert gyroscope data
    scaled_data[3] = (data[8] << 8) | data[9]; // X-axis gyroscope data
    scaled_data[4] = (data[10] << 8) | data[11]; // Y-axis gyroscope data
    scaled_data[5] = (data[12] << 8) | data[13]; // Z-axis gyroscope data

    return 0;
}

bool perform_self_test(int file){
    // Self test registers
    const uint8_t SELF_TEST_X = 0x0D;
    const uint8_t SELF_TEST_Y = 0x0E;
    const uint8_t SELF_TEST_Z = 0x0F;
    const uint8_t SELF_TEST_A = 0x10;
    
    // Read self-test register
    
    char config[2] = {0x1B, 0xE0};
    if (write(file, config, 2) != 2) {
        std::cerr << "Failed to enable gyro self test" << std::endl;
        return false;
    }
    
    char config2[2] = {0x1c, 0xE0};
    if (write(file, config2, 2) != 2) {
        std::cerr << "Failed to enable accel self test" << std::endl;
        return false;
    }
    
    uint8_t self_test[4];
    write(file, &SELF_TEST_X, 1);
    if (read(file, self_test, 4) != 4) {
        std::cerr << "Failed to read selt test registers" << std::endl;
        return false;
    }
    
    return true;
}

int main(){
    int file;
    const char *i2c_device = "/dev/i2c-1"; // I2C device file

    if ((file = open(i2c_device, O_RDWR)) < 0) {
        std::cerr << "Failed to open the i2c bus" << std::endl;
        return -1;
    }
    if (ioctl(file, I2C_SLAVE, MPU6050_ADDR) < 0) {
        std::cerr << "Failed to acquire bus access and/or talk to slave" << std::endl;
        close(file);
        return -1;
    }

    char config[2] = {MPU6050_REG_PWR_MGMT_1, 0x00}; // Set gyro to ±250 degrees/s
    if (write(file, config, 2) != 2) {
        std::cerr << "Failed to set gyro configuration" << std::endl;
        close(file);
        return -1;
    }
    if(!perform_self_test(file)){
        std::cout << "HERE" << std::endl;
        return -1;
    }
    float pitch = 0.0, roll = 0.0; // Orientation angles
    const float dt = 0.1; // Time step in seconds (100ms)
    float angle_pitch = 0; float angle_roll = 0;

    uint8_t MPU6050_REG_ACCEL_XOUT_H = 0x3B;
    while(1){
        // Read accelerometer and gyroscope data
        write(file, &MPU6050_REG_ACCEL_XOUT_H, 1);
        int8_t raw_data[14]; // 14 bytes of data (6 for accelerometer, 2 for temperature, 6 for gyroscope)
        int16_t scaled_data[6];
        if (read(file, raw_data, 14) != 14) {
            std::cerr << "Failed to read data" << std::endl;
            close(file);
            return -1;
        }
        convert_data(file,raw_data,scaled_data);
        std::cout << "Accelerometer: X: " << scaled_data[0] << ", Y: " << scaled_data[1] << ", Z: " << scaled_data[2] << std::endl;
        std::cout << "Gyroscope: X: " << scaled_data[3] << ", Y: " << scaled_data[4] << ", Z: " << scaled_data[5] << std::endl;
        
       
        float GAcX = (float) scaled_data[0] / 4096.0;
        float GAcY = (float) scaled_data[1] / 4096.0;
        float GAcZ = (float) scaled_data[2] / 4096.0;
       
        float accel_pitch = atan((GAcY - (float)MPU6050_AYOFFSET/4096.0) / sqrt(GAcX * GAcX + GAcZ * GAcZ)) * (180 / M_PI);
        float accel_roll = -atan((GAcX - (float) MPU6050_AXOFFSET/4096.0) / sqrt(GAcY*GAcY + GAcZ * GAcZ)) * (180 / M_PI);
        float Cal_GyX = 0, Cal_GyY = 0, Cal_GyZ = 0;
        /*Cal_GyX += (float)(scaled_data[3] - MPU6050_GXOFFSET) * 0.000244140625;
        Cal_GyY += (float)(scaled_data[4] - MPU6050_GYOFFSET) * 0.000244140625;
        Cal_GyZ += (float)(scaled_data[5] - MPU6050_GZOFFSET) * 0.000244140625;*/
       
        angle_pitch = 0.98 * (((float)(scaled_data[3] - MPU6050_GXOFFSET) * 0.000244140625) + angle_pitch) + 0.02 * accel_pitch;
        angle_roll = 0.98 * (((float)(scaled_data[4] - MPU6050_GXOFFSET) * 0.000244140625) + angle_roll) + 0.02 * accel_roll;
       
       
        std::cout << "Pitch: " << angle_pitch << "°, Roll: " << angle_roll << "°" << std::endl;
       usleep(4000); // Sleep for 4ms

    }
    close(file); // Close the I2C device file
    return 0;
}
