# EECounter

EECounter is persistent 32-bit counter in EEPROM memory, optimized for increment-by-one on AVR Arduino boards (Arduino Uno/Nano/Mega/Leonardo).

EECounter spreads the write cycles evenly over your selected range of 16, 32, or more bytes of EEPROM.  Write/erase cycles are minimized
via direct register access to individual EEPROM bits.  A counter using just 32 dedicated EEPROM bytes can be incremented over 24 million
times while staying under 100,000 erase/write cycles.

## Usage

```
#include "EECounter.h"
```

The constructor takes two parameters: ```start_address``` and ```length```.
```
EECounter counter1(100, 32); // defines a counter occupying 32 bytes, EEPROM locations 100 thru 131
EECounter counter2(200, 456); // defines a counter occupying 456 bytes, EEPROM locations 200 thru 655
```

In this example, the only difference between these two counters is that counter2 spreads its writes over a much
larger EEPROM area.  It could sustain 400 million increments while keeping under 100,000 write cycles.

Calling ```begin()``` is optional, and resets corrupted or never-initialized counters to 0.  Initialization is optional.
A never-initialized counter might start its count at an arbitrary small non-zero integer.

Calling ```read()``` reads the counter directly from EEPROM.  The count is 32 bits, unsigned, and rolls to
zero when it overflows.

Calling ```increment()``` increments the counter in EEPROM and returns the new count.  

Calling ```resetcount()``` re-initializes the counter with a new start count.  This consumes a write/erase cycle on the EEPROM on
part or all of the range.

## Background

Each call to ```increment()``` typically erases or writes exactly one byte of EEPROM, and is essentially atomic.

This class implements a circular journal over the range of the EEPROM.  It erases and writes bytes in separate steps.
Direct register access is used to gain access to writing partial bytes (individual bits) at a time.
Selective erasing is part of the counting strategy.  Most of the time, ```increment()``` erases one byte or flips one bit.

## Tips

EEPROM integrity is vulnerable to overvoltage and undervoltage situations.  Sometimes these are unanticipated.  If your Arduino
project drives relays, motors, or electromagnetic loads, these produce strong voltage surges when powered off (google "back EMF" for
more information on this).  Those surges will corrupt EEPROM if they reach the Arduino's processor.

Avoid using EEPROM location 0.  On Arduino, it is most likely to get corrupted during power offs.  Consider it a burner location.

There is a secondary mode for ```begin```: ```begin(true)```.
This will perform one "test" increment of the counter, and then verify that the EEPROM
reads correctly with the new count.  If it does not, the counter will be re-initialized with the anticipated value.  Note
that this permanently increments the counter and should only be used where continuity is preferable to

