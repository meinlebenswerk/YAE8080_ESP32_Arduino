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
  uint8_t    z:1;
  uint8_t    s:1;
  uint8_t    p:1;
  uint8_t    cy:1;
  uint8_t    ac:1;
  uint8_t    pad:3;
};

class YAE8080_State{
public:
  //methods::
   YAE8080_State();
  ~YAE8080_State();

  //vars::
  uint8_t    a;
  uint8_t    b;
  uint8_t    c;
  uint8_t    d;
  uint8_t    e;
  uint8_t    h;
  uint8_t    l;
  uint16_t    SP;
  uint16_t    PC;
  uint8_t     *registers;
  uint8_t     *memory;
  uint8_t     *iodevices;
  YAE8080_ConditionCodes* cc;
  uint8_t     int_enable;
  uint8_t     running;
};

class YAE8080_ShiftRegister{
public:
  uint16_t data;
  uint8_t offset;
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
  void loadIntoMemory(uint16_t offset, uint8_t* data, int length);
  void stepEmulator();
  void fireInterrupt(int interrupt_num);
  //vars
  YAE8080_State*     state;
  YAE8080_IODevices* devices;
private:
  void writeMemory(uint16_t address, uint8_t data);
  void writeIO(uint8_t address, uint8_t data);
  uint8_t readIO (uint8_t address);
};
#endif
