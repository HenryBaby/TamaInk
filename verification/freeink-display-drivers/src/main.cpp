#include <Arduino.h>
#include <FreeInkDisplay.h>

// Compile/link smoke target only. No begin(), GPIO, SPI, or display operation
// is performed, so building this sketch never touches attached hardware.
freeink::FreeInkDisplay display(/*sclk=*/18, /*mosi=*/23, /*cs=*/5,
                                /*dc=*/17, /*rst=*/16, /*busy=*/4);

void setup() {}
void loop() {}
