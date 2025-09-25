#include <Arduino.h>
#include <TM1637Display.h>
#include <SoftwareSerial.h>

// 引腳定義
const int BUTTON_PIN = 2; // 按鈕連接到數位引腳2 (INT0)
const int CLK_PIN = 3;    // TM1637 CLK 引腳
const int DIO_PIN = 4;    // TM1637 DIO 引腳

// MP3 模組引腳定義 (使用 SoftwareSerial)
const int MP3_RX_PIN = 5; // MP3 模組 TX 連接到 Arduino 引腳 7
const int MP3_TX_PIN = 6; // MP3 模組 RX 連接到 Arduino 引腳 8

// TCRT5000 IR 感測器引腳（總共5個感測器）
const int IR_ANALOG_PINS[5] = {A1, A2, A3, A4, A5}; // 所有感測器類比輸出引腳
const int IR_DIGITAL_PINS[5] = {7, 8, 9, 10, 11};   // 所有感測器數位輸出引腳

// TM1637 顯示器物件
TM1637Display display(CLK_PIN, DIO_PIN);

// MP3 模組物件
SoftwareSerial mp3Serial(MP3_RX_PIN, MP3_TX_PIN);

// 計時器相關變數
volatile int countdown_time = 0;      // 倒數時間（秒）
volatile bool timer_active = false;   // 計時器是否啟動
volatile bool display_update = false; // 顯示更新標誌

const int GAME_TIME = 120; // 遊戲時間（秒）- 1分30秒

// 分數和 IR 感測器相關變數
volatile int score = 0; // 進球分數

// 所有感測器讀值陣列（5個感測器）
int ir_analog_values[5] = {0};                // 5個IR 感測器類比數值
int ir_digital_values[5] = {0};               // 5個IR 感測器數位數值
volatile bool balls_detected[5] = {false};    // 5個球體偵測標誌
unsigned long detection_pause_until[5] = {0}; // 5個暫停偵測直到此時間點

// 狀態枚舉
enum GameState
{
  IDLE,          // 閒置狀態
  MUSIC_PLAYING, // 音樂播放中
  COUNTING,      // 計時中
  FINISHED       // 計時結束
};

volatile GameState current_state = IDLE;

// 音樂播放相關變數
unsigned long music_start_time = 0;           // 音樂開始播放時間
const unsigned long MUSIC_01_DURATION = 3000; // 音樂01播放時長(毫秒) - 可根據實際音樂長度調整
volatile bool music_finished = false;         // 音樂播放完成標誌

// 燈光特效相關變數
unsigned long last_light_update = 0;   // 上次燈光更新時間
const unsigned long LIGHT_DELAY = 100; // 燈光切換間隔(毫秒)
int light_position = 0;                // 目前燈光位置 (0-11, 外圈一圈12個位置)

// 函數宣告
void setupTimer1();
void startCountdown();
void stopCountdown();
void buttonISR();
void displayCountdown();
void displayRotatingLight(); // 顯示燈光繞圈特效
void checkBallDetection();   // 檢查進球偵測

// MP3 播放控制函數
void mp3_initial();
void mp3_start(uint8_t volume = 0x64, uint8_t song = 0x01);
void mp3_stop();

void setup()
{
  // 初始化串列通信
  Serial.begin(9600);
  Serial.println("=== 槌球遊戲機倒數計時器 ===");
  Serial.println("按下按鈕開始倒數計時！");

  // 設定按鈕引腳
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 設定所有 IR 感測器引腳
  for (int i = 0; i < 5; i++)
  {
    pinMode(IR_DIGITAL_PINS[i], INPUT); // 數位輸出引腳
    // 類比引腳 (A1-A5) 不需要 pinMode 設定
  }

  // 設定外部中斷
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, RISING);

  // 設定Timer1中斷
  setupTimer1();

  // 初始化 TM1637 顯示器
  display.setBrightness(0x0a); // 設定亮度 (0-15)
  display.clear();

  // 初始化 MP3 模組
  mp3_initial();
  Serial.println("MP3 模組已初始化");

  Serial.println("系統就緒，等待按鈕按下...");
}

