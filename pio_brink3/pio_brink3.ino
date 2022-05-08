#include <hardware/pio.h>

// Our assembled program:
#include "blink.pio.h"
#include "blink1.pio.h"
#include "blink2.pio.h"

#define LED_RED		17
#define LED_GREEN	16
#define LED_BLUE	25

void setup() {
  //init_serial();
  //clear_rgb();

  PIO pio = pio0;
  uint offset0 = pio_add_program(pio, &blink_program);
  Serial.printf("Loaded program at %d\n", offset0);
  uint offset1 = pio_add_program(pio, &blink1_program);
  Serial.printf("Loaded program at %d\n", offset1);
  uint offset2 = pio_add_program(pio, &blink2_program);
  Serial.printf("Loaded program at %d\n", offset2);

    blink_pin_forever(pio, 0, offset0, LED_RED,   2);
    blink_pin_forever(pio, 1, offset1, LED_GREEN, 2);
    blink_pin_forever(pio, 2, offset2, LED_BLUE,  2);
}
void init_serial(){
  Serial.begin(115200);
  while (!Serial) { ; } // wait for serial port to connect. Needed for native USB port only
  delay(10);
}
void clear_rgb(){
}

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);
    pio->txf[sm] = clock_get_hz(clk_sys) / (2 * freq);
    Serial.printf("Blinking pin %d at %d Hz\n", pin, freq);
}

void loop() { }
