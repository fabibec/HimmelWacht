#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

int distMessung(){

    uint8_t dist = 0;
    
    // Write 0x51 to register 0
    buffer[0] = 0;      // Register 0
    buffer[1] = 0x51;   // Value for Start Measurement
    if (write(file, buffer, 2) != 2) {
        perror("Failed to write to the I2C bus");
    }
    
    delay(65);

    // Set register 2
    buffer[0] = 2; // Register 2
    if (write(file, buffer, 1) != 1) {
        perror("Failed to set register for reading");
    }

    // Read 2 bytes from register 2 and 3
    if (read(file, buffer, 2) != 2) {
        perror("Failed to read from the I2C bus");
    } else {
        dist = (int)(buffer[0] << 8) | buffer[1];
    }

    close(file);
    return dist;
}

int main(){

    // Copied code
    int file;
    char *filename = "/dev/i2c-1";
    uint8_t buffer[2];

    // Open the I2C device
    if ((file = open(filename, O_RDWR)) < 0) {
        perror("Failed to open the I2C bus");
        return -1;
    }

    // Specify the I2C address of the device
    int addr = 0xE0; // I2C address of the device
    if (ioctl(file, I2C_SLAVE, addr) < 0) {
        perror("Failed to acquire bus access and/or talk to slave. \n");
        close(file);
        return -1;
    }
    // End of copied code

    return 0;
}