void loop()
{
  // 讀取所有 5 個 IR 感測器數值
  for (int i = 0; i < 5; i++)
  {
    ir_analog_values[i] = analogRead(IR_ANALOG_PINS[i]);    // 讀取類比數值 (0-1023)
    ir_digital_values[i] = digitalRead(IR_DIGITAL_PINS[i]); // 讀取數位數值 (0 或 1)
  }

  // 在閒置狀態顯示燈光繞圈特效
  if (current_state == IDLE)
  {
    displayRotatingLight();
  }

  // 檢查音樂播放完成（僅在 MUSIC_PLAYING 狀態）
  if (current_state == MUSIC_PLAYING)
  {
    if (millis() - music_start_time >= MUSIC_01_DURATION)
    {
      music_finished = true;
      // 音樂播放完成，開始倒數計時
      countdown_time = GAME_TIME;
      timer_active = true;
      current_state = COUNTING;
      display_update = true;

      Serial.println("開場音樂播放完成，開始倒數計時！");

      // 立即顯示初始倒數時間 (分:秒格式)
      int minutes = countdown_time / 60;
      int seconds = countdown_time % 60;
      int display_value = minutes * 100 + seconds;                    // MMSS 格式
      display.showNumberDecEx(display_value, 0b01000000, true, 4, 0); // 顯示冒號
    }
  }

  // 檢查是否需要更新顯示
  if (display_update)
  {
    display_update = false;
    displayCountdown();
  }

  // 檢查進球偵測（僅在計時期間）
  if (current_state == COUNTING)
  {
    checkBallDetection();
  }

  // 檢查計時是否結束
  if (current_state == FINISHED)
  {
    // 延遲後回到準備狀態
    delay(2000);

    current_state = IDLE;
    // 重置燈光特效變數
    light_position = 0;
    last_light_update = millis();
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

// 開始倍數計時
void startCountdown()
{
  // 重置遊戲狀態
  score = 0;              // 重置分數
  music_finished = false; // 重置音樂完成標誌

  // 重置所有感測器狀態
  for (int i = 0; i < 5; i++)
  {
    detection_pause_until[i] = 0; // 重置暫停偵測時間
    balls_detected[i] = false;    // 重置偵測標誌
  }

  // 設定為音樂播放狀態
  current_state = MUSIC_PLAYING;
  music_start_time = millis();

  // 播放音樂01，音量80% (約128/255)
  mp3_start(50, 1); // 80% 音量約為 51 (80% of 64)
  Serial.println("開始播放開場音樂(01)！");

  // 顯示播放音樂提示
  uint8_t play_msg[] = {0x73, 0x38, 0x77, 0x6E}; // P-L-A-y
  display.setSegments(play_msg);
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
    // 計算分鐘和秒數（僅用於串列監視器）
    int minutes = countdown_time / 60;
    int seconds = countdown_time % 60;

    // 串列監視器顯示
    Serial.print("倒數計時: ");
    Serial.print(minutes);
    Serial.print(":");
    if (seconds < 10)
      Serial.print("0"); // 補零
    Serial.print(seconds);
    Serial.print(" | 分數: ");
    Serial.println(score);

    // 在 TM1637 顯示器上只顯示分數（最多 4 位數）
    if (score <= 9999)
    {
      display.showNumberDec(score, false, 4, 0); // 顯示分數，不顯示前導零
    }
    else
    {
      // 如果分數超過 4 位數，顯示 "Hi" (高分)
      uint8_t high_score[] = {0x76, 0x06, 0x00, 0x00}; // H-i--
      display.setSegments(high_score);
    }
  }
  break;

  case FINISHED:
  {
    static bool end_sound_played = false; // 確保結束音效只播放一次

    if (!end_sound_played)
    {
      // 播放結束音效03 (100%音量)
      mp3_start(64, 3); // 100% 音量，播放第三首歌
      Serial.println("遊戲結束！播放結束音效03");
      end_sound_played = true;
    }

    // 直接顯示最終分數（最多 4 位數）
    if (score <= 9999)
    {
      display.showNumberDec(score, false, 4, 0); // 顯示分數
      Serial.print("最終分數: ");
      Serial.println(score);
    }
    else
    {
      // 如果分數超過 4 位數，顯示 "Hi" (高分)
      uint8_t high_score[] = {0x76, 0x06, 0x00, 0x00}; // H-i--
      display.setSegments(high_score);
      Serial.println("恭喜！獲得超高分數！");
    }

    // 重置結束音效標誌，準備下一輪
    end_sound_played = false;
  }
  break;

  case IDLE:
  case MUSIC_PLAYING:
  default:
    // 閒置狀態和音樂播放狀態不需要特別處理倒數顯示
    break;
  }
}

// 檢查進球偵測（支援5個感測器的統一處理）
void checkBallDetection()
{
  static unsigned long last_detection = 0; // 上次偵測時間

  // 統一處理所有5個感測器
  for (int i = 0; i < 5; i++)
  {
    if (millis() >= detection_pause_until[i])
    {
      bool ball_present = (ir_digital_values[i] == 0); // 進球偵測邏輯：數位 = 0

      if (ball_present && !balls_detected[i])
      {
        // 防誤判：確保間隔至少 200ms
        if (millis() - last_detection > 200)
        {
          balls_detected[i] = true;
          score++;
          last_detection = millis();

          // 設定暫停偵測 1 秒
          detection_pause_until[i] = millis() + 1000;

          // 播放進球音效02 (100%音量)
          mp3_start(64, 2); // 100% 音量，播放第二首歌
          Serial.print("進球！第");
          Serial.print(i + 1);
          Serial.print("號進球點！播放音效02，目前分數: ");
          Serial.println(score);
        }
      }

      // 重置偵測標誌（當感測器數值回到正常範圍）
      if (!ball_present && millis() >= detection_pause_until[i])
      {
        balls_detected[i] = false;
      }
    }
  }
}

