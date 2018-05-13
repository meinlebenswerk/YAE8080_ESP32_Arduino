#include <Arduino.h>
#include "invaders.h"
#include "yae8080.h"
YAE8080_IODevices devices;
YAE8080* yae;


void setup() {
    Serial.begin(115200);
    yae = new YAE8080(&devices);

    yae->loadIntoMemory(0x0000, (uint_fast8_t*)&invaders_h[0], invaders_h_size);
    yae->loadIntoMemory(0x0800, (uint_fast8_t*)&invaders_g[0], invaders_g_size);
    yae->loadIntoMemory(0x1000, (uint_fast8_t*)&invaders_f[0], invaders_f_size);
    yae->loadIntoMemory(0x1800, (uint_fast8_t*)&invaders_e[0], invaders_e_size);
}

void loop() {
    long tin = micros();

    for(int i=0;i<100000;i++){
      yae->stepEmulator();
    }

    long tout = micros();
    double cycletime = ((double)tout-tin)/100000.0;
    double maxfreq   = 1000000/cycletime; //cycles per second
    double maxfreq_mhz = maxfreq/1000000;

    Serial.printf("cycletime: [%1.6lf]us, -> max emulation frequency: [%2.4lf] MHz\n",cycletime,maxfreq_mhz);
    Serial.printf("Free Heap:: [%i]bytes\n",ESP.getFreeHeap());
}