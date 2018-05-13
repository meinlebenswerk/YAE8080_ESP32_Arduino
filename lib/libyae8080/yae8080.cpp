#include <stdlib.h>
#include <string.h> // memcpy
#include "yae8080.h"

YAE8080_State::YAE8080_State(){
  //Initialize Registers::
  a = 0;
  b = 0;
  c = 0;
  d = 0;
  e = 0;
  h = 0;
  l = 0;

  SP = 0;
  PC = 0;

  registers = (uint8_t*) malloc(amount_regs);
  memory    = (uint8_t*) malloc(mem_bytes);
  iodevices = (uint8_t*) malloc(amount_IO);

  cc = new YAE8080_ConditionCodes();
  cc->ac = 0;
  cc->cy = 0;
  cc->z  = 0;
  cc->s  = 0;

  int_enable = 0; //????? TODO"!
  running = 1;
}

uint8_t parity(uint32_t ino) {
  uint8_t noofones = 0;
  while(ino != 0) {
    noofones++;
    ino &= (ino-1); // the loop will execute once for each bit of ino set
  }
  /* if noofones is odd, least significant bit will be 1 */
  return !(noofones & 1);
}


//YAE8080::
YAE8080::YAE8080(YAE8080_IODevices* io_devices){
  state = new YAE8080_State(); // this does what init previously did!

  devices = io_devices;
}
YAE8080::~YAE8080(){
  free(state->memory);
  free(state->registers);
  free(state->iodevices);
  free(state->cc);
  free(state);
}

void YAE8080::writeMemory(uint16_t address, uint8_t data){
  if (address < 0x2000){
      //        //printf("Writing ROM not allowed %x\n", address);
      return;
  }if (address >=0x4000){
      //       //printf("Writing out of Space Invaders RAM not allowed %x\n", address);
      return;
  }
  state->memory[address] = data;
}

void YAE8080::writeIO(uint8_t address, uint8_t data){
  //This gets only called if an i/O op Happens :)
  uint32_t imd0, imd1;
  switch(address){
    case 0x02:
      //ShiftREG
      //Writing to port 2 (bits 0,1,2) sets the offset for the 8 bit result
      devices->shiftreg->offset = state->iodevices[2]&0x07;
    break;
    case 0x04:
      //ShiftREG
      //Writing to port 4 shifts x into y, and the new value into x, eg.
      //  ;    $0000,
      //  ;    write $aa -> $aa00,
      //  ;    write $ff -> $ffaa,
      //  ;    write $12 -> $12ff, ..
      imd0 = devices->shiftreg->data;
      devices->shiftreg->data = (data)<<(8) | (devices->shiftreg->data>>(8));
    break;
  }
}

uint8_t YAE8080::readIO(uint8_t address){
  uint32_t imd0, imd1;
  switch(address){
    case 0x03:
      imd0 = (devices->shiftreg->data & (0xF0>>devices->shiftreg->offset))>>(8-devices->shiftreg->offset);
      state->iodevices[3] = imd0&0xFF;
    break;
  }
  return state->iodevices[address];
}

void YAE8080::fireInterrupt(int interrupt_num){
  //Push PC onto the stack -> TODO: have a instruction injection sidechannel
  writeMemory(state->SP-1, state->PC>>8 &0xFF);
  writeMemory(state->SP-2, state->PC    &0xFF);
  state->SP = state->SP - 2;

  state->PC = 8 * interrupt_num;
}

void YAE8080::loadIntoMemory(uint16_t offset,uint8_t* data, int length){
  memcpy(&state->memory[offset], data, length);
}

