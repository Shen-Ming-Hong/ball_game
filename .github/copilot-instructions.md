# 🎯 槌球遊戲機 AI 開發指南

## 專案概述

這是一個基於 Arduino UNO 的槌球遊戲計時系統，使用 PlatformIO 開發，採用中斷驅動架構確保精確計時。

## 🏗️ 核心架構模式

### 中斷驅動設計

-   **Timer1 硬體中斷**: 1 秒精確倒數計時，配置在 `setupTimer1()` 中
-   **外部中斷**: 按鈕響應使用 `digitalPinToInterrupt(BUTTON_PIN)` 和 `buttonISR()`
-   **ISR 函數規則**: 中斷服務程序內僅設置 volatile 標誌，實際處理在主循環中進行

### 狀態管理模式

```cpp
enum GameState { IDLE, COUNTING, FINISHED };
volatile GameState current_state = IDLE;
```

所有狀態變更都通過中斷觸發，主循環根據狀態執行對應操作。

### 顯示更新模式

使用標誌驅動的非阻塞更新：`volatile bool display_update = false;`

## 🔧 開發工作流程

### 建置與部署

```bash
pio run                    # 編譯
pio run --target upload    # 上傳到 Arduino
pio device monitor         # 串列監控 (9600 波特率)
```

### 硬體連接標準

-   **按鈕**: 數位引腳 2 (INT0) + GND，使用內建上拉電阻
-   **TM1637**: CLK → 引腳 3, DIO → 引腳 4
-   **電源**: 透過 USB 或外部 5V 供電

## 📋 專案特定慣例

### 計時精度配置

```cpp
// Timer1 配置公式: 16MHz / 1024分頻 = 15625 Hz
OCR1A = 15624;  // 計數到15624 = 1秒精確中斷
```

### 防抖動標準

所有按鈕輸入使用 200ms 軟體防抖動窗口，結合硬體上拉電阻。

### 顯示格式慣例

-   **時間顯示**: MM:SS 格式使用 `showNumberDecEx()`
-   **狀態提示**: 自定義 7 段顯示碼 (如 "rEAd" = `{0x50, 0x79, 0x77, 0x5E}`)
-   **串列輸出**: 中文提示 + 英文狀態，9600 波特率

### 變數命名模式

-   **中斷相關**: 使用 `volatile` 關鍵字
-   **引腳定義**: `_PIN` 後綴常數
-   **狀態標誌**: `_active`, `_update` 後綴

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

1. 擴展 `GameState` 枚舉
2. 在 `displayCountdown()` 中添加新狀態處理
3. 更新狀態轉換邏輯

### 調試最佳實踐

-   使用 `pio device monitor` 觀察串列輸出
-   關鍵變數狀態列印在主循環中，非中斷內
-   硬體問題優先檢查線路連接和電源
