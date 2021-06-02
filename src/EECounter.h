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

#ifndef __chipguy_EEPROMfs_h_included
#define __chipguy_EEPROMfs_h_included

#include <Arduino.h>



/*****************************************

chipguy_EEPROMcounter32 - Wear Leveled Counter for AVR EEPROM

Highlights:
  * Implements a 32-bit counter in EEPROM over a dedicated block of EEPROM bytes.
  * Incrementing the counter writes and erases one bit, on average, per increment.
  * The full 32 bit value is persisted across power cycles.
  * Initialize to any 32-bit value.  Unsigned counter wraps to 0 on overflow.
      
Details:
  * The block of EEPROM bytes are used in a circular fashion.
  * Writes are spread over length bytes.  16 minimum, 32 recommended, no maximum
  *  At 16 bytes, each increment cycles about 1.5 bits on average
  *  At 32 bytes, each increment cycles about 1.05 bits on average
  *  At 41 and 64 bytes, each increment averages 1 bit and 0.95 bits, respectively
  *  At 32 bytes, a 100,000-write chip should be good for 24,500,000 increments
  *  At 128 bytes, the same chip should be good for 118,700,000 increments

*****************************************/

class EECounter {
public:
    EECounter(int _start_address, int _length);

    void begin(bool recover=false);
    uint32_t read();
    uint32_t increment();
    uint32_t resetcount(uint32_t newcount);
    
    // direct optimized EEPROM access avoiding Arduino EEPROM.h
    static byte readeeprom(int addr);
    static void updateeeprom(int addr, byte data);

    
private:
    int start_address;
    int length;
    int dec_index(int x);
    int inc_index(int x);
    uint32_t op(byte op_id);

};






/*****************************************

chipguy_EEPROMfs - Trivial File System for EEPROM

Highlights:
  * File system holds records, let's not call them files.
  * Designed around storing configuration choices.
  * Each record can be 1 to 255 bytes.
  * Each record has a one-byte "filename" called "key" (0x00-0xFF)
  * There's no deleting or resizing records, but you can add and overwrite them.
  * There can only be one record per unique "key", with one exception: ID list.
  
List feature:
  * Designed for maintaining an access control database of valid 32-bit ID numbers.
  * You can add/remove ID numbers.  You can query if ID numbers are in the list.
  * ID 0 is reserved for deleted IDs.  Space from deleted IDs is used only for new IDs.
  * There can be more than one ID list.  The whole list is accessed by a single "key".

*****************************************/


class EERecordSystem {
public:
    EERecordSystem();
    
    // Starts up the class, formatting the file system if needed.
    // Formatting overwrites the first 6 bytes of the EEPROM with a header.
    void begin();
    
    // Updates or adds a new record to the EEPROM.
    // Returns false if wasn't possible, such as insufficient space.
    // Updates (overwrites) a record when its key matches.  Note size cannot change.
    // Beware of incomplete writes due to power loss (first bytes written, last bytes not).
    bool updaterecord(byte key, void *recorddata, byte datasize);
    bool updaterecord(byte key, byte data); // if the record is a single char/byte.
    
    // ID list functions
    // queries to see if an ID exists in the list.
    bool queryID(uint32_t id, byte listid, bool compare_only_24_bits=false);
    
    // adds an ID to list if it is not already present.
    bool addID(uint32_t id, byte listid);
    
    // deletes an ID from the list if it is present.
    // Can delete multiple IDs if matching them on a partial bit mask.
    bool deleteID(uint32_t id, byte listid, bool compare_only_24_bits=false);
    
    // enumerate valid IDs in the database, calls you back
    void enumIDs(byte listid, void (*enumcallback)(uint32_t));

    // Gets the EEPROM location of a record's data, or -1 if key not found.
    int getrecordaddress(byte key, byte &datasize);    

private:
    bool eeprom_addnewrecord(byte key, void *recorddata, byte datasize);
    bool idop(char op, uint32_t id, byte listid, void (*enumcallback)(uint32_t));
    bool began=false;

};















#endif __chipguy_EEPROMfs_h_included