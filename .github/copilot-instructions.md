# 🎯 槌球遊戲機 AI 開發指南

## 專案概述

這是一個基於 Arduino UNO 的槌球遊戲機系統，集成計時、多點進球偵測、音效播放和 LED 顯示功能。使用 PlatformIO 開發，採用中斷驅動架構確保精確計時與即時響應。

## 🏗️ 核心架構模式

### 中斷驅動設計

-   **Timer1 硬體中斷**: 1 秒精確倒數計時（120 秒遊戲時間），配置在 `setupTimer1()` 中
-   **外部中斷**: 按鈕響應使用 `digitalPinToInterrupt(BUTTON_PIN)` 和 `buttonISR()`
-   **ISR 函數規則**: 中斷服務程序內僅設置 volatile 標誌，實際處理在主循環中進行

### 多狀態機管理

```cpp
enum GameState { IDLE, MUSIC_PLAYING, COUNTING, FINISHED };
volatile GameState current_state = IDLE;
```

狀態轉換流程：`IDLE` → `MUSIC_PLAYING` → `COUNTING` → `FINISHED` → `IDLE`

### 多感測器進球偵測系統

-   **原有感測器**: A1(類比) + 引腳 7(數位)
-   **新增 4 個感測器**: A2-A5(類比) + 引腳 8-11(數位)
-   **防誤判機制**: 每個感測器獨立的 1 秒暫停偵測期 + 200ms 最小間隔

## 🔧 開發工作流程

### 建置與部署

```bash
pio run                    # 編譯專案
pio run --target upload    # 上傳到 Arduino UNO
pio device monitor         # 串列監控 (9600 波特率)
```

### 🚨 人工執行必要指令

**AI 編程助手無法執行硬體相關操作，以下 PlatformIO 指令需要開發者手動執行：**

```bash
# 開發過程必執行指令
pio run --target upload    # 上傳程式到 Arduino（需要硬體連接）
pio device monitor         # 開啟串列監控觀察執行結果
pio run --target clean     # 清理建置快取（解決編譯問題時）

# 偵錯與測試指令
pio device list            # 列出可用的序列埠裝置
pio run --target compiledb # 生成編譯資料庫（IDE 智能提示用）

# 依賴管理指令
pio lib install            # 安裝新的函式庫依賴
pio lib update             # 更新現有函式庫到最新版本
```

**執行時機說明：**

-   程式碼修改後 → 執行 `pio run --target upload` 測試硬體反應
-   感測器讀值異常 → 執行 `pio device monitor` 觀察串列輸出
-   編譯錯誤 → 執行 `pio run --target clean` 後重新編譯
-   新增硬體功能 → 可能需要 `pio lib install` 安裝對應函式庫

### 硬體連接標準

**核心控制**:

-   **按鈕**: 數位引腳 2 (INT0) + GND，使用內建上拉電阻
-   **TM1637**: CLK → 引腳 3, DIO → 引腳 4

**音效系統**:

-   **MP3 模組**: TX → 引腳 5, RX → 引腳 6 (使用 SoftwareSerial)

**進球偵測系統**:

-   **主感測器**: A1(類比) + 引腳 7(數位)
-   **擴展感測器**: A2-A5(類比) + 引腳 8-11(數位)
-   **偵測邏輯**: `ir_digital_value == 0` 表示進球

## 📋 專案特定慣例

### 音效系統控制

```cpp
// MP3 播放控制 - 使用 SoftwareSerial (引腳 5-6)
mp3_start(uint8_t volume, uint8_t song);  // 音量 0-64，歌曲編號
// 預設音效檔案：01=開場音樂, 02=進球音效, 03=結束音效
```

### 計時精度配置

```cpp
// Timer1 配置公式: 16MHz / 1024分頻 = 15625 Hz
OCR1A = 15624;  // 計數到15624 = 1秒精確中斷
const int GAME_TIME = 120;  // 遊戲時間2分鐘
```

### 多點進球偵測模式

```cpp
// 原有感測器 + 4個新增感測器的陣列處理
for (int i = 0; i < 4; i++) {
    ir_digital_values[i] = digitalRead(IR_DIGITAL_PINS[i]);
    // 偵測邏輯：數位輸出 == 0 時表示進球
}
```

### 防抖動標準

所有按鈕輸入使用 200ms 軟體防抖動窗口，結合硬體上拉電阻。

### 顯示格式慣例

-   **閒置狀態**: 12 位置燈光繞圈特效 (`displayRotatingLight()`)
-   **計時期間**: TM1637 顯示當前分數，串列輸出時間與分數
-   **音樂播放**: 顯示 "P-L-A-y" 提示訊息 (`{0x73, 0x38, 0x77, 0x6E}`)
-   **高分顯示**: 分數超過 9999 時顯示 "H-i" (`{0x76, 0x06, 0x00, 0x00}`)

### 變數命名模式

-   **中斷相關**: 使用 `volatile` 關鍵字，如 `volatile bool timer_active`
-   **引腳定義**: `_PIN` 後綴常數，如 `IR_DIGITAL_PINS[4]`
-   **感測器陣列**: 複數形式，如 `ir_digital_values[4]`, `balls_detected[4]`
-   **時間戳變數**: `_until` 後綴，如 `detection_pause_until_new[4]`

## 🔗 依賴管理

### 核心函式庫

-   **TM1637Display**: 來自 GitHub `avishorp/TM1637.git`，用於 4 位數 7 段顯示器
-   **Arduino.h**: 標準 Arduino 框架

### Timer1 暫存器操作

直接操作 AVR 暫存器：`TCCR1A`, `TCCR1B`, `TCNT1`, `OCR1A`, `TIMSK1`

## 🚨 常見陷阱

### 中斷安全性

-   避免在 ISR 中使用 `Serial.print()` 或 `delay()`
-   所有中斷共用變數必須宣告為 `volatile`
-   使用 `noInterrupts()`/`interrupts()` 保護關鍵區段

### 顯示器時序

-   TM1637 需要適當的 CLK/DIO 時序，避免在中斷中直接操作
-   使用標誌延遲更新到主循環中執行

### 記憶體考量

Arduino UNO 僅 2KB SRAM，避免大型陣列或遞迴，優先使用常數和 volatile 變數。

## 🔄 擴展指南

### 新增硬體元件

1. 在引腳定義區塊添加常數
2. 在 `setup()` 中初始化
3. 遵循中斷驅動模式避免阻塞

### 新增遊戲模式

1. 擴展 `GameState` 枚舉 (當前: IDLE, MUSIC_PLAYING, COUNTING, FINISHED)
2. 在 `displayCountdown()` 中添加新狀態處理
3. 更新狀態轉換邏輯和時序控制

### 調試最佳實踐

-   使用 `pio device monitor` 觀察串列輸出
-   關鍵變數狀態列印在主循環中，非中斷內
-   硬體問題優先檢查線路連接和電源
-   **重要**: AI 助手只能修改程式碼，硬體測試需人工執行 `pio run --target upload`
