#include <Arduino.h>
#include <TM1637Display.h>

// 引腳定義
const int BUTTON_PIN = 2; // 按鈕連接到數位引腳2 (INT0)
const int CLK_PIN = 3;    // TM1637 CLK 引腳
const int DIO_PIN = 4;    // TM1637 DIO 引腳

// TM1637 顯示器物件
TM1637Display display(CLK_PIN, DIO_PIN);

// 計時器相關變數
volatile int countdown_time = 0;      // 倒數時間（秒）
volatile bool timer_active = false;   // 計時器是否啟動
volatile bool display_update = false; // 顯示更新標誌

const int GAME_TIME = 90; // 遊戲時間（秒）- 1分30秒

// 狀態枚舉
enum GameState
{
  IDLE,     // 閒置狀態
  COUNTING, // 計時中
  FINISHED  // 計時結束
};

volatile GameState current_state = IDLE;

// 函數宣告
void setupTimer1();
void startCountdown();
void stopCountdown();
void buttonISR();
void displayCountdown();

void setup()
{
  // 初始化串列通信
  Serial.begin(9600);
  Serial.println("=== 槌球遊戲機倒數計時器 ===");
  Serial.println("按下按鈕開始倒數計時！");

  // 設定按鈕引腳
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 設定外部中斷
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, RISING);

  // 設定Timer1中斷
  setupTimer1();

  // 初始化 TM1637 顯示器
  display.setBrightness(0x0a); // 設定亮度 (0-15)
  display.clear();

  // 顯示初始提示 "rEAd" (準備)
  uint8_t ready[] = {0x50, 0x7C, 0x77, 0x5E}; // r-E-A-d
  display.setSegments(ready);

  Serial.println("系統就緒，等待按鈕按下...");
}

void loop()
{
  // 檢查是否需要更新顯示
  if (display_update)
  {
    display_update = false;
    displayCountdown();
  }

  // 檢查計時是否結束
  if (current_state == FINISHED)
  {
    Serial.println("=== 遊戲時間結束！===");
    Serial.println("按下按鈕開始新的倒數計時！");

    // 延遲後回到準備狀態
    delay(2000);
    uint8_t ready[] = {0x50, 0x7C, 0x77, 0x5E}; // r-E-A-d
    display.setSegments(ready);

    current_state = IDLE;
  }

  // 主循環可以在這裡添加其他非時間關鍵的功能
  delay(10); // 避免過度消耗CPU
}

// Timer1中斷服務程序
ISR(TIMER1_COMPA_vect)
{
  if (timer_active && countdown_time > 0)
  {
    countdown_time--;
    display_update = true;

    // 檢查是否計時結束
    if (countdown_time <= 0)
    {
      timer_active = false;
      current_state = FINISHED;
    }
  }
}

// 按鈕中斷服務程序
void buttonISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  // 簡單的按鈕防抖動（200ms）
  if (interrupt_time - last_interrupt_time > 200)
  {
    if (current_state == IDLE)
    {
      startCountdown();
    }
  }
  last_interrupt_time = interrupt_time;
}

// 設定Timer1為1秒中斷
void setupTimer1()
{
  // 停用全域中斷
  noInterrupts();

  // 清除Timer1控制暫存器
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  // 設定比較值（16MHz / 1024 預分頻 = 15625 Hz，計數到15625 = 1秒）
  OCR1A = 15624; // (16*10^6) / (1024*1) - 1 = 15624

  // 開啟CTC模式
  TCCR1B |= (1 << WGM12);

  // 設定1024預分頻
  TCCR1B |= (1 << CS12) | (1 << CS10);

  // 開啟Timer1比較中斷
  TIMSK1 |= (1 << OCIE1A);

  // 恢復全域中斷
  interrupts();
}

// 開始倒數計時
void startCountdown()
{
  countdown_time = GAME_TIME;
  timer_active = true;
  current_state = COUNTING;
  display_update = true;

  // 立即顯示初始倒數時間 (分:秒格式)
  int minutes = countdown_time / 60;
  int seconds = countdown_time % 60;
  int display_value = minutes * 100 + seconds;                    // MMSS 格式
  display.showNumberDecEx(display_value, 0b01000000, true, 4, 0); // 顯示冒號

  Serial.println("\n=== 開始倒數計時！===");
}

// 停止倒數計時
void stopCountdown()
{
  timer_active = false;
  current_state = IDLE;
  countdown_time = 0;
}

// 顯示倒數計時
void displayCountdown()
{
  switch (current_state)
  {
  case COUNTING:
  {
    // 計算分鐘和秒數
    int minutes = countdown_time / 60;
    int seconds = countdown_time % 60;

    // 串列監視器顯示
    Serial.print("倒數計時: ");
    Serial.print(minutes);
    Serial.print(":");
    if (seconds < 10)
      Serial.print("0"); // 補零
    Serial.print(seconds);
    Serial.println("");

    // 在 TM1637 顯示器上以 MM:SS 格式顯示
    int display_value = minutes * 100 + seconds;                    // MMSS 格式
    display.showNumberDecEx(display_value, 0b01000000, true, 4, 0); // 顯示冒號
  }
  break;

  case FINISHED:
    Serial.println("倒數計時: 0:00");

    // 顯示 "0:00" 然後顯示 "End" 結束訊息
    display.showNumberDecEx(0, 0b01000000, true, 4, 0); // 顯示 0:00
    delay(1000);
    {
      uint8_t end_msg[] = {0x79, 0x54, 0x5E, 0x00}; // E-n-d-空白
      display.setSegments(end_msg);
    }
    break;

  case IDLE:
  default:
    // 閒置狀態不需要特別處理倒數顯示
    break;
  }
}