void YAE8080::stepEmulator(){
  //get Instruction
  uint8_t op = state->memory[state->PC];
  uint8_t *opcode = (state->memory+state->PC);
  uint8_t opbytes = 1;

  //NOP
  if(op == 0x00){
    state->PC += opbytes;
    return;
  };

  //MOV-tmp vars::
  uint8_t dest, src;

  //Math intermediate Variables
  uint32_t imd0, imd1;

  //FUCK FUCK FUCK TODO -> check if dual thigies want to address the stackpointer!!!!!!!!!!
  //This should be fixed :)
  uint8_t tmp = (op>>6);
  switch(tmp){
    //SIZE:OK
    //INS:OK
    case 0:
      //this could be a lot -> sorting is done via last 3 bits :)
      tmp = op & 0x07;
      switch(tmp){
        //INS:OK
        case 0x01:
          dest = (op>>4) & 0x03;
          imd0 = (opcode[2] << 8) | opcode[1];
          if((op>>3)&0x01){
            //printf("%s 0x0%x\n", "DAD", dest);
            //double_add from RP to HL
            //first get HL
            imd0  = (state->registers[0x04]<<8) | state->registers[0x05];

            if(dest == 0x03){
              imd1 = state->SP;
            }else{
              dest *= 2;
              imd1 =  (state->registers[dest]<<8) | state->registers[dest+1];
            }

            imd0 += imd1;
            //set STATUS register, and HL regs
            state->cc->cy = (imd0>>16);
            state->registers[0x04] = (imd0>>8) &0xFF;
            state->registers[0x05] = (imd0)    &0xFF;
          }else{
            imd0 = (opcode[2]<<8) | opcode[1];
            //printf("%s 0x0%x 0x%x \n", "LXI", dest,imd0);
            opbytes = 3;
            if(dest == 0x03){
              state->SP  = imd0 & 0xFFFF;
            }else{
              dest *= 2;
              state->registers[dest]   = opcode[2];
              state->registers[dest+1] = opcode[1];
            }

          }
        break;
        //INS:OK
        case 0x02:
          tmp = (op >> 3) & 0x07;
          switch(tmp){
            //INS:OK
            case 0x04:
              //printf("%s\n", "SHLD");
              opbytes = 3;
              imd0 = (state->memory[state->PC+2]<<8)&state->memory[state->PC+1];
              state->memory[imd0]   = state->registers[0x05];
              state->memory[imd0+1] = state->registers[0x04];
            break;
            //INS:OK
            case 0x05:
              //printf("%s\n", "LHLD");
              opbytes = 3;
              imd0 = (state->memory[state->PC+2]<<8)&state->memory[state->PC+1];
              state->registers[0x05] = state->memory[imd0];
              state->registers[0x04] = state->memory[imd0+1];
            break;
            //INS:OK
            case 0x06:
              //printf("%s\n", "STA");
              opbytes = 3;
              imd0 = (state->memory[state->PC+2]<<8) | state->memory[state->PC+1];
              state->memory[imd0] = state->registers[0x07];
            break;
            //INS:OK
            case 0x07:
              //printf("%s\n", "LDA");
              opbytes = 3;
              imd0 = (state->memory[state->PC+2]<<8) | state->memory[state->PC+1];
              state->registers[0x07] = state->memory[imd0];
            break;
            //INS:OK
            default:
              dest = (op>>4) & 0x01;
              imd0 = (state->registers[dest*2]<<8) | (state->registers[(dest*2)+1]);
              if((op>>3)&0x01){
                //printf("%s 0x%x 0x%x\n", "LDAX", dest, imd0);
                state->registers[0x07] = state->memory[imd0];
              }else{
                //printf("%s 0x0%x\n", "STAX", dest);
                state->memory[imd0] = state->registers[0x07];
              }
            break;
          }
        break;
        //INS:OK
        case 0x03:
          dest = (op>>4) & 0x03;
          dest *= 2;
          imd0 = state->registers[dest]<<8 | state->registers[dest+1];
          if(dest==0x03){
            imd0 = state->SP;
          }

          if((op>>3)&0x01){
            //printf("%s 0x0%x\n", "DCX", dest);
            imd0--;
          }else{
            //printf("%s 0x0%x\n", "INX", dest);
            imd0++;
          }
          //set regs
          if(dest == 0x03){
            state->SP  = (imd0>>8) &0xFF;
            state->SP  = (imd0)    &0xFF;
          }else{
            state->registers[dest]   = (imd0>>8) &0xFF;
            state->registers[dest+1] = (imd0)    &0xFF;
          }
        break;
        //INS:OK
        case 0x04:
          //this can only be INR
          dest = (op>>3) & 0x07;
          //printf("%s 0x0%x\n", "INR", dest);
          if(dest == 0x06){
            imd0 = state->memory[(state->registers[0x04]<<8)&state->registers[0x05]];
            imd0++;
            state->memory[(state->registers[0x04]<<8)&state->registers[0x05]] = imd0&0xFF;
          }else{
            imd0 = state->registers[dest];
            imd0++;
            state->registers[dest] = imd0&0xFF;
          }

          state-> cc->cy = (imd0>>8)&0xFF;
          state-> cc->z  = (imd0 == 0);
          state-> cc->s  = (imd0>>7)&0x01;
          state-> cc->p  = parity(imd0);
        break;
        //INS:OK
        case 0x05:
          //this can only be DCR
          dest = (op>>3) & 0x07;
          //printf("%s 0x0%x\n", "DCR", dest);
          if(dest == 0x06){
            imd0 = state->memory[(state->registers[0x04]<<8)&state->registers[0x05]];
            imd0--;
            state->memory[(state->registers[0x04]<<8)&state->registers[0x05]] = imd0&0xFF;
          }else{
            imd0 = state->registers[dest];
            imd0--;
            state->registers[dest] = imd0&0xFF;
          }

          state-> cc->cy = (!imd0>>8)&0xFF;
          state-> cc->z  = (imd0 == 0);
          state-> cc->s  = (imd0>>7)&0x01;
          state-> cc->p  = parity(imd0);
        break;
        //INS:OK
        case 0x06:
          //this can only be MVI
          dest = (op>>3) & 0x07;
          //printf("%s 0x0%x\n", "MVI", dest);
          opbytes = 2;
          state->registers[dest] = state->memory[state->PC+1];
        break;
        //INS:OK -> DAA is strange but ok -> and needs the ac to be properly implemented //
        case 0x07:
          //these are rotate instructions
          tmp = (op >> 3) & 0x07;
          switch(tmp){
            //INS:OK
            case 0x00:
              //printf("%s\n", "RLC");
              state-> cc->cy = (state->registers[0x07]>>7)&0x01;
              state-> registers[0x07] = state->registers[0x07] << 1;
            break;
            //INS:OK
            case 0x01:
              //printf("%s\n", "RRC");
              state-> cc->cy = state->registers[0x07]&0x01;
              state-> registers[0x07] = state->registers[0x07] >> 1;
            break;
            //INS:OK
            case 0x02:
              //printf("%s\n", "RAL");
              src = state-> cc->cy;
              state-> cc->cy = (state->registers[0x07]>>7)&0x01;
              state-> registers[0x07] = state->registers[0x07] << 1;
              state-> registers[0x07] = state->registers[0x07] & src;
            break;
            //INS:OK
            case 0x03:
              //printf("%s\n", "RAR");
              src = state-> cc->cy;
              state-> cc->cy = state->registers[0x07]&0x01;
              state-> registers[0x07] = state->registers[0x07] >> 1;
              state-> registers[0x07] = state->registers[0x07] & (src<<7);
            break;
            //INS:STRANGE->not needed???, but should be OK
            case 0x04:
              //printf("%s\n", "DAA");
              imd0 = state->registers[0x07] & 0x0F;
              if(imd0>9 || state->cc->ac){
                imd0 = state->registers[0x07] + 6;
                state->cc->ac = (imd0>>4)&0x01;
                state->registers[0x07] = imd0&0xFF;
              }
              imd0 = (state->registers[0x07]>>8) & 0x0F;
              if(imd0>9 || state->cc->cy){
                imd0 = state->registers[0x07] + (6<<4);
                state->cc->cy = (imd0>>8)&0x01;
                state->registers[0x07] = imd0&0xFF;
              }
            break;
            //INS:OK
            case 0x05:
              //printf("%s\n", "CMA");
              state->registers[0x07] = !state->registers[0x07];
            break;
            //INS:OK
            case 0x06:
              //printf("%s\n", "STC");
              state->cc->cy = 1;
            break;
            //INS:OK
            case 0x07:
              //printf("%s\n", "CMC");
              state->cc->cy = !state->cc->cy;
            break;
          }
        break;
      }
    break;

    //MOV-OPs
    //SIZE:OK
    //INS:OK
    //BUT HALT!
    case 1:
      //init MOV-decode
      src  =  op     & 0x07;
      dest = (op>>3) & 0x07;
      //check for halt-op::
      if(src == dest && dest == 0x06){
        //Halt-OP
        //printf("%s\n","HLT");
        state->running = 0x00;
        break;
      }
      //printf("%s 0x0%x 0x0%x\n", "MOV", dest, src);
      if(src == 0x06){
        state->registers[dest] = state->memory[(state->registers[0x04]<<8) | state->registers[0x05]];
      }else if (dest == 0x06){
        state->memory[(state->registers[0x04]<<8) | state->registers[0x05]] = state->registers[src];
      }else{
        state->registers[dest] = state->registers[src];
      }
    break;

    //ALU-OPs
    //SIZE: OK
    //INS:OK
    case 2:
      //ALU-OPs
      //this might be ADD, ADC, SUB, SBB, ANA, ORA, XRA, CMP
      tmp = (op>>3) & 0x07;
      src = (op&0x07);
      switch(tmp){
        //INS:OK
        case 0x00:
          //printf("%s 0x0%x\n", "ADD", src);
          imd0  = state->registers[0x07];
          if(src == 0x06){
            imd0 += state->memory[(state->registers[0x04]<<8)&state->registers[0x05]];
          }else{
            imd0 += state->registers[src];
          }

          state->registers[0x07] = imd0 &0xFF;
          state-> cc->cy = (imd0>>8)&0xFF;
          state-> cc->z  = (imd0 == 0);
          state-> cc->s  = (imd0>>7)&0x01;
          state-> cc->p  = parity(imd0);
        break;
        //INS:OK
        case 0x01:
          //printf("%s 0x0%x\n", "ADC", src);
          imd0  = state->registers[0x07];
          if(src == 0x06){
            imd0 += state->memory[(state->registers[0x04]<<8)&state->registers[0x05]];
          }else{
            imd0 += state->registers[src];
          }
          imd0 += state->cc->cy;

          state->registers[0x07] = imd0 &0xFF;
          state-> cc->cy = (imd0>>8)&0xFF;
          state-> cc->z  = (imd0 == 0);
          state-> cc->s  = (imd0>>7)&0x01;
          state-> cc->p  = parity(imd0);
        break;
        //INS:OK
        case 0x02:
          //printf("%s 0x0%x\n", "SUB", src);
          imd0  = state->registers[0x07];
          if(src == 0x06){
            imd0 -= state->memory[(state->registers[0x04]<<8)&state->registers[0x05]];
          }else{
            imd0 -= state->registers[src];
          }

          state->registers[0x07] = imd0 &0xFF;
          state-> cc->cy = (!imd0>>8)&0xFF;
          state-> cc->z  = (imd0 == 0);
          state-> cc->s  = (imd0>>7)&0x01;
          state-> cc->p  = parity(imd0);
        break;
        //INS:OK
        case 0x03:
          //printf("%s 0x0%x\n", "SBB", src);
          imd0  = state->registers[0x07];
          if(src == 0x06){
            imd0 -= state->memory[(state->registers[0x04]<<8)&state->registers[0x05]];
          }else{
            imd0 -= state->registers[src];
          }

          imd0 -= state->cc->cy;

          state->registers[0x07] = imd0 &0xFF;
          state-> cc->cy = (!imd0>>8)&0xFF;
          state-> cc->z  = (imd0 == 0);
          state-> cc->s  = (imd0>>7)&0x01;
          state-> cc->p  = parity(imd0);
        break;
        //INS:OK
        case 0x04:
          //printf("%s 0x0%x\n", "ANA", src);
          imd0  = state->registers[0x07];
          if(src == 0x06){
            imd0 &= state->memory[(state->registers[0x04]<<8)&state->registers[0x05]];
          }else{
            imd0 &= state->registers[src];
          }

          state->registers[0x07] = imd0 &0xFF;
          state-> cc->cy = (imd0>>8)&0xFF;
          state-> cc->z  = (imd0 == 0);
          state-> cc->s  = (imd0>>7)&0x01;
          state-> cc->p  = parity(imd0);
        break;
        //INS:OK
        case 0x05:
          //printf("%s 0x0%x\n", "XRA", src);
          imd0  = state->registers[0x07];
          if(src == 0x06){
            imd0 ^= state->memory[(state->registers[0x04]<<8)&state->registers[0x05]];
          }else{
            imd0 ^= state->registers[src];
          }

          state->registers[0x07] = imd0 &0xFF;
          state-> cc->cy = (imd0>>8)&0xFF;
          state-> cc->z  = (imd0 == 0);
          state-> cc->s  = (imd0>>7)&0x01;
          state-> cc->p  = parity(imd0);
        break;
        //INS:OK
        case 0x06:
          //printf("%s 0x0%x\n", "ORA", src);
          imd0  = state->registers[0x07];
          if(src == 0x06){
            imd0 |= state->memory[(state->registers[0x04]<<8)&state->registers[0x05]];
          }else{
            imd0 |= state->registers[src];
          }

          state->registers[0x07] = imd0 &0xFF;
          state-> cc->cy = (imd0>>8)&0xFF;
          state-> cc->z  = (imd0 == 0);
          state-> cc->s  = (imd0>>7)&0x01;
          state-> cc->p  = parity(imd0);
        break;
        //INS:OK
        case 0x07:
          //printf("%s 0x0%x\n", "CMP", src);
          imd0  = state->registers[0x07];
          if(src == 0x06){
            imd0 -= state->memory[(state->registers[0x04]<<8)&state->registers[0x05]];
          }else{
            imd0 -= state->registers[src];
          }

          state-> cc->cy = (!imd0>>8)&0xFF;
          state-> cc->z  = (imd0 == 0);
          state-> cc->s  = (imd0>>7)&0x01;
          state-> cc->p  = parity(imd0);
        break;
      }
    break;

    //INS:OK
    case 3:
      tmp = op & 0x07;
      switch(tmp){
        //INS:OK
        case 0x00:
          src = (op>>3)&0x07;
          //printf("%s 0x0%x\n", "Rccc", src);
          switch(src){
            //INS:OK
            case 0x00:
              //if not zero
              if(!state->cc->z){
                imd1 = 1;
              }
            break;
            //INS:OK
            case 0x01:
              //if zero
              if(state->cc->z){
                imd1 = 1;
              }
            break;
            //INS:OK
            case 0x02:
              //if not carry
              if(!state->cc->cy){
                imd1 = 1;
              }
            break;
            //INS:OK
            case 0x03:
              //if carry
              if(state->cc->cy){
                imd1 = 1;
              }
            break;
            //INS:OK
            case 0x04:
              //if not parity
              if(!state->cc->p){
                imd1 = 1;
              }
            break;
            //INS:OK
            case 0x05:
              //if parity
              if(state->cc->p){
                imd1 = 1;
              }
            break;
            //INS:OK
            case 0x06:
              //if not sign
              if(!state->cc->s){
                imd1 = 1;
              }
            break;
            //INS:OK
            case 0x07:
              //if sign
              if(state->cc->s){
                imd1 = 1;
              }
            break;
          }
          if(imd1 == 1){
            opbytes = 0;
            imd0 = state->memory[state->SP] | (state->memory[state->SP+1]<<8);
            state->PC = imd0&0xFFFF;
            state->SP += 2;
          }
          //
        break;
        //INS:OK
        case 0x01:
          tmp = (op>>3)&0x07;
          switch(tmp){
            //INS:OK
            case 0x01:
              //printf("%s\n", "RET");
              //pop the last address from stack and set as PC
              opbytes = 0;
              imd0 = state->memory[state->SP] | (state->memory[state->SP+1]<<8);
              state->PC = imd0&0xFFFF;
              state->SP += 2;
            break;
            //INS:OK
            case 0x03:
              //does not exist
            break;
            //INS:OK
            case 0x05:
              //printf("%s\n", "PCHL");
              //set the PC to H and L
              state->PC = (state->registers[0x04])<<8;
              state->PC &=(state->registers[0x05]);
            break;
            //INS:OK
            case 0x07:
              //printf("%s\n", "SPHL");
              //set the SP to H and L
              state->SP = (state->registers[0x04])<<8;
              state->SP &=(state->registers[0x05]);
            break;
            //INS:OK
            default:
              dest = (op>>4)&0x03;
              //printf("%s 0x0%x\n", "POP", dest);
              imd0 = state->memory[state->SP] | state->memory[state->SP+1]<<8;
              if(dest == 0x03){
                uint8_t psw = (imd0) & 0xFF;
                state->registers[0x07] = imd0>>8 & 0xFF;
                state->cc->z   = (psw)   &0x01;
                state->cc->s   = (psw>>1)&0x01;
                state->cc->p   = (psw>>2)&0x01;
                state->cc->cy  = (psw>>3)&0x01;
                state->cc->ac  = (psw>>4)&0x01;
              }else{
                dest*=2;
                state->registers[dest]   = imd0>>8  &0xFF;
                state->registers[dest+1] = imd0     &0xFF;
              }
              state->SP += 2;
            break;
          }
        break;
        //INS:OK
        case 0x02:
          src = (op>>3)&0x07;
          opbytes = 0; //once again, a hack;
          imd0 = opcode[1] | (opcode[2]<<8);  //this is the JUMP address
          //printf("%s 0x%x\n", "Jccc", imd0);
          imd1 = 0;
          switch(src){
            //INS:OK
            case 0x00:
              //if not zero
              imd1 = !state->cc->z;
            break;
            //INS:OK
            case 0x01:
              //if zero
              imd1 = state->cc->z;
            break;
            //INS:OK
            case 0x02:
              //if not carry
              imd1 = !state->cc->cy;
            break;
            //INS:OK
            case 0x03:
              //if carry
              imd1 = state->cc->cy;
            break;
            //INS:OK
            case 0x04:
              //if not parity
              imd1 = !state->cc->p;
            break;
            //INS:OK
            case 0x05:
              //if parity
              imd1 = state->cc->p;
            break;
            //INS:OK
            case 0x06:
              //if not sign
              imd1 = !state->cc->s;
            break;
            //INS:OK
            case 0x07:
              //if sign
              imd1 = state->cc->s;
            break;
          }
          if(imd1==1){
            state->PC = imd0&0xFFFF;
          }else{
            opbytes = 3;
          }
        break;
        //INS:OK
        case 0x03:
          tmp = (op>>3) & 0x07;
          switch(tmp){
            //INS:OK
            case 0x00:
              //printf("%s\n", "JMP");
              opbytes = 0;    //this is a hack to come out to the right address -> TODO: rework stepping system!
              state->PC = (opcode[2] << 8) | opcode[1];
            break;
            //INS:OK
            case 0x01:
              //does not exist
            break;
            //INS:OK
            case 0x02:
              //printf("%s\n", "OUT");
              imd0 = opcode[1];
              writeIO(imd0&0xFF, state->registers[0x07]);
              opbytes = 2;
            break;
            //INS:OK
            case 0x03:
              //printf("%s\n", "IN");
              imd0 = opcode[1];
              state->registers[0x07] = readIO(imd0&0xFF);
              opbytes = 2;
            break;
            //INS:OK
            case 0x04:
              //printf("%s\n", "XTHL");
              state->registers[0x05] = imd0 = state->memory[state->SP];   //L = memory[stack pointer]
            break;
            //INS:OK
            case 0x05:
              //printf("%s\n", "XCHG");
              imd0 = state->registers[0x04];                    //imd0 = H
              state->registers[0x04] = state->registers[0x02];  // H = D
              state->registers[0x02] = imd0 & 0xFF;             // D = imd0(aka old H)

              imd0 = state->registers[0x05];                    //imd0 = L
              state->registers[0x05] = state->registers[0x03];  // L = E
              state->registers[0x03] = imd0 & 0xFF;             // E = imd0(aka old L)
            break;
            //INS:OK
            case 0x06:
              //printf("%s\n", "DI");
              state->int_enable = 0;
            break;
            //INS:OK
            case 0x07:
              //printf("%s\n", "EI");
              state->int_enable = 0;
            break;
          }
        break;
        //INS:OK
        case 0x04:
          src = (op>>3)&0x07;
          //printf("%s 0x0%x\n", "Cccc", src);
          opbytes = 3;
          imd1 = 0;
          switch(src){
            //INS:OK
            case 0x00:
              //if not zero
              imd1 = !state->cc->z;
            break;
            //INS:OK
            case 0x01:
              //if zero
              imd1 = state->cc->z;
            break;
            //INS:OK
            case 0x02:
              //if not carry
              imd1 = !state->cc->cy;
            break;
            //INS:OK
            case 0x03:
              //if carry
              imd1 = state->cc->cy;
            break;
            //INS:OK
            case 0x04:
              //if not parity
              imd1 = !state->cc->p;
            break;
            //INS:OK
            case 0x05:
              //if parity
              imd1 = state->cc->p;
            break;
            //INS:OK
            case 0x06:
              //if not sign
              imd1 = !state->cc->s;
            break;
            //INS:OK
            case 0x07:
              //if sign
              imd1 = state->cc->s;
            break;
          }
          if(imd1){
            opbytes = 0; //once again, a hack.
            imd0 = opcode[1] | (opcode[2]<<8);  //this is the call address
            imd1 = state->PC+3;
            writeMemory(state->SP-1, (imd1>>8) &0xFF);
            writeMemory(state->SP-2, (imd1) &0xFF);
            state->SP = state->SP - 2;

    				state->PC = (opcode[2] << 8) | opcode[1];
          }
        break;
        //INS:OK -> TODO: fix with M_reg_hack
        case 0x05:
          //INS:OK
          if((op>>3)&0x01){
            #ifdef FOR_CPUDIAG
                      if (5 ==  ((opcode[2] << 8) | opcode[1]))
                      {
                          if (state->registers[0x01] == 9)
                          {
                              uint16_t offset = (state->registers[0x02]<<8) | (state->registers[0x03]);
                              char *str = (char*) &state->memory[offset+3];  //skip the prefix bytes
                              while (*str != '$')
                                  //printf("%c", *str++);
                              //printf("\n");
                          }
                          else if (state->registers[0x01] == 2)
                          {
                              //saw this in the inspected code, never saw it called
                              //printf ("print char routine called\n");
                          }
                      }
                      else if (0 ==  ((opcode[2] << 8) | opcode[1]))
                      {
                          exit(0);
                      }
                      else
             #endif
            //printf("%s\n", "CALL");
            opbytes = 0; //once again, a hack.
            imd0 = opcode[1] | (opcode[2]<<8);  //this is the call address
            imd1 = state->PC+3;
            state->memory[state->SP-1] = (imd1>>8) &0xFF;
            state->memory[state->SP-2] = (imd1)    &0xFF;
            state->PC = imd0&0xFFFF;
            state->SP = state->SP -2;
          }else{
            //INS:OK??
            src = (op>>4)&0x03;
            //printf("%s 0x0%x\n", "PUSH", src);
            if(src == 0x03){
              // uint8_t psw = (state->cc->z |
        			// 				state->cc->s << 1 |
        			// 				state->cc->p << 2 |
        			// 				state->cc->cy << 3 |
        			// 				state->cc->ac << 4 );
              uint8_t psw = (state->cc->s<<7)|(state->cc->z<<6)|(0<<5)|(state->cc->ac<<4)|(0<<3)|(state->cc->p<<2)|(1<<1)|(state->cc->cy);
              writeMemory(state->SP-1, state->registers[0x07]);
              writeMemory(state->SP-2, psw);
            }else{
              src*=2;
              imd0 = state->registers[src+1] | state->registers[src]<<8;
              writeMemory(state->SP-1, imd0>>8 &0xFF);
              writeMemory(state->SP-2, imd0    &0xFF);
            }
            state->SP = state->SP - 2;
          }
        break;
        //INS:OK ->> immediate ALU-OPs
        case 0x06:
          opbytes = 2;
          tmp = (op>>3) & 0x07;
          switch(tmp){
            //INS:OK
            case 0x00:
              //printf("%s\n", "ADI");
              imd0  = state->registers[0x07] + opcode[1];

              state->registers[0x07] = imd0 &0xFF;

              state-> cc->cy = (imd0>>8)&0xFF;
              imd0 = state->registers[0x07];
              state-> cc->z  = (imd0 == 0);
              state-> cc->s  = (imd0>>7)&0x01;
              state-> cc->p  = parity(imd0);
            break;
            //INS:OK
            case 0x01:
              //printf("%s\n", "ACI");
              imd0  = state->registers[0x07] + opcode[1];
              imd0 += state->cc->cy;

              state->registers[0x07] = imd0 &0xFF;
              state-> cc->cy = (imd0>>8)&0xFF;
              imd0 &= 0xFF;
              state-> cc->z  = (imd0 == 0);
              state-> cc->s  = (imd0>>7)&0x01;
              state-> cc->p  = parity(imd0);
            break;
            //INS:OK
            case 0x02:
              //printf("%s\n", "SUI");
              imd0  = state->registers[0x07];
              imd0 -= state->memory[state->PC+1];

              state->registers[0x07] = imd0 &0xFF;
              state-> cc->cy = (imd0>>8);
              imd0 &= 0xFF;
              state-> cc->z  = (imd0 == 0);
              state-> cc->s  = (imd0>>7)&0x01;
              state-> cc->p  = parity(imd0);
            break;
            //INS:OK
            case 0x03:
              //printf("%s\n", "SBI");
              imd0  = state->registers[0x07] - opcode[1];
              imd0 -= state->cc->cy;

              state->registers[0x07] = imd0 &0xFF;
              state-> cc->cy = (imd0>>8);
              imd0 &= 0xFF;
              state-> cc->z  = (imd0 == 0);
              state-> cc->s  = (imd0>>7)&0x01;
              state-> cc->p  = parity(imd0);
            break;
            //INS:OK
            case 0x04:
              //printf("%s\n", "ANI");
              imd0  = state->registers[0x07];
              imd0 &= state->memory[state->PC+1];

              state->registers[0x07] = imd0 &0xFF;
              state-> cc->cy = (imd0>>8)&0xFF;
              state-> cc->z  = (imd0 == 0);
              state-> cc->s  = (imd0>>7)&0x01;
              state-> cc->p  = parity(imd0);
            break;
            //INS:OK
            case 0x05:
              //printf("%s\n", "XRI");
              imd0  = state->registers[0x07];
              imd0 ^= state->memory[state->PC+1];

              state->registers[0x07] = imd0 &0xFF;
              state-> cc->cy = (imd0>>8)&0xFF;
              state-> cc->z  = (imd0 == 0);
              state-> cc->s  = (imd0>>7)&0x01;
              state-> cc->p  = parity(imd0);
            break;
            //INS:OK
            case 0x06:
              //printf("%s\n", "ORI");
              imd0  = state->registers[0x07];
              imd0 |= state->memory[state->PC+1];

              state->registers[0x07] = imd0 &0xFF;
              state-> cc->cy = (imd0>>8)&0xFF;
              state-> cc->z  = (imd0 == 0);
              state-> cc->s  = (imd0>>7)&0x01;
              state-> cc->p  = parity(imd0);
            break;
            //INS:OK
            case 0x07:
              //printf("%s\n", "CPI");
              imd0  = state->registers[0x07];
              imd0 -= state->memory[state->PC+1];

              state-> cc->cy = (imd0>>8)&0xFF;
              state-> cc->z  = (imd0 == 0);
              state-> cc->s  = (imd0>>7)&0x01;
              state-> cc->p  = parity(imd0);
            break;
          }
        break;
        //INS:OK
        case 0x07:
          src = (op>>3)&0x07;
          //printf("%s 0x0%x\n", "RST", src);
          opbytes = 1;
          imd0 = src<<3;  //this is the call//restart address
          //this is the basic call-behaviour
          state->memory[state->SP-1] = ((state->PC)>>8)&0xFF;
          state->memory[state->SP-2] = (state->PC)&0xFF;
          state->PC = imd0&0xFFFF;
          state->SP -= 2;
        break;
      }
    break;
  }
  //Hack:: set the M-Register (since not used in MOV to flags :) -> cheat for dual-reg 3) -> hack implemented
  //imd0 = (state->cc->s<<7)&(state->cc->z<<6)&(0<<5)&(state->cc->ac<<4)&(0<<3)&(state->cc->p<<2)&(1<<1)&(state->cc->cy);
  //state->registers[0x06] = imd0&0xFF;

  state->PC+=opbytes;
}


/*
void ReadFileIntoMemoryAt(State8080* state, char* filename, uint32_t offset){
	FILE *f= fopen(filename, "rb");
	if (f==NULL)
	{
		printf("error: Couldn't open %s\n", filename);
		exit(1);
	}
	fseek(f, 0L, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0L, SEEK_SET);

	uint8_t *buffer = &state->memory[offset];
	fread(buffer, fsize, 1, f);
	fclose(f);
}*/
