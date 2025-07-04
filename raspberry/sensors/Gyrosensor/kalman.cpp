// used source: https://github.com/jarzebski/Arduino-KalmanFilter
// @Author : Nicolas Koch

#include "kalman.h"

Kalman::Kalman()
{
    Q_angle = 0.01f;  // Trust in gyro
    Q_bias  = 0.03f;  // Trust in bias estimate
    R_measure = 0.01f; // Trust in accelerometer

    
    angle = 0.0f;
    bias = 0.0f;
    
    P[0][0] = 1.0f;  // Initial values for error covariance
    P[0][1] = 0.0f;
    P[1][0] = 0.0f;
    P[1][1] = 1.0f;
}

float Kalman::update(float newAngle, float newRate, float dt)
{
    // Prediction step
    rate = newRate - bias;
    angle += dt * rate;

    // Update estimation error covariance
    P[0][0] += dt * (dt * P[1][1] - P[0][1] - P[1][0] + Q_angle);
    P[0][1] -= dt * P[1][1];
    P[1][0] -= dt * P[1][1];
    P[1][1] += Q_bias * dt;

    // Innovation
    float S = P[0][0] + R_measure;
    float K[2]; // Kalman gain
    K[0] = P[0][0] / S;
    K[1] = P[1][0] / S;

    float y = newAngle - angle;
    angle += K[0] * y;
    bias += K[1] * y;

    // Update error covariance
    float P00_temp = P[0][0];
    float P01_temp = P[0][1];

    P[0][0] -= K[0] * P00_temp;
    P[0][1] -= K[0] * P01_temp;
    P[1][0] -= K[1] * P00_temp;
    P[1][1] -= K[1] * P01_temp;

    return angle;
}