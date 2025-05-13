class Kalman
{
public:
    Kalman();

    float update(float newAngle, float newRate, float dt);

private:
    float Q_angle;
    float Q_bias;
    float R_measure;

    float angle;
    float bias;
    float rate;
    float P[2][2];
};
