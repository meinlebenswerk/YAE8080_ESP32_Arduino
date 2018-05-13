#include <Arduino.h>
#include "invaders.h"
#include "yae8080.h"
YAE8080_IODevices devices;
YAE8080* yae;
void ReadFileIntoMemoryAt(YAE8080* yae, const char* filename, uint32_t offset){
  //not really nice :)
	FILE *f= fopen(filename, "rb");
	if (f==NULL)
	{
		printf("error: Couldn't open %s\n", filename);
		exit(1);
	}
	fseek(f, 0L, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0L, SEEK_SET);

	uint8_t *buffer = (uint8_t*) malloc(fsize);
	fread(buffer, fsize, 1, f);
  yae->loadIntoMemory(offset, buffer, fsize);
	fclose(f);
  free(buffer);
}


void setup() {
    Serial.begin(115200);
    yae = new YAE8080(&devices);

    yae->loadIntoMemory(0x0000, (uint8_t*)&invaders_h[0], invaders_h_size);
    yae->loadIntoMemory(0x0800, (uint8_t*)&invaders_g[0], invaders_g_size);
    yae->loadIntoMemory(0x1000, (uint8_t*)&invaders_f[0], invaders_f_size);
    yae->loadIntoMemory(0x1800, (uint8_t*)&invaders_e[0], invaders_e_size);

    long tin = micros();

    for(int i=0;i<10000;i++){
      yae->stepEmulator();
    }

    long tout = micros();
    double cycletime = ((double)tout-tin)/10000.0;
    double maxfreq   = 1000000/cycletime; //cycles per second
    double maxfreq_khz = maxfreq/1000;
    free(yae);
    Serial.printf("cycletime: [%lf]us, -> max emulation frequency: [%lf] KHz",cycletime,maxfreq_khz);
}

void loop() {
    // put your main code here, to run repeatedly:
}