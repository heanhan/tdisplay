
#ifndef __WIRING_H__
#define __WIRING_H__

// ===== 板型选择：只需要改这一处 =====
// ESP32_DEVKITC : ESP32-DevKitC-1 / 兼容板（WROVER 模组），板载蓝灯在 IO2，高电平点亮
// LOLIN32_LITE  : WeMos LOLIN32 Lite，板载红灯在 IO22，低电平点亮
// 两种板子的 SPI、按键、ADC 接线完全一致，只有 LED 不同。
#define ESP32_DEVKITC
//#define LOLIN32_LITE

// Output PIN
#define SPI_MOSI GPIO_NUM_23
#define SPI_MISO GPIO_NUM_19 // Reserved
#define SPI_SCK GPIO_NUM_18
#define SPI_CS GPIO_NUM_5
#define SPI_DC GPIO_NUM_17
#define SPI_RST GPIO_NUM_16
#define SPI_BUSY GPIO_NUM_4
// I2C
#define I2C_SDA GPIO_NUM_21
#define I2C_SCL GPIO_NUM_22
// Other PIN
#define KEY_M GPIO_NUM_14 // 注意：由于此按键负责唤醒，因此需要选择支持RTC唤醒的PIN脚。

#define PIN_ADC GPIO_NUM_32 // ADC

// ===== 电量检测开关 =====
// 1：启用。已接好 2×500K 分压电路并用锂电池供电时使用，低于 3.0V 立即休眠以保护电芯。
// 0：关闭。只插 USB 台架调试时必须关闭 —— GPIO32 悬空会读到约 3.0V 的假电压，
//      恰好命中「低电警告并休眠」分支，导致开机即睡、无法完成配网。
#define BATTERY_CHECK 0

#ifdef ESP32_DEVKITC
#define PIN_LED_R GPIO_NUM_2
#define LED_ON_LEVEL HIGH
#define LED_OFF_LEVEL LOW
#endif

#ifdef LOLIN32_LITE
#define PIN_LED_R GPIO_NUM_22
#define LED_ON_LEVEL LOW
#define LED_OFF_LEVEL HIGH
#endif

#endif
