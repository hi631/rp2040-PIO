//#include <stdio.h>
//#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#define LED_RED		17
#define LED_GREEN	16
#define LED_BLUE	25

#define CAPTURE_MEM			32768 // 1024 // 2048 // 32768	// Byte(Max32768Byte)
#define CAPTURE_RING_BITS	   15 //   10 //   11//     15	// CAPTURE_MEMのビット幅を指定

uint32_t capture_buf[CAPTURE_MEM/4] __attribute__((aligned(CAPTURE_MEM))); //キャプチャバッファ(アドレスを補正)
const uint CAPTURE_PIN_BASE  =  0;										// 入力端子の先頭
const uint CAPTURE_PIN_MAX   =  8; // 4,8,16,30						// 入力端子数(Max30?)
const uint CAPTURE_PIN_IRQ   = CAPTURE_PIN_BASE + CAPTURE_PIN_MAX - 2;	// トリガ入力端子
const uint CAPTURE_PIN_TDLY  = CAPTURE_PIN_BASE + CAPTURE_PIN_MAX - 1;	// トリガ遅延中表示
const uint CAPTURE_N_ALL     = CAPTURE_MEM*8/CAPTURE_PIN_MAX;			// キャプチャビット数
const uint CAPTURE_D_ALL     = 128;										// テキスト表示での文字数
uint CAPTURE_DIV             = 1; 										// キャプチャクロック分周(1=125MHz)

void setup() {
  init_led();
  init_serial();
  cmain();
  set_dumy_wave(pio1, 0, CAPTURE_PIN_BASE + CAPTURE_PIN_MAX - 4, 32 );	// ダミー波形を生成
  set_dumy_trig(pio0, CAPTURE_PIN_IRQ);									// ダミートリガ信号を作成
}
int rbfp = -1;
int req_mode = 0, req_send = 'A';
char rd, rbf[128];
uint capture_req = 0;
void loop() {
  if(rbfp==-1) {Serial.printf("\r\n>"); rbfp = 0; rbf[0] = 0;}
  if(Serial.available() > 0) { 
    rd = Serial.read();
    Serial.printf("%c", rd);
    if(rd>=0x20 || rd==0x08){
      if(rd!=0x08) rbf[rbfp++] = rd;
      else if(rbfp>0) rbfp--; 
    } else {
      if(rd==0x0d) { 
      	rbf[rbfp] = 0;
		int num = strtol(&rbf[1],NULL,16); // Serial.printf("[%d]", num);
        switch(rbf[0] & 0xdf){
          case 'C': req_send = 'A'; req_mode = 'C'; capture_req = 0; break; // 文字で表示
          case 'D': req_send = 'B'; req_mode = 'C'; capture_req = 0; break; // バイナリ出力
          case 'R':                 req_mode = 'R'; capture_req = 0; break;	// 上記を繰り返し実行
          case 'T': CAPTURE_DIV = num; 								// キャプチャのクロック分周比を設定
          	pio_sm_set_clkdiv (pio0, 0, num); pio_sm_set_clkdiv (pio0, 1, num); break;
          default : req_mode = 0; break; 
        }
        rbfp = -1;
      }
    }
  }
  if(req_mode!=0) { 
  	if(capture_req==2){	// キャプチャしていたら
       disp_run(req_send); 
      if(req_mode=='R') capture_req = 0;
      else              req_mode = 0;
  	}
  } 
  if(capture_req==0) {
    pio_sm_set_enabled(pio0, 0, true); pio_sm_set_enabled(pio0, 1, true); // キャプチャを開始
    capture_req = 1;
  }
}

void init_led() { pinMode(LED_RED, OUTPUT); pinMode(LED_GREEN, OUTPUT); pinMode(LED_BLUE , OUTPUT);}
void init_serial(){
  Serial.begin(115200);
  int wcnt=100;
  while(true){
  	if(Serial) {Serial.printf("COM.Connect\r\n"); break;}
  	if((wcnt--)==0) break;
  	delay(10);
  }
  delay(100);
}
//-- main ----------------------------------------------------
void cmain() {
    Serial.printf("PIO logic analyser example+\n");
    set_capture_pin(pio0, 0, CAPTURE_PIN_BASE, CAPTURE_PIN_MAX, CAPTURE_DIV);	// キャプチャーはsm=0を使う
	set_irq(pio0, 1, CAPTURE_DIV);												// 完了割り込みはsm=1を使う
	set_dma();																	// DMAの読み込みを設定
}

