#ifndef YAE8080_H
#define YAE8080_H

#include <stdint.h>

//yae8080 constants::
#define mem_bytes 65536

//regs & sizes::
#define amount_regs 8
#define amount_IO   256
#define vram_size   7168

union YAE8080_ConditionCodes{
  unsigned z:1, s:1, p:1, cy:1, ac:1, pad:3;
};

class YAE8080_State{
public:
  //methods::
   YAE8080_State();
  ~YAE8080_State();

  //vars::
  uint_fast8_t    a;
  uint_fast8_t    b;
  uint_fast8_t    c;
  uint_fast8_t    d;
  uint_fast8_t    e;
  uint_fast8_t    h;
  uint_fast8_t    l;
  uint_fast16_t    SP;
  uint_fast16_t    PC;
  uint_fast8_t     *registers;
  uint_fast8_t     *memory;
  uint_fast8_t     *iodevices;
  YAE8080_ConditionCodes* cc;
  uint_fast8_t     int_enable;
  uint_fast8_t     running;
};

class YAE8080_ShiftRegister{
public:
  uint_fast16_t data;
  uint_fast8_t offset;
};

class YAE8080_IODevices{
public:
  YAE8080_ShiftRegister* shiftreg;
};

class YAE8080{
public:
  //METHODS:
   YAE8080(YAE8080_IODevices* io_devices);
  ~YAE8080();
  void loadIntoMemory(uint_fast16_t offset, uint_fast8_t* data, int length);
  void stepEmulator();
  void fireInterrupt(int interrupt_num);
  //vars
  YAE8080_State*     state;
  YAE8080_IODevices* devices;
private:
  void writeMemory(uint_fast16_t address, uint_fast8_t data);
  void writeIO(uint_fast8_t address, uint_fast8_t data);
  uint_fast8_t readIO (uint_fast8_t address);
};
#endif
