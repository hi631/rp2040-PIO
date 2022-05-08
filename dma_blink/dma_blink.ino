#include <hardware/pio.h>
#include "hardware/dma.h"
#include "hardware/irq.h"
#define PIO_SERIAL_CLKDIV 65535.f
#define PWM_REPEAT_COUNT 1
#define N_PWM_LEVELS 32

// Our assembled program:
#include "pio_serialiser.pio.h"

#define LED_RED		17
#define LED_GREEN	16
#define LED_BLUE	25

void setup() {
  init_serial();
  clear_rgb();
  cmain();
}
void loop() { }

void init_serial(){
  Serial.begin(115200);
  int wcnt=100;
  while(true){
  	if(Serial) {Serial.printf("COM.Connect\n"); break;}
  	if((wcnt--)==0) break;
  	delay(10);
  }
}

void clear_rgb(){
  pinMode(LED_RED, OUTPUT); digitalWrite(LED_RED, HIGH);
  pinMode(LED_GREEN, OUTPUT); digitalWrite(LED_GREEN, HIGH);
  pinMode(LED_BLUE, OUTPUT); digitalWrite(LED_BLUE, HIGH);
}

//　チャネルの完了時にチャネルを再構成して再起動する方法
//　DMAチャネルは、データをPIOステートマシンに転送します。
//　プッシュする生のビットを1つずつシリアル化するように構成されています。
//　これを使用して、値を繰り返し送信することにより、粗いLEDPWMを実行します
//
//　チャネルが所定の量のデータを送信すると、チャネルは停止し、
//　割り込みフラグを立てます。プロセッサは、割り込みハンドラに入ります。
//　これに応答し、チャネルを再構成して再起動します。これを繰り返します。

int dma_chan;
void dma_handler() {
    static int pwm_level = 0;
    static uint32_t wavetable[N_PWM_LEVELS];
    static bool first_run = true;
    //エントリ番号`i`は`i`が1ビットで`（32 --i）`が0ビットです。
    if (first_run) {
        first_run = false;
        for (int i = 0; i < N_PWM_LEVELS; ++i) wavetable[i] = ~(~0u << i);
    }
    //割り込み要求をクリアします。
    dma_hw->ints0 = 1u << dma_chan;
    //チャネルに読み取り元の新しいウェーブテーブルエントリを与え、それを再トリガーします
    dma_channel_set_read_addr(dma_chan, &wavetable[pwm_level], true);
    pwm_level = (pwm_level + 1) % N_PWM_LEVELS;
}

int cmain() {
    //ビットをシリアル化するようにPIOステートマシンを設定します
    uint offset = pio_add_program(pio0, &pio_serialiser_program);
    pio_serialiser_program_init(pio0, 0, offset, LED_GREEN, PIO_SERIAL_CLKDIV);

    //同じワード（32ビット）をPIO0に繰り返し書き込むようにチャネルを構成します
    //SM0のTXFIFO、そのペリフェラルからのデータ要求信号によってペースが調整されます。
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_PIO0_TX0);

    dma_channel_configure(
        dma_chan,
        &c,
        &pio0_hw->txf[0], //アドレスを書き込みます（これは1回だけ設定する必要があります）
        NULL,             //読み取りアドレスをまだ提供しない
        PWM_REPEAT_COUNT, //同じ値を何度も書き込んでから、停止して割り込みます
        false             //まだ開始しない
    );

    //チャネルがブロックを終了したときにIRQライン0を上げるようにDMAに指示します
    dma_channel_set_irq0_enabled(dma_chan, true);

    // DMA IRQ 0がアサートされたときにdma_handler（）を実行するようにプロセッサを構成します
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    //ハンドラーを手動で一度呼び出して、最初の転送をトリガーします
    dma_handler();

    //この時点以降、他のすべては割り込み駆動型です。プロセッサには
    //座って、その早期引退について考える時間-多分パン屋を開く？
    while (true) {
    	loop(); tight_loop_contents();
    }
}
