#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cmath>
#include <sys/time.h>
#include "kalman.h"

#define MPU6050_ADDR 0x68 // I2C address of the MPU6050
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define OFFSET_SAMPLES 1000
	
float get_dt(struct timeval &prev_time)
{
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    float dt = (current_time.tv_sec - prev_time.tv_sec) +
               (current_time.tv_usec - prev_time.tv_usec) / 1000000.0f;
    prev_time = current_time;
    return dt;
}

uint8_t get_gyro_scaling(int &file)
{
    uint8_t MPU6050_REG_GYRO_CONFIG = 0x1B;
    write(file, &MPU6050_REG_GYRO_CONFIG, 1); // Set gyro to ±250 degrees/s
    uint8_t scaling;
    read(file, &scaling, 1); // Read the scaling value

    uint8_t fs_sel = (scaling >> 3) & 0x03; // Get the FS_SEL bits
    switch (fs_sel)
    {
    case 0:
        return 131; // ±250 degrees/s
    case 1:
        return 65.5; // ±500 degrees/s
    case 2:
        return 32.8; // ±1000 degrees/s
    case 3:
        return 16.4; // ±2000 degrees/s
    default:
        return 0; // Invalid scaling
    }
}

uint16_t get_accel_scaling(int &file)
{
    uint8_t MPU6050_REG_ACCEL_CONFIG = 0x1C;
    write(file, &MPU6050_REG_ACCEL_CONFIG, 1); // Set accel to ±16384 degrees/s
    uint8_t scaling;
    read(file, &scaling, 1); // Read the scaling value

    uint8_t fs_sel = (scaling >> 3) & 0x03; // Get the FS_SEL bits
    switch (fs_sel)
    {
    case 0:
        return 16384; // ±2 g
    case 1:
        return 8192; // ±4 g
    case 2:
        return 4096; // ±8 g
    case 3:
        return 2048; // ±16 g
    default:
        return 0; // Invalid scaling
    }
}

uint8_t get_raw_data(int16_t &a_x,int16_t &a_y,int16_t &a_z,int16_t &g_x,int16_t &g_y,int16_t &g_z, int8_t data[14])
{
    // Convert the data to appropriate values
    a_x = (data[0] << 8) | data[1]; // X-axis accelerometer data
    a_y = (data[2] << 8) | data[3]; // Y-axis accelerometer data
    a_z = (data[4] << 8) | data[5]; // Z-axis accelerometer data

    // skip temperature data

    // Convert gyroscope data
    g_x = (data[8] << 8) | data[9];   // X-axis gyroscope data
    g_y = (data[10] << 8) | data[11]; // Y-axis gyroscope data
    g_z = (data[12] << 8) | data[13]; // Z-axis gyroscope data

    return 0;
}

bool perform_self_test(int file)
{
    // Self test registers
    const uint8_t SELF_TEST_X = 0x0D;
    const uint8_t SELF_TEST_Y = 0x0E;
    const uint8_t SELF_TEST_Z = 0x0F;
    const uint8_t SELF_TEST_A = 0x10;

    // Read self-test register

    if (ioctl(file, I2C_SLAVE, MPU6050_ADDR) < 0)
    {
        std::cerr << "Failed to acquire bus access and/or talk to slave" << std::endl;
        close(file);
        return -1;
    }

    char pwr_mgmt_config[2] = {MPU6050_REG_PWR_MGMT_1, 0x00}; // Set gyro to ±250 degrees/s
    if (write(file, pwr_mgmt_config, 2) != 2)
    {
        std::cerr << "Failed to set gyro configuration" << std::endl;
        close(file);
        return -1;
    }
    char gyro_config[2] = {0x1B, 0xE0};
    if (write(file, gyro_config, 2) != 2)
    {
        std::cerr << "Failed to enable gyro self test" << std::endl;
        return false;
    }

    char accel_config[2] = {0x1c, 0xE0};
    if (write(file, accel_config, 2) != 2)
    {
        std::cerr << "Failed to enable accel self test" << std::endl;
        return false;
    }

    uint8_t self_test[4];
    write(file, &SELF_TEST_X, 1);
    if (read(file, self_test, 4) != 4)
    {
        std::cerr << "Failed to read selt test registers" << std::endl;
        return false;
    }

    return true;
}

