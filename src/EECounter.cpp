/*
EECounter Copyright 2021 chipguyhere, License: GPLv3

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <Arduino.h>
#include "EECounter.h"



EECounter::EECounter(int _start_address, int _length) {
    start_address=_start_address;
    length=_length;
    while (length < 16) {} // don't use length less than 16.
}

static byte EECounter::readeeprom(int addr) {
  while(EECR & (1<<EEPE)); /*wait until previous write any*/
  EEAR = addr;
  EECR |= (1<<EERE);
  return EEDR;
}


static void EECounter::updateeeprom(int addr, byte data) {
  for (byte i=0; i<5; i++) {  
    while(EECR & (1<<EEPE)); /*wait until previous write any*/
    EEAR = addr;
    EECR |= (1<<EERE);
    byte b = EEDR;
    if (b==data) return;
  
    while(EECR & (1<<EEPE)); /*wait until previous write any*/
    EEDR=data;  
    // need erase?
    if ((b & data) != data) {
      // yes.  erase only?
      if (data==0xff) {  // EEPM0=erase only EEPM1=write only  00=erase&write
        EECR = (EECR & ~(1<<EEPM1)) | (1<<EEPM0);      
      } else {
      // erase and write too.
        EECR &= ~((1<<EEPM0)|(1<<EEPM1));
      }
    } else {
      // write only
      EECR = (EECR & ~(1<<EEPM0)) | (1<<EEPM1);      
    }
    EECR |= (1<<EEMPE);
    EECR |= (1<<EEPE);
    if (readeeprom(addr)==data) return;
  }
}


uint32_t EECounter::resetcount(uint32_t newcount) {
    for (int i=0; i<5; i++) {
        uint32_t z = newcount >> ((4-i)*7);
        byte bz = z;
        bz |= 0x80;
        if (bz==0xFF) bz=0x70;      
        updateeeprom(start_address+i,bz);
    }
    updateeeprom(start_address+5,0xff);     
    for (int i=6; i<length; i++) {
        if (readeeprom(start_address+i)==0xff) {
            updateeeprom(start_address+i, 0x77);
        }
    }
    readeeprom(0);
}

uint32_t EECounter::read() { return op(0); }
uint32_t EECounter::increment() { return op(1); }

int EECounter::inc_index(int x) {
    return (x+1) % length;
}

int EECounter::dec_index(int x) {
    return (x-1+length) % length;
}

void EECounter::begin(bool recover=false) {
    if (recover) {
        uint32_t try0 = op(0);
        uint32_t try1 = op(1);
        uint32_t try2 = op(0);
        if (try0+1 != try1 || try1 != try2) resetcount(try0+1);    
    } else {
        op(2);
    }
}

