#include <hardware/pio.h>

// Our assembled program:
#include "blink.pio.h"

#define LED_RED		17
#define LED_GREEN	16
#define LED_BLUE	25

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; } // wait for serial port to connect. Needed for native USB port only
  delay(10);

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &blink_program);
    Serial.printf("Loaded program at %d\n", offset);

    blink_pin_forever(pio, 0, offset, LED_RED,   4);
    blink_pin_forever(pio, 1, offset, LED_GREEN, 2);
    blink_pin_forever(pio, 2, offset, LED_BLUE,  1);
}

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);
    pio->txf[sm] = clock_get_hz(clk_sys) / (2 * freq);
    Serial.printf("Blinking pin %d at %d Hz\n", pin, freq);
}

void loop() {

 }
