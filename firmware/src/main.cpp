/**
 * SparkAI V3 Firmware for Waveshare ESP32-S3-Touch-LCD-1.69
 * Companion Screen with Capy Animations, Double/Triple Buzzer Tones,
 * Touch and Physical Button Approvals.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>

#define LV_CONF_INCLUDE_SIMPLE 
#include <lvgl.h>
#include "Arduino_GFX_Library.h"

#include "pin_config.h"
#include "buzzer.h"
#include "touch_cst816.h"

// Hardware Bus & Display Driver
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 0, true, LCD_WIDTH, LCD_HEIGHT, 0, 20, 0, 0);

// Peripherals
SparkBuzzer buzzer;
CST816TTouch touch;

// LVGL Draw Buffer
static const uint32_t screenWidth  = 240;
static const uint32_t screenHeight = 280;
#define DRAW_BUF_SIZE (screenWidth * 40)
static lv_color_t buf[DRAW_BUF_SIZE];

// Active State & UI Objects
lv_obj_t *character_gif_obj;
lv_obj_t *agent_badge_label;
lv_obj_t *status_label;
lv_obj_t *approval_panel = NULL;
lv_obj_t *btn_approve = NULL;
lv_obj_t *btn_deny = NULL;

String currentState = "calm";
String currentAgent = "SparkAI";
String currentCharacter = "capy";
String currentApprovalId = "";
uint32_t lastTick = 0;

// LittleFS to LVGL File System Bridge
static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
  const char * flags = (mode == LV_FS_MODE_WR) ? "w" : "r";
  String fullPath = "/" + String(path);
  File f = LittleFS.open(fullPath, flags);
  if (!f) return NULL;
  return (void *)(new File(f));
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p) {
  File * fp = (File *)file_p;
  fp->close();
  delete fp;
  return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
  File * fp = (File *)file_p;
  *br = fp->read((uint8_t *)buf, btr);
  return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
  File * fp = (File *)file_p;
  SeekMode mode = (whence == LV_FS_SEEK_SET) ? SeekSet : (whence == LV_FS_SEEK_CUR) ? SeekCur : SeekEnd;
  fp->seek(pos, mode);
  return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
  File * fp = (File *)file_p;
  *pos_p = fp->position();
  return LV_FS_RES_OK;
}

void init_littlefs_for_lvgl() {
  static lv_fs_drv_t fs_drv;
  lv_fs_drv_init(&fs_drv);
  fs_drv.letter = 'L';
  fs_drv.open_cb = fs_open;
  fs_drv.close_cb = fs_close;
  fs_drv.read_cb = fs_read;
  fs_drv.seek_cb = fs_seek;
  fs_drv.tell_cb = fs_tell;
  lv_fs_drv_register(&fs_drv);
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t * px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_display_flush_ready(disp);
}

void updateCharacterAnimation(const String& state) {
  String gifPath = "L:" + state + ".gif";
  if (LittleFS.exists("/" + state + ".gif")) {
    lv_gif_set_src(character_gif_obj, gifPath.c_str());
  }
}

void hideApprovalPanel() {
  if (approval_panel) {
    lv_obj_add_flag(approval_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

void showApprovalDialog(const String& id, const String& agent, const String& question, const String& details) {
  currentApprovalId = id;
  currentState = "waiting";

  updateCharacterAnimation("waiting");
  buzzer.doubleBeep(); // DOUBLE BEEP for human approval

  if (!approval_panel) {
    approval_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(approval_panel, 230, 150);
    lv_obj_align(approval_panel, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(approval_panel, lv_color_hex(0x18181B), 0);
    lv_obj_set_style_border_color(approval_panel, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_border_width(approval_panel, 2, 0);
    lv_obj_set_style_radius(approval_panel, 12, 0);
  }

  lv_obj_clear_flag(approval_panel, LV_OBJ_FLAG_HIDDEN);

  // Update Agent badge
  lv_label_set_text_fmt(agent_badge_label, "[ %s ]", agent.c_str());
  lv_obj_set_style_text_color(agent_badge_label, lv_color_hex(0xF59E0B), 0);

  lv_label_set_text(status_label, question.c_str());
  lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), 0);
}

void sendApprovalResponse(const String& id, const String& choice, const String& source) {
  buzzer.clickTone();

  DynamicJsonDocument doc(256);
  doc["event"] = "approval_response";
  doc["id"] = id;
  doc["choice"] = choice;
  doc["source"] = source;

  String output;
  serializeJson(doc, output);
  Serial.println(output);

  hideApprovalPanel();
  currentApprovalId = "";
  currentState = (choice == "Approve") ? "working" : "calm";
  updateCharacterAnimation(currentState);
}

void handleIncomingJson(const String& jsonStr) {
  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) return;

  String event = doc["event"] | "";

  if (event == "approval_request") {
    String id = doc["id"] | "";
    String agent = doc["agent"] | "Agent";
    String question = doc["question"] | "Approve task?";
    String details = doc["details"] | "";
    showApprovalDialog(id, agent, question, details);
  }
  else if (event == "task_complete") {
    currentState = "done";
    String agent = doc["agent"] | currentAgent.c_str();
    String summary = doc["summary"] | "Task Complete!";

    lv_label_set_text_fmt(agent_badge_label, "[ %s ]", agent.c_str());
    lv_obj_set_style_text_color(agent_badge_label, lv_color_hex(0x10B981), 0);

    lv_label_set_text(status_label, summary.c_str());
    updateCharacterAnimation("done");
    buzzer.tripleBeepDone(); // TRIPLE BEEP melody
  }
  else if (event == "agent_connected") {
    String agent = doc["agent"] | "Agent";
    currentAgent = agent;
    lv_label_set_text_fmt(agent_badge_label, "[ %s ONLINE ]", agent.c_str());
    lv_obj_set_style_text_color(agent_badge_label, lv_color_hex(0x3B82F6), 0);
    lv_label_set_text(status_label, "Ready for prompts");
    buzzer.connectChime();
  }
  else if (event == "notification" || event == "state_change") {
    String state = doc["state"] | "calm";
    String msg = doc["message"] | "";
    currentState = state;
    updateCharacterAnimation(state);
    if (msg.length() > 0) {
      lv_label_set_text(status_label, msg.c_str());
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Power Latch - keep high to power LCD and peripherals
  pinMode(SYS_EN_PIN, OUTPUT);
  digitalWrite(SYS_EN_PIN, HIGH);

  // Buttons
  pinMode(BTN_BOOT, INPUT_PULLUP);
  pinMode(BTN_PWR, INPUT_PULLUP);

  // Buzzer & Touch
  buzzer.begin();
  touch.begin();

  // LittleFS
  LittleFS.begin(true);

  // LCD Init
  gfx->begin();
  gfx->fillScreen(BLACK);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  gfx->setRotation(1);

  // LVGL Init
  lv_init();
  init_littlefs_for_lvgl();

  lv_display_t * disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, buf, NULL, DRAW_BUF_SIZE * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

  // UI Setup
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x09090B), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

  // Agent Badge (Top Header)
  agent_badge_label = lv_label_create(lv_screen_active());
  lv_label_set_text(agent_badge_label, "[ SPARKAI V3 ]");
  lv_obj_set_style_text_color(agent_badge_label, lv_color_hex(0x38BDF8), 0);
  lv_obj_align(agent_badge_label, LV_ALIGN_TOP_MID, 0, 8);

  // Character Animated GIF
  character_gif_obj = lv_gif_create(lv_screen_active());
  lv_obj_align(character_gif_obj, LV_ALIGN_CENTER, 0, -25);
  updateCharacterAnimation("calm");

  // Status Message Toast
  status_label = lv_label_create(lv_screen_active());
  lv_label_set_text(status_label, "Capy is resting...");
  lv_obj_set_style_text_color(status_label, lv_color_hex(0x94A3B8), 0);
  lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -15);

  lastTick = millis();
  buzzer.connectChime();
}

void loop() {
  uint32_t currentTick = millis();
  uint32_t delta = currentTick - lastTick;
  if (delta > 0) {
    lv_tick_inc(delta);
    lastTick = currentTick;
  }
  lv_timer_handler();

  // 1. Check Serial Commands from Gateway Hub
  if (Serial.available()) {
    String payload = Serial.readStringUntil('\n');
    payload.trim();
    if (payload.length() > 0) {
      handleIncomingJson(payload);
    }
  }

  // 2. Check Physical Buttons for Instant Human Approval
  if (currentState == "waiting" && currentApprovalId.length() > 0) {
    // Physical BOOT Button (GPIO0) -> Instant APPROVE
    if (digitalRead(BTN_BOOT) == LOW) {
      delay(50); // Debounce
      if (digitalRead(BTN_BOOT) == LOW) {
        sendApprovalResponse(currentApprovalId, "Approve", "boot_button");
        while (digitalRead(BTN_BOOT) == LOW) delay(10);
      }
    }

    // Physical PWR Button (GPIO40) -> Instant DENY
    if (digitalRead(BTN_PWR) == LOW) {
      delay(50); // Debounce
      if (digitalRead(BTN_PWR) == LOW) {
        sendApprovalResponse(currentApprovalId, "Deny", "pwr_button");
        while (digitalRead(BTN_PWR) == LOW) delay(10);
      }
    }
  }

  // 3. Check CST816T Capacitive Touch Screen
  TouchPoint pt = touch.read();
  if (pt.touched && currentState == "waiting" && currentApprovalId.length() > 0) {
    // Top half / bottom left -> Approve, bottom right -> Deny
    if (pt.y > 180) {
      if (pt.x < 120) {
        sendApprovalResponse(currentApprovalId, "Approve", "touch_screen");
      } else {
        sendApprovalResponse(currentApprovalId, "Deny", "touch_screen");
      }
      delay(300); // Prevent double-tap
    }
  }

  delay(5);
}
