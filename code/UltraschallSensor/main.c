#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

int distMessung(){

    uint8_t dist = 0;
    
    // Messung
    REG0 = 0x51;
    delay(65);
    dist = (int)(REG2 + 256) + REG3;

    return dist;
}

int main(){

    return 0;
}