// op_id 0 = read
// op_id 1 = increment
// op_id 2 = begin
uint32_t EECounter::op(byte op_id) {

    // Find first and second FF
    int first_FF_index=-1, second_FF_index=-1;
    for (int i=0,a=start_address; i<length; i++,a++) {
        if (readeeprom(a)==0xFF) {
            if (first_FF_index != -1) {
                if (op_id==2 && second_FF_index != 1) {
                    // On begin, if we get too many FF's, then reset.
                    resetcount(0);
                }
                second_FF_index=i;
                if (op_id != 2) break;          
            }
            first_FF_index=i;
        }
    }
    
    if (op_id==2) {
        if (first_FF_index==-1) resetcount(0);
        return;    
    }

    // If we don't have any FF's, the counter is 0
    if (first_FF_index==-1) {
        if (op_id==1) {  // increment
            resetcount(1);
            return 1;
        }
        readeeprom(0);            
        return 0;
    }
    
    // If they are the first and last byte, then the one at index 0 is actually second.
    if (first_FF_index==0 && second_FF_index==length-1) {
        first_FF_index=second_FF_index;
        second_FF_index=0;
    }
    
    // Scan all the bytes.
    // Meaning of bytes are as follows:
    // FF = erased byte.  First one has no value,
    // FF (second erased byte) has a value of 1.
    // 7F = value 2.  writing it has a net value of 1 as it consumes an erased byte.
    // 3F, 1F, 0F, 07, 03, 01, 00 = value 3,4,5,6,7,8,9
    // 80 thru FE = has a value of 2, plus provides 7 bits to the checkpoint register.
    // 70 = has a value of 2, plus provides "1111111" to the checkpoint register.
    //      (required because FF is already reserved for erased status)
    uint32_t count=0;
    uint32_t checkpoint_register=0;
    byte checkpoint_count=0;
    int first_checkpoint_index=0;
    int x = first_FF_index;
    byte ff_plus_one_present=0;
    for (int i=0; i<length; i++,x=inc_index(x)) {
        byte b = readeeprom(start_address+x);
        if (b == 0x70 || (b >= 0x80 && b < 0xFF)) {
            // checkpoint_count stays intact        
        } else {
            checkpoint_count=0;
        }
        for (byte c=2,q=0x7f; c<=9; c++,q>>=1) { 
            if (b==q) {
                count+=c;
                break;
            }
        }
        if (b==0xFF) {
            if (i==1) ff_plus_one_present=1,count++;
        } else {
            if (b==0x70) b=0xFF; // treat it like 0xFF now that we've cased it out
            if (b>=0x80) {
                if (checkpoint_count==0) checkpoint_register=0;
                b &= 0x7F; // keep only the bottom 7 bits.
                checkpoint_register <<= 7; // make room for 7 bits.
                checkpoint_register |= b; // add them.
                checkpoint_count++;
                count+=2;
                if (checkpoint_count==5) {
                    // adopt the count
                    count = checkpoint_register + ff_plus_one_present;
                    checkpoint_count=0;
                    first_checkpoint_index = (x-4+length)%length;
                }
              
            }
        }
    }
    if (op_id==1) {
        count++;
        second_FF_index = inc_index(first_FF_index);
        int second_FF_address = second_FF_index + start_address;
        int third_FF_address = start_address + inc_index(second_FF_index);
        int before_FF_address = start_address + dec_index(first_FF_index);
        
        // Erasing second_FF_index will increment, if it's not already erased.
        if (readeeprom(second_FF_address) != 0xFF) {
            if (readeeprom(third_FF_address) == 0xFF) {
                // (but avoid letting third FF be FF, or it throws off count)
                updateeeprom(third_FF_address, 0x77);             
            }
            updateeeprom(second_FF_address, 0xFF);
        } else {
            if (readeeprom(third_FF_address) == 0xFF) {
                updateeeprom(third_FF_address, 0x77);             
            }       
            // see if we can increment by clearing a bit off the latest byte
            byte lup = readeeprom(before_FF_address);
            byte q;
            for (q=0x7f; q; q>>=1) {
                if (q==lup) {
                    updateeeprom(before_FF_address, lup >> 1);
                    break;
                }
            }
            if (!q) {
                // if we can't, write a new byte.
                // that will either be a byte of the future checkpoint register,
                // or it will be a 7F byte (which is one bit stripped off an FF byte)
                byte i;
                for (i=2; i<=6; i++) {
                    if (((first_FF_index + i) % length) == first_checkpoint_index) {
                        // If we will overwrite first_checkpoint_index i bytes from now,
                        // write part of future checkpoint register
                        byte ii4 = i * 2 - 4;
                        uint32_t fcr = count + ii4; 
                        fcr >>= (i-2)*7;
                        byte bfcr = fcr;
                        bfcr |= 0x80;
                        if (bfcr==0xFF) bfcr=0x70;
                        updateeeprom(first_FF_index + start_address, bfcr);
                        break;
                    }
                }
                if (i==7) updateeeprom(first_FF_index + start_address, 0x7F);
                
            }
        }
    }
    readeeprom(0);
    return count;
}





