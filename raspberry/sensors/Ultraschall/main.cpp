/**
 * Code for accessing the ultrasonic sensor
 *
 * @author Jonathan Wohlrab
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include "../i2c_bus.h"

#define SRF02_ADDR 0x71  // Adjust address if needed

uint16_t distMessung(int file) {
    uint16_t dist = 0;
    uint8_t buffer[2] = {0};

    // Start measurement command (0x51) to register 0
    buffer[0] = 0;
    buffer[1] = 0x51;
    if (write(file, buffer, 2) != 2) {
        perror("Starten der Messung fehlgeschlagen");
        return (uint16_t)-1;
    }

    usleep(65000);  // wait for measurement

    // Set register 2 to read distance
    buffer[0] = 2;
    if (write(file, buffer, 1) != 1) {
        perror("Setzen des Registers zur Rückgabe des Ergebnisses fehlgeschlagen");
        return (uint16_t)-1;
    }

    // Read 2 bytes from sensor
    if (read(file, buffer, 2) != 2) {
        perror("Fehler beim Lesen des Ergebnisses");
        return (uint16_t)-1;
    } else if (buffer[0] == 255 && buffer[1] == 255) {
        fprintf(stderr, "Wert außerhalb des Messbereichs\n");
        return (uint16_t)-1;
    } else {
        dist = (uint16_t)(buffer[0] << 8) | buffer[1];
    }

    return dist;
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);  // Disable buffering

    int file = open("/dev/i2c-1", O_RDWR);;
    if (file < 0) {
        // Error already printed by get_i2c_fd()
        return -1;
    }

    if (ioctl(file, I2C_SLAVE, SRF02_ADDR) < 0) {
        perror("Herstellen der Verbindung zum Ultraschallsensor fehlgeschlagen");
        close_i2c_fd();
        return -1;
    }

    while (1) {
        uint16_t dist = distMessung(file);
        if (dist != (uint16_t)-1) {
            printf("%d cm\n", dist);
            fflush(stdout);
        } else {
            fprintf(stderr, "Fehlerhafte Messung, versuche erneut.\n");
        }
        usleep(100000);  // 100ms delay between measurements
    }

    close(file);

    return 0;
}
