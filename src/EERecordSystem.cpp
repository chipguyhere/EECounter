/*
EEPROMfs Copyright 2021 chipguyhere, License: GPLv3

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
#include <EEPROM.h>
#include "EECounter.h"



EERecordSystem::EERecordSystem() {}

// Header: first six bytes we write to indicate our file system is present.
// We take over the whole EEPROM as new records are added, growing toward the end.
const byte eeprom_initial_image[] PROGMEM = { 4, 'C', 'A', 'S', 0, 0};

void EERecordSystem::begin() {
  began=true;
  if (EECounter::readeeprom(1) != 'C' || EECounter::readeeprom(2) != 'A' || EECounter::readeeprom(3) != 'S')  
	for (int i = 0; i < sizeof(eeprom_initial_image); i++) 
	  EECounter::updateeeprom(i, pgm_read_byte_near(eeprom_initial_image + i));
  // fix location 0 as it commonly gets corrupted.
  if (EECounter::readeeprom(0) != 0x04) EECounter::updateeeprom(0, 0x04);
}



bool EERecordSystem::updaterecord(byte key, byte data) {
  if (began==false) return false;
  return updaterecord(key, &data, 1);
}

bool EERecordSystem::updaterecord(byte key, void *recorddata, byte datasize) {
  if (began==false) return false;

  byte existingDatasize=0;
  int address = getrecordaddress(key, existingDatasize);
  // address points to key, if valid.

  // write record if it doesn't already exist
  if (address == -1) return eeprom_addnewrecord(key, recorddata, datasize);

  // if overwriting a record, only overwrite it at its existing size, if smaller
  if (datasize < existingDatasize) datasize = existingDatasize;

  // update the record when it does
  for (int i = 0; i < datasize; i++) EECounter::updateeeprom(address + i + 1, ((byte*)(recorddata))[i]);
  return true;
}


int EERecordSystem::getrecordaddress(byte key, byte &datasize) {
  if (began==false) return -1;
    
  int EElength = EEPROM.length();

  int EEaddress = 0;
  while (EEaddress < EElength) {
    byte b = EECounter::readeeprom(EEaddress);
    if (b == 0) return -1;
    int currentrecordaddress = EEaddress + 1;
    byte currentrecordlength = b - 1;
    EEaddress += b;
    
    if (EECounter::readeeprom(currentrecordaddress) == key) {
      datasize = currentrecordlength;
      return currentrecordaddress;
    }
  }
  return -1;
}



// Do an operation on an ID list (all of which involves walking the list)
bool EERecordSystem::idop(char op, uint32_t id, byte listid, void (*enumcallback)(uint32_t)=NULL) {
  // uninitialized eeprom? abort
  if (began==false) return false; // eeprom has not been initialized
  if (id==0) return false;
  int EElength = EEPROM.length();

  int EEaddress = 0;
  int freeSlot=0;
  
  // if op is A (Add), temporarily treat op as Q (Query), adding only if fails.
  bool isAdd=false;
  if (op=='A') {
    isAdd=true;
    op='Q';
  }

  bool delete_success=false;
  
  while (EEaddress < EElength) {
    byte b = EECounter::readeeprom(EEaddress);
    if (b == 0) break;
    int currentrecordaddress = EEaddress + 1;
    byte currentrecordlength = b - 1; // length includes the identifier byte but we've subtracted the length byte
    EEaddress += b;
    if (EECounter::readeeprom(currentrecordaddress) == listid) {
      currentrecordlength--;
      currentrecordaddress++;

      if (op=='q' || op=='Q' || op=='x' || op=='d' || op=='D' || op=='E') { // Query/Delete to see if ID exists (q = compare only 24 bits, x = delete comparing only 8 bits).  E=enumerate
        while (currentrecordlength >= 4) {
          int i=0;
          bool match=true;
          byte *bid = (byte*)(&id);
          uint32_t v=0;
          byte *vid = (byte*)(&v);
          for (int i=0; i<4; i++) {
            currentrecordlength--;            
            byte rb = EECounter::readeeprom(currentrecordaddress++);
            *vid++ = rb;
            if (i==1 && (op=='x')) continue;
            if (i==3 && (op=='d' || op=='q')) continue;
            if (rb != *bid++) match=false;

          }
          if (match) {
            if (op=='q' || op=='Q') return true;
            // Delete the record by overwriting it with zeroes.
            if (op=='d' || op=='D' || op=='x') {
              delete_success=true;
              for (i=-4; i<0; i++) EECounter::updateeeprom(currentrecordaddress+i, 0);
            
            }
          }
          if (v==0) {
            if (freeSlot==0) freeSlot = currentrecordaddress-4;
          } else if (op=='E') {
            enumcallback(v);            
          }
        }
      }
    }
  }
  if (op=='d' || op=='D' || op=='x') return delete_success;
  if (isAdd) {
    // if we found a free slot, use it.
    if (freeSlot) {
//      SerialMonitor->println(F("found free slot"));          
      
      byte *bid = (byte*)(&id);
      for (int i=0; i<4; i++) EECounter::updateeeprom(freeSlot+i, *bid++);          
    } else {
      // otherwise, invent a new slot.
      byte newslot[12];
      memset(&newslot, 0, 12);
      *(uint32_t*)(&newslot[0])=id;
//      SerialMonitor->println(F("added new record"));          

      return eeprom_addnewrecord(listid, &newslot, 12);
    }
  }
  return false;

}


bool EERecordSystem::queryID(uint32_t id, byte listid, bool compare_only_24_bits=false) {
  return idop(compare_only_24_bits ? 'q' : 'Q', id, listid);
}

bool EERecordSystem::addID(uint32_t id, byte listid) { return idop('A', id, listid); }

bool EERecordSystem::deleteID(uint32_t id, byte listid, bool compare_only_24_bits=false) { 
  return idop(compare_only_24_bits ? 'd' : 'D', id, listid);
}

void EERecordSystem::enumIDs(byte listid, void (*enumcallback)(uint32_t)) {
  return idop('E', 0, listid, enumcallback);


}
