#pragma once

// Waveshare ESP32-S3-Touch-LCD-1.69 Hardware Pinout

// ST7789V2 SPI Display (240x280)
#define LCD_WIDTH   240
#define LCD_HEIGHT  280
#define LCD_DC      4
#define LCD_CS      5
#define LCD_SCK     6
#define LCD_MOSI    7
#define LCD_RST     8
#define LCD_BL      15

// CST816T I2C Capacitive Touch Screen
#define TP_SDA      11
#define TP_SCL      10
#define TP_RST      13
#define TP_INT      14
#define TP_I2C_ADDR 0x15

// Power Management & Latch
#define SYS_EN_PIN  41  // MUST drive HIGH to latch power on

// Passive Buzzer PWM (LEDC)
#define BUZZER_PIN  42

// Hardware Buttons (Active LOW)
#define BTN_BOOT    0   // Hardware BOOT button -> Instant APPROVE
#define BTN_PWR     40  // Hardware PWR button  -> Instant REJECT/CANCEL

// Battery Voltage ADC
#define BAT_ADC_PIN 1   // 1:3 divider (R3=200K, R7=100K)

// Sensors on I2C bus (SDA=11, SCL=10)
#define QMI8658_ADDR 0x6B
#define PCF85063_ADDR 0x51