uint8_t get_offsets(int16_t &MPU6050_AXOFFSET, int16_t &MPU6050_AYOFFSET, int16_t &MPU6050_AZOFFSET,
                    int16_t &MPU6050_GXOFFSET, int16_t &MPU6050_GYOFFSET, int16_t &MPU6050_GZOFFSET,
                    int16_t samples, int& file)
{
    int16_t a_x, a_y, a_z, g_x, g_y, g_z;
    int8_t data[14];
	uint8_t MPU6050_REG_ACCEL_XOUT_H = 0x3B;
    // Calculate the offsets to zero the sensor
    for (int i = 0; i < samples; i++)
    {
		write(file, &MPU6050_REG_ACCEL_XOUT_H, 1);
		if (read(file, data, 14) != 14)
        {
            std::cerr << "Failed to read data" << std::endl;
            continue;
        }
        
        get_raw_data(a_x, a_y, a_z, g_x, g_y, g_z, data);
        MPU6050_AXOFFSET += a_x;
        MPU6050_AYOFFSET += a_y;
        MPU6050_AZOFFSET += a_z;
        MPU6050_GXOFFSET += g_x;
        MPU6050_GYOFFSET += g_y;
        MPU6050_GZOFFSET += g_z;
        usleep(5000); // Sleep for 4ms
    }
    MPU6050_AXOFFSET /= samples;
    MPU6050_AYOFFSET /= samples;
    MPU6050_AZOFFSET /= samples;
    MPU6050_GXOFFSET /= samples;
    MPU6050_GYOFFSET /= samples;
    MPU6050_GZOFFSET /= samples;
    
    //std::cout << "SET OFFSETS!" << std::endl;

    return 1;
}

int main()
{
    int file;
    const char *i2c_device = "/dev/i2c-1"; // I2C device file
    int8_t raw_data[14];

    if ((file = open(i2c_device, O_RDWR)) < 0)
    {
        std::cerr << "Failed to open the i2c bus" << std::endl;
        return -1;
    }

    if (!perform_self_test(file))
    {
        return -1;
    }

    int16_t MPU6050_AXOFFSET = 0, MPU6050_AYOFFSET = 0, MPU6050_AZOFFSET = 0,
            MPU6050_GXOFFSET = 0, MPU6050_GYOFFSET = 0, MPU6050_GZOFFSET = 0;

    if (!get_offsets(MPU6050_AXOFFSET, MPU6050_AYOFFSET, MPU6050_AZOFFSET,
                     MPU6050_GXOFFSET, MPU6050_GYOFFSET, MPU6050_GZOFFSET, OFFSET_SAMPLES, file))
    {
        std::cerr << "Failed to set offsets" << std::endl;
        close(file);
        return -1;
    }

    Kalman kalman_pitch;
    Kalman kalman_roll;
    uint8_t MPU6050_REG_ACCEL_XOUT_H = 0x3B;
    struct timeval prev_time;
    gettimeofday(&prev_time, NULL);
		
    uint16_t accel_scaling = get_accel_scaling(file);


    while (1)
    {

        // Read accelerometer and gyroscope data
        
        int16_t a_x, a_y, a_z, g_x, g_y, g_z;
		write(file, &MPU6050_REG_ACCEL_XOUT_H, 1);
        if (read(file, raw_data, 14) != 14)
        {
            std::cerr << "Failed to read data" << std::endl;
            continue;
        }
        std::cout << "Raw bytes: ";
		for (int i = 0; i < 14; ++i)
			printf("%02X ", raw_data[i]);
		std::cout << std::endl;
        get_raw_data(a_x, a_y, a_z, g_x, g_y, g_z, raw_data);
        //std::cout << "a_x: " << a_x << "a_y: " << a_y << "a_z: " << a_z << "g_x: " << g_x << "g_y: " << g_y << "g_z: " << g_z << std::endl;
        /*std::cout << "Accelerometer: X: " << a_x << ", Y: " << a_y << ", Z: " << a_z << std::endl;
        std::cout << "Gyroscope: X: " << g_x << ", Y: " << g_y << ", Z: " << g_z << std::endl;*/
		
		float GAcX = (float)(a_x - MPU6050_AXOFFSET) / (float)accel_scaling;
		float GAcY = (float)(a_y - MPU6050_AYOFFSET) / (float)accel_scaling;
		float GAcZ = (float)(a_z - MPU6050_AZOFFSET) / (float)accel_scaling;


        float accel_pitch = atan2(GAcY, sqrt(GAcX * GAcX + GAcZ * GAcZ)) * (180.0 / M_PI);
        float accel_roll = atan2(GAcX, sqrt(GAcY * GAcY + GAcZ * GAcZ)) * (180.0 / M_PI);
/*
		float accel_pitch = atan2(-GAcX, sqrt(GAcY*GAcY + GAcZ*GAcZ)) * (180.0 / M_PI);
		float accel_roll  = atan2(GAcY, GAcZ) * (180.0 / M_PI);
*/
        float gyroX_rate = (float)(g_x - MPU6050_GXOFFSET) / 131.0f;
        float gyroY_rate = (float)(g_y - MPU6050_GYOFFSET) / 131.0f;
        
        

        float dt = get_dt(prev_time);

        float angle_pitch = kalman_pitch.update(accel_pitch, gyroX_rate, dt);
        float angle_roll = kalman_roll.update(accel_roll, gyroY_rate, dt);

        // std::cout << "Pitch: " << angle_pitch << "°, Roll: " << angle_roll << "°" << std::endl;
        //std::cout << angle_pitch << ", " << angle_roll << std::endl;
        usleep(5000); // Sleep for 4ms
    }
    close(file); // Close the I2C device file
    return 0;
}
