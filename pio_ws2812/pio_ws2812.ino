#include <hardware/pio.h>
#include "ws2812.pio.h"

#define WS2812_PWR 11
#define WS2812_PIN 12

void init_serial(){
  Serial.begin(115200);
  int wcnt=100;
  while(true){
  	if(Serial) {Serial.printf("COM.Connect\n"); break;}
  	if((wcnt--)==0) break;
  	delay(10);
  }
}

void setup() {
	init_serial();
    pinMode(WS2812_PWR, OUTPUT); digitalWrite(WS2812_PWR, HIGH);
    Serial.printf("WS2812 Test, using pin %d(Din):%d(Vcc)\n", WS2812_PIN, WS2812_PWR);

    PIO pio = pio0; int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, true);
}

uint8_t r=0,g=85,b=170;
void loop() { 
    //uint32_t pixel_grb = rand();
    uint32_t pixel_grb = ((uint32_t) (r++) << 8) | ((uint32_t) (g++) << 16) | (uint32_t) (b++); 
    pio_sm_put_blocking(pio0, 0, (pixel_grb & 0x1f1f1f1f) << 8u);
    sleep_ms(50);
}
