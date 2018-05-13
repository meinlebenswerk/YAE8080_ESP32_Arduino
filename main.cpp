#include <stdio.h>
#include <stdlib.h>
#include "lib/libyae8080/yae8080.h"

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

int main (int argc, char**argv){
  YAE8080_IODevices devices;
  YAE8080* yae = new YAE8080(&devices);

  ReadFileIntoMemoryAt(yae, "invaders.h", 0);
	ReadFileIntoMemoryAt(yae, "invaders.g", 0x800);
	ReadFileIntoMemoryAt(yae, "invaders.f", 0x1000);
	ReadFileIntoMemoryAt(yae, "invaders.e", 0x1800);

  // ReadFileIntoMemoryAt(&state , "./progs/test.bin", 0x0100);
  // // ReadFileIntoMemoryAt(&state_alt, "./progs/test.bin", 0x0100);
  // // //Fix the first instruction to be JMP 0x100
  // state.memory[0]=0xc3;
  // state.memory[1]=0;
  // state.memory[2]=0x01;
  //
  // //Fix the stack pointer from 0x6ad to 0x7ad
  //   // this 0x06 byte 112 in the code, which is
  //   // byte 112 + 0x100 = 368 in memory
  // state.memory[368] = 0x7;
  //
  // //Skip DAA test
  // state.memory[0x59c] = 0xc3; //JMP
  // state.memory[0x59d] = 0xc2;
  // state.memory[0x59e] = 0x05;
  //
  // //SAME
  // state_alt.memory[0]=0xc3;
  // state_alt.memory[1]=0;
  // state_alt.memory[2]=0x01;
  // state_alt.memory[368] = 0x7;
  // state_alt.memory[0x59c] = 0xc3; //JMP
  // state_alt.memory[0x59d] = 0xc2;
  // state_alt.memory[0x59e] = 0x05;
  while(yae->state->running){
    for(int i=0;i<1000;i++){
      yae->stepEmulator();
    }
		yae->state->running = 0;
  }
  free(yae);
  printf("%s \n","Exiting YAE8080");
  return 0;
}
