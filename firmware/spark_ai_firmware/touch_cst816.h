#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "pin_config.h"

struct TouchPoint {
  bool touched;
  uint16_t x;
  uint16_t y;
  uint8_t gesture;
};

class CST816TTouch {
public:
  bool begin() {
    pinMode(TP_INT, INPUT);
    pinMode(TP_RST, OUTPUT);
    digitalWrite(TP_RST, LOW);
    delay(20);
    digitalWrite(TP_RST, HIGH);
    delay(50);

    Wire.begin(TP_SDA, TP_SCL, 400000);
    Wire.beginTransmission(TP_I2C_ADDR);
    return (Wire.endTransmission() == 0);
  }

  TouchPoint read() {
    TouchPoint pt = { false, 0, 0, 0 };
    // Only query I2C bus when touch controller asserts INT pin (Active LOW)
    if (digitalRead(TP_INT) != LOW) return pt;

    Wire.beginTransmission(TP_I2C_ADDR);
    Wire.write(0x01); // Start reading from gesture register
    if (Wire.endTransmission(false) != 0) return pt;

    uint8_t buf[6];
    if (Wire.requestFrom((uint8_t)TP_I2C_ADDR, (uint8_t)6) != 6) return pt;

    for (int i = 0; i < 6; i++) {
      buf[i] = Wire.read();
    }

    uint8_t finger_num = buf[1] & 0x0F;
    if (finger_num == 0) return pt;

    pt.touched = true;
    pt.gesture = buf[0];
    uint16_t raw_x = ((buf[2] & 0x0F) << 8) | buf[3];
    uint16_t raw_y = ((buf[4] & 0x0F) << 8) | buf[5];

    // Hardware mirror calibration (Waveshare 1.69" 240x280)
    if (raw_x < LCD_WIDTH) {
      pt.x = (LCD_WIDTH - 1) - raw_x;
    } else {
      pt.x = 0;
    }

    if (raw_y < LCD_HEIGHT) {
      pt.y = (LCD_HEIGHT - 1) - raw_y;
    } else {
      pt.y = 0;
    }

    return pt;
  }
};
