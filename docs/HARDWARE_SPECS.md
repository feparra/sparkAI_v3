# Waveshare ESP32-S3-Touch-LCD-1.69 Hardware Specifications

| Component | Specification | GPIO Pin | Notes |
|---|---|---|---|
| **MCU** | ESP32-S3R8 Dual-core LX7 @ 240MHz | - | 16MB Flash, 8MB Octal PSRAM |
| **Display Controller** | ST7789V2 SPI | - | 240 x 280 resolution, 262K colors |
| **LCD DC** | Data / Command Select | `GPIO 4` | Fast SPI GPIO |
| **LCD CS** | Chip Select | `GPIO 5` | Active LOW |
| **LCD SCLK** | SPI Clock | `GPIO 6` | Up to 80 MHz |
| **LCD MOSI** | SPI Master Out | `GPIO 7` | Data line |
| **LCD RST** | Hardware Reset | `GPIO 8` | Active LOW |
| **LCD BL** | Backlight PWM | `GPIO 15` | HIGH = On, PWM for dimming |
| **Touch Controller** | CST816T Capacitive Touch | `GPIO 11 (SDA), GPIO 10 (SCL)` | I2C address `0x15`, mirrored coordinates |
| **Touch RST / INT** | Reset & Interrupt | `GPIO 13 (RST), GPIO 14 (INT)` | - |
| **Buzzer** | Passive Piezo Buzzer | `GPIO 42` | PWM Tone Generator (LEDC) |
| **Power Latch** | System Power Enable | `GPIO 41` | MUST drive HIGH immediately on boot |
| **BOOT Button** | Hardware Pushbutton | `GPIO 0` | Active LOW (Used for instant Approve) |
| **PWR Button** | Power / Secondary Button | `GPIO 40` | Active LOW (Used for instant Deny) |
| **Battery Voltage** | Voltage ADC Sense | `GPIO 1` | 1:3 divider (R3=200K / R7=100K) |
| **6-Axis IMU** | QMI8658C Accelerometer/Gyro | `I2C 0x6B` | SDA=11, SCL=10 |
| **RTC** | PCF85063 Real-Time Clock | `I2C 0x51` | SDA=11, SCL=10 |
