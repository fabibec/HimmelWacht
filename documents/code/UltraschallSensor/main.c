
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