static inline uint bits_packed_per_word(uint pin_count) {
   const uint SHIFT_REG_WIDTH = 32;
   return SHIFT_REG_WIDTH - (SHIFT_REG_WIDTH % pin_count); // DMA一回毎の格納ビット数
}

void disp_run(int send_md){
    check_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_MAX, CAPTURE_N_ALL);
    if(send_md=='A')
   	  disp_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_MAX, CAPTURE_N_ALL);
   	else
   	  send_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_MAX, CAPTURE_N_ALL);
}

int start_bufp, trig_bufp;
void check_capture_buf(const uint32_t *buf, uint pin_base, uint pin_count, uint32_t n_samples) {
    int pin = CAPTURE_PIN_TDLY;
    int tseq = 0; start_bufp = 0;
    uint record_size_bits = bits_packed_per_word(pin_count);
    for (int sample = 0; sample < n_samples; ++sample) {
        uint bit_index = pin + sample * pin_count;
        uint word_index = bit_index / record_size_bits;
        uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
        int  dtb = buf[word_index] & word_mask ? 1 : 0;
        if(tseq==0 && dtb==1) { tseq = 1; trig_bufp = sample;}
        if(tseq==1 && dtb==0) { tseq = 2; start_bufp = sample;} // キャプチャー終了位置(大体)
    }
}

void send_capture_buf(const uint32_t *buf, uint pin_base, uint pin_count, uint32_t n_samples) {
  //キャプチャバッファをバイナリで出力
  uint8_t txb, txc, txd;
    Serial.printf("CD(%02d:%05d:%05d)\r\n", pin_count, n_samples, CAPTURE_DIV);
    for (int pin = 0; pin < pin_count; ++pin) {
        //Serial.printf("P%02d:\r\n", pin + pin_base);
        uint record_size_bits = bits_packed_per_word(pin_count);
        txc = 0;
        for (int sample = 0; sample < n_samples; ++sample) {
            uint pbufbp = sample+start_bufp; 
            uint bit_index  = pin + (pbufbp) * pin_count;
            //uint bit_index = pin + sample * pin_count;
            uint word_index = (bit_index / record_size_bits) % (CAPTURE_MEM/4);
            uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
            txb = buf[word_index] & word_mask ? 1 : 0; txd = (txd <<1 ) | txb;
            if(txc++==7) { Serial.write(txd); txc =0; }
        }
        Serial.printf("\r\n");
    }
}

void disp_capture_buf(const uint32_t *buf, uint pin_base, uint pin_count, uint32_t n_samples) {
    //キャプチャバッファを次のようにテキスト形式でデータ圧縮して表示
    // 00：__--__--__--__--__--__--
    // 01：____----____----____----
    //各FIFOレコードは、pin_countが32の係数かどうかで、部分的にしか埋められない場合があります。
    uint record_size_bits = bits_packed_per_word(pin_count);
    int pacs = CAPTURE_N_ALL/CAPTURE_D_ALL ;
    Serial.printf("   Disp Capture(1/%d)\r\n", pacs);
    for (int pin = 0; pin < pin_count; ++pin) {
        Serial.printf("%02d: ", pin + pin_base);
        for (int sample = 0; sample < CAPTURE_D_ALL; ++sample) {
          int pacct = 0; char pacch;
          for(int pacl=0; pacl<pacs; pacl++) {
            uint pbufbp = (sample*pacs+pacl)+start_bufp; 
            uint bit_indexp  = pin + (pbufbp) * pin_count;
            uint word_indexp = (bit_indexp / record_size_bits) % (CAPTURE_MEM/4);
            uint word_maskp = 1u << (bit_indexp % record_size_bits + 32 - record_size_bits);
            if(buf[word_indexp] & word_maskp) pacct++;
          }
          if(pacct!=0 && pacct!=pacs) Serial.printf(":");
          else
            Serial.printf(pacct>(pacs/2) ? "-" : "_");
        }
        Serial.printf("\r\n");
    }
}

//-- IRQ handler ------------------------------------------------------------
uint dma_chan0, dma_chan1;
int irq_counter = 0;
void dma_handler1() {
  irq_counter++; digitalWrite(LED_GREEN, (irq_counter >> 11) & 1);
  dma_hw->ints0 = 1u << dma_chan0;	//割り込み要求0をクリア
}
void dma_handler2() {
  irq_counter++; digitalWrite(LED_BLUE, (irq_counter >> 13) & 1);
  dma_hw->ints0 = 1u << dma_chan1;	//割り込み要求1をクリア
}
void pio0_irq0(){
	//delayMicroseconds(10);
    pio_interrupt_clear(pio0, 0);			// irq0割り込みをクリア
    pio_sm_set_enabled(pio0, 0, false);		// sm0の動作を停止
    pio_sm_set_enabled(pio0, 1, false);		// sm1の動作を停止
    capture_req = 2;
}

//-- DMA Setting ------------------------------------------------------------
void set_dma(){
    // DMAを設定
    dma_chan0 = dma_claim_unused_channel(true); 
    dma_chan1 = dma_claim_unused_channel(true);
    hard_assert(capture_buf);
    dma_set_priority();
    dma_init(dma_chan0, true); dma_init(dma_chan1, false);
}
void dma_init(uint dma_chan, bool dmaena){
  dma_channel_config dma_config = dma_channel_get_default_config(dma_chan);
  channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
  channel_config_set_read_increment(&dma_config, false);  // Pins
  channel_config_set_write_increment(&dma_config, true);  // RAM buffer.
  channel_config_set_ring(&dma_config, true, CAPTURE_RING_BITS); 		// バッファの先頭にラップする
  channel_config_set_dreq(&dma_config, pio_get_dreq(pio0, 0, false)); 	// sm0によって割り込まれる

  if(dma_chan==dma_chan0) {
    channel_config_set_chain_to(&dma_config, dma_chan1);  	// 完了したら、次のチャネル(1)を開始
    dma_channel_set_irq0_enabled(dma_chan, true);    		// 割り込みチャネル(0)を使用
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler1);    	// IRQハンドラー(0)を設定
    irq_set_enabled(DMA_IRQ_0, true);						// 割り込み(0)を許可
  } else {
    channel_config_set_chain_to(&dma_config, dma_chan0); 	// 完了したら、次のチャネル(0)を開始
    dma_channel_set_irq1_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_1, dma_handler2);
    irq_set_enabled(DMA_IRQ_1, true);
  }
  dma_channel_configure(dma_chan, &dma_config,
                        capture_buf,   	// dst
                        &pio0->rxf[0],  // src(pio0.sm0)
                        CAPTURE_MEM,	// 転送カウント
                        dmaena          // 開始設定
    );
}

void dma_set_priority(){
    // DMAに高いバス優先度を付与して、プロセッサが邪魔にならないようにします。
    //これは、ここで16ビット/clkを超える(バスを完全に飽和させる)ものをプッシュする場合にのみ必要
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
}
//-- PIO Setting ------------------------------------------------------
#define wrap_wave_top 	1
#define wrap_wave 		2
static const uint16_t prog_wave_instructions[] = {
    0xe081, //  0: set    pindirs, 1	// init start
    0xe001, //  1: set    pins, 1		// .wrap_wave_top
    0xe000, //  2: set    pins, 0		// .wrap_wave
};
static const struct pio_program prog_wave = {
    .instructions = prog_wave_instructions, .length = wrap_wave+1, .origin = -1, };
//----
#define wrap_irq_top	2
#define wrap_irq		10
static const uint16_t pio_irq_instructions[] = {
    0x80a0, //  0: pull   block			// トリガ後の遅延時間を読み込み
    0x6040, //  1: out    y, 32                      
    0xa022, //  2: mov    x, y                       
    0x0043, //  3: jmp    x--, 3		// スタート時も遅延                     
    0x2080+CAPTURE_PIN_IRQ, //  4: wait   1 gpio, x	// CAPTURE_PIN_IRQの立ち上がりを待つ                
    0xe001, //  5: set    pins, 1		// キャプチャ後の遅延中を通達
    0xa022, //  6: mov    x, y
    0x0047, //  7: jmp    x--, 7		// 遅延時間ループ                     
    0xc000, //  8: irq    nowait 0		// IRQでキャプチャ終了を通達                   
    0xe000, //  9: set    pins, 0		// キャプチャー完了を通達
    0x2000+CAPTURE_PIN_IRQ, // 10: wait   0 gpio, x	// CAPTURE_PIN_IRQのの立下りを待つ                   
};
static const struct pio_program prog_irq = {
    .instructions = pio_irq_instructions, .length = wrap_irq+1, .origin = -1, };
//----
#define wrap_tsig_top	3
#define wrap_tsig		8
static const uint16_t pio_tsig_instructions[] = {
    0x80a0, //  0: pull   block                      
    0x6040, //  1: out    y, 32                      
    0x80a0, //  2: pull   block                      
    0xa022, //  3: mov    x, y                       
    0xe001, //  4: set    pins, 1                    
    0x0045, //  5: jmp    x--, 5                     
    0xe000, //  6: set    pins, 0                    
    0xa027, //  7: mov    x, osr                     
    0x0048, //  8: jmp    x--, 8                     
};
static const struct pio_program prog_tsig = {
    .instructions = pio_tsig_instructions, .length = wrap_tsig+1, .origin = -1, };
//----
#define wrap_capture_top	0
#define wrap_capture		0
static const uint16_t capture_prog_instr[] = {                   
    0x4000+CAPTURE_PIN_MAX, //0x4008, //  0: in     pins, n(1-32)                     
};
    struct pio_program capture_prog = {
            .instructions = capture_prog_instr, .length = 1, .origin = -1 };
//----
void set_capture_pin(PIO pio, uint sm, uint pin_base, uint pin_count, float div) {
    //ｎ個(pin_count)のピンをキャプチャするプログラムの連続実行を設定
    uint offset = pio_add_program(pio, &capture_prog);
    pio_sm_config c = pio_program_get_default_config(offset, wrap_capture_top, wrap_capture);

    sm_config_set_in_shift(&c, true, true, bits_packed_per_word(pin_count));
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    sm_config_set_in_pins(&c, pin_base);
    sm_config_set_clkdiv(&c, div);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    Serial.printf("Set PIO sm=%d LoadAdr=%d  pin=%d div=%1.1f\n", sm, offset, pin_base, div);
}

void set_irq(PIO pio, uint sm, float cdiv){
    pio_pin_config(pio, sm, &prog_irq, wrap_irq_top, wrap_irq, CAPTURE_PIN_TDLY, true, false, cdiv);
    irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq0);
    irq_set_enabled(PIO0_IRQ_0, true);
    pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
    pio->txf[sm] = CAPTURE_N_ALL/2;		// トリガー遅延期間
}
void set_dumy_wave(PIO pio, uint sm, int spin, int ddiv){
    pio_pin_config(pio, sm+0, &prog_wave, wrap_wave_top, wrap_wave, CAPTURE_PIN_BASE + spin + 0, true, true, ddiv*128);
    pio_pin_config(pio, sm+1, &prog_wave, wrap_wave_top, wrap_wave, CAPTURE_PIN_BASE + spin + 1, true, true, ddiv*256);
}

void set_dumy_trig(PIO pio, uint pin){
    int freq = 1;
    pio_pin_config(pio0, 2, &prog_tsig, wrap_tsig_top, wrap_tsig, pin, true, true, 1); // Dumyトリガ信号
    pio->txf[2] = 16;								// Hi期間(トリガー信号)
    pio->txf[2] = clock_get_hz(clk_sys) / (2 * 5);	// Lo期間(5Hz)
}

void pio_pin_config(PIO pio, uint sm, const struct pio_program pprog[], 
						uint wrap_top, uint wrap, uint pin, bool pdir, bool smena, float cdiv) {
   pio_gpio_init(pio, pin);
   pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, pdir);
  uint offset = pio_add_program(pio, pprog);
  pio_program_init(pio, sm, offset, wrap_top, wrap, pin, pdir, smena, cdiv);
}
void pio_program_init(PIO pio, uint sm, uint offset, uint wrap_top, uint wrap, uint pin, bool pdir, bool smena, float cdiv) {
   pio_sm_config c = pio_program_get_default_config(offset, wrap_top, wrap);
   sm_config_set_set_pins(&c, pin, 1);
   sm_config_set_clkdiv(&c, cdiv);
   pio_sm_init(pio, sm, offset, &c);
   pio_sm_set_enabled(pio, sm, smena);
  Serial.printf("Set PIO sm=%d LoadAdr=%d  pin=%d div=%1.1f\n", sm, offset, pin, cdiv);
}
static inline pio_sm_config pio_program_get_default_config(uint offset, uint wrap_top, uint wrap ) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + wrap_top, offset + wrap);
    return c;
}