// MP3 模組控制函數實現

// 初始化 MP3 模組
void mp3_initial()
{
  mp3Serial.begin(9600);
  delay(500); // 等待 MP3 模組初始化
}

// 開始播放指定歌曲（音量範圍：0-64，對應0%-100%）
void mp3_start(uint8_t volume, uint8_t song)
{
  // 確保音量在有效範圍內
  if (volume > 64)
    volume = 64;

  // 音量控制命令 (13)
  // 命令格式: AA 13 01 VOL SM
  uint8_t volumeCmd[5];
  volumeCmd[0] = 0xAA;
  volumeCmd[1] = 0x13;
  volumeCmd[2] = 0x01;
  volumeCmd[3] = volume;
  volumeCmd[4] = volumeCmd[0] + volumeCmd[1] + volumeCmd[2] + volumeCmd[3]; // 檢查碼

  for (int i = 0; i < 5; i++)
  {
    mp3Serial.write(volumeCmd[i]);
  }

  delay(100); // 短暫延遲

  // 指定歌曲播放命令 (07)
  // 命令格式: AA 07 02 filename(Hi) filename(Lw) SM
  uint8_t playCmd[6];
  playCmd[0] = 0xAA;
  playCmd[1] = 0x07;
  playCmd[2] = 0x02;
  playCmd[3] = 0x00;                                                           // 音樂檔案開頭名稱的16進制
  playCmd[4] = song;                                                           // 音樂檔案結尾名稱的16進制
  playCmd[5] = playCmd[0] + playCmd[1] + playCmd[2] + playCmd[3] + playCmd[4]; // 檢查碼

  for (int i = 0; i < 6; i++)
  {
    mp3Serial.write(playCmd[i]);
  }

  // 記錄播放資訊
  Serial.print("MP3播放：音量 ");
  Serial.print((volume * 100) / 64);
  Serial.print("% | 歌曲 ");
  Serial.println(song);
}

// 停止播放
void mp3_stop()
{
  // 停止播放命令 (04)
  // 命令格式: AA 04 00 AE
  uint8_t stopCmd[4];
  stopCmd[0] = 0xAA;
  stopCmd[1] = 0x04;
  stopCmd[2] = 0x00;
  stopCmd[3] = 0xAE; // 固定檢查碼

  for (int i = 0; i < 4; i++)
  {
    mp3Serial.write(stopCmd[i]);
  }

  Serial.println("MP3播放停止");
}

// 顯示燈光繞圈特效（閒置狀態使用）
void displayRotatingLight()
{
  // 檢查是否到了更新燈光的時間
  if (millis() - last_light_update >= LIGHT_DELAY)
  {
    // 定義整個四位數字顯示器外圈的燈光路徑（純順時鐘）
    // 路徑：頂部從左到右 -> 右側從上到下 -> 底部從右到左 -> 左側從下到上
    uint8_t ring_patterns[12][4] = {
        // 頂部橫段（從左到右：位置0-3）
        {0x01, 0x00, 0x00, 0x00}, // 位置0: 第1位上段
        {0x00, 0x01, 0x00, 0x00}, // 位置1: 第2位上段
        {0x00, 0x00, 0x01, 0x00}, // 位置2: 第3位上段
        {0x00, 0x00, 0x00, 0x01}, // 位置3: 第4位上段

        // 右側豎段（從上到下：位置4-5）
        {0x00, 0x00, 0x00, 0x02}, // 位置4: 第4位右上豎
        {0x00, 0x00, 0x00, 0x04}, // 位置5: 第4位右下豎

        // 底部橫段（從右到左：位置6-9）
        {0x00, 0x00, 0x00, 0x08}, // 位置6: 第4位下段
        {0x00, 0x00, 0x08, 0x00}, // 位置7: 第3位下段
        {0x00, 0x08, 0x00, 0x00}, // 位置8: 第2位下段
        {0x08, 0x00, 0x00, 0x00}, // 位置9: 第1位下段

        // 左側豎段（從下到上：位置10-11）
        {0x10, 0x00, 0x00, 0x00}, // 位置10: 第1位左下豎
        {0x20, 0x00, 0x00, 0x00}  // 位置11: 第1位左上豎
    };

    // 顯示目前燈光位置
    display.setSegments(ring_patterns[light_position]);

    // 移動到下一個位置 (0-11 循環，完成一個完整的順時鐘圈)
    light_position = (light_position + 1) % 12;

    // 更新時間
    last_light_update = millis();
  }
}