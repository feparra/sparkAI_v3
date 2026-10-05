#pragma once
#include <Arduino.h>
#include "pin_config.h"

class SparkBuzzer {
private:
  int channel;
public:
  SparkBuzzer(int pwm_channel = 0) : channel(pwm_channel) {}

  void begin() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
  }

  void playTone(uint32_t frequency, uint32_t duration_ms) {
    if (frequency == 0) {
      delay(duration_ms);
      return;
    }
    tone(BUZZER_PIN, frequency, duration_ms);
    delay(duration_ms);
    noTone(BUZZER_PIN);
  }

  // Double beep when agent asks for approval or needs help
  void doubleBeep() {
    playTone(950, 100);
    delay(80);
    playTone(1350, 160);
  }

  // Triple beep + melodic flourish when task is successfully completed
  void tripleBeepDone() {
    playTone(784, 90);   // G5
    delay(50);
    playTone(988, 90);   // B5
    delay(50);
    playTone(1318, 240); // E6
  }

  // Tactile touch / button feedback
  void clickTone() {
    playTone(2400, 15);
  }

  // Welcome chime when agent binds
  void connectChime() {
    playTone(880, 80);
    delay(40);
    playTone(1175, 120);
  }

  // Error alert tone
  void errorAlert() {
    playTone(350, 350);
  }
};
