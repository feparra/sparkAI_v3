/**
 * SparkAI V3 - Physical Companion Screen Firmware
 * Waveshare ESP32-S3-Touch-LCD-1.69
 * Hardware: ST7789V2 SPI, CST816T Touch, GPIO42 Buzzer, GPIO0 BOOT, GPIO40 PWR
 * Orientation: Portrait (240x280) matching Muse AI (rotation 2: MX | MY)
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include <lvgl.h>
#include "Arduino_GFX_Library.h"

#include "pin_config.h"
#include "buzzer.h"
#include "touch_cst816.h"
#include "wifi_manager.h"
#include "web_api_server.h"
#include "agent_pairing_modal.h"

// Wi-Fi, On-Chip REST API & Pairing Modal
SparkWiFiManager wifiManager;
SparkApiServer apiServer(7890);
AgentPairingModal pairingModal;

// Hardware Bus & Display Driver (Portrait: 240x280, gap offset: 0, 20)
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 2, true, LCD_WIDTH, LCD_HEIGHT, 0, 20, 0, 20);

// Hardware Peripherals
SparkBuzzer buzzer;
CST816TTouch touch;

// LVGL Draw Buffer (40 lines = 19.2 KB)
static const uint32_t screenWidth  = 240;
static const uint32_t screenHeight = 280;
#define DRAW_BUF_SIZE (screenWidth * 40)
static lv_color_t buf[DRAW_BUF_SIZE];

// Active UI Objects
lv_obj_t *agent_badge_label = NULL;
lv_obj_t *character_gif_obj = NULL;
lv_obj_t *avatar_card_obj = NULL;
lv_obj_t *avatar_eyes_label = NULL;
lv_obj_t *avatar_name_label = NULL;

lv_obj_t *status_card_obj = NULL;
lv_obj_t *status_label = NULL;

lv_obj_t *approval_panel = NULL;
lv_obj_t *approval_title_label = NULL;
lv_obj_t *question_label = NULL;
lv_obj_t *btn_approve_obj = NULL;
lv_obj_t *btn_deny_obj = NULL;

String currentCharacter = "capy";
String currentState = "calm";
String currentAgent = "SparkAI";
String currentApprovalId = "";
uint32_t lastTick = 0;
bool littlefs_ready = false;

// LittleFS to LVGL File System Bridge (Robust slash normalization)
static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
  if (!littlefs_ready) return NULL;
  const char * flags = (mode == LV_FS_MODE_WR) ? "w" : "r";
  String fullPath = path;
  if (!fullPath.startsWith("/")) fullPath = "/" + fullPath;
  if (!LittleFS.exists(fullPath)) return NULL;
  File f = LittleFS.open(fullPath, flags);
  if (!f) return NULL;
  return (void *)(new File(f));
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p) {
  File * fp = (File *)file_p;
  if (fp) {
    fp->close();
    delete fp;
  }
  return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
  File * fp = (File *)file_p;
  if (!fp) return LV_FS_RES_FS_ERR;
  *br = fp->read((uint8_t *)buf, btr);
  return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
  File * fp = (File *)file_p;
  if (!fp) return LV_FS_RES_FS_ERR;
  SeekMode mode = (whence == LV_FS_SEEK_SET) ? SeekSet : (whence == LV_FS_SEEK_CUR) ? SeekCur : SeekEnd;
  fp->seek(pos, mode);
  return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
  File * fp = (File *)file_p;
  if (!fp) return LV_FS_RES_FS_ERR;
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

void updateAvatarFace(const String& character, const String& state) {
  if (!avatar_eyes_label || !avatar_name_label) return;

  String nameDisplay = character;
  nameDisplay.toUpperCase();
  lv_label_set_text(avatar_name_label, nameDisplay.c_str());

  if (state == "working") {
    lv_label_set_text(avatar_eyes_label, "(* _ *)");
    lv_obj_set_style_text_color(avatar_eyes_label, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_color(avatar_card_obj, lv_color_hex(0x0284C7), 0);
  }
  else if (state == "waiting") {
    lv_label_set_text(avatar_eyes_label, "(! _ !)");
    lv_obj_set_style_text_color(avatar_eyes_label, lv_color_hex(0xF59E0B), 0);
    lv_obj_set_style_border_color(avatar_card_obj, lv_color_hex(0xD97706), 0);
  }
  else if (state == "done") {
    lv_label_set_text(avatar_eyes_label, "(^ O ^)");
    lv_obj_set_style_text_color(avatar_eyes_label, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_border_color(avatar_card_obj, lv_color_hex(0x059669), 0);
  }
  else if (state == "error") {
    lv_label_set_text(avatar_eyes_label, "(x _ x)");
    lv_obj_set_style_text_color(avatar_eyes_label, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_border_color(avatar_card_obj, lv_color_hex(0xDC2626), 0);
  }
  else { // calm
    lv_label_set_text(avatar_eyes_label, "(^ . ^)");
    lv_obj_set_style_text_color(avatar_eyes_label, lv_color_hex(0xA7F3D0), 0);
    lv_obj_set_style_border_color(avatar_card_obj, lv_color_hex(0x4B5563), 0);
  }
}

void updateCharacterAnimation(const String& character, const String& state) {
  currentCharacter = character;
  currentState = state;

  String filename = "/" + character + "_" + state + ".gif";
  if (!LittleFS.exists(filename)) {
    filename = "/" + state + ".gif";
  }

  // Attempt GIF display if available
  if (littlefs_ready && LittleFS.exists(filename) && character_gif_obj) {
    String lvPath = "L:" + filename;
    lv_gif_set_src(character_gif_obj, lvPath.c_str());
    lv_obj_clear_flag(character_gif_obj, LV_OBJ_FLAG_HIDDEN);
    if (avatar_card_obj) lv_obj_add_flag(avatar_card_obj, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  // Fallback to Expressive Animated Avatar Card
  if (avatar_card_obj) {
    lv_obj_clear_flag(avatar_card_obj, LV_OBJ_FLAG_HIDDEN);
    if (character_gif_obj) lv_obj_add_flag(character_gif_obj, LV_OBJ_FLAG_HIDDEN);
    updateAvatarFace(character, state);
  }
}

void hideApprovalDialog() {
  if (approval_panel) {
    lv_obj_add_flag(approval_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

void showApprovalDialog(const String& id, const String& agent, const String& question) {
  currentApprovalId = id;
  currentState = "waiting";

  updateCharacterAnimation(currentCharacter, "waiting");
  buzzer.doubleBeep(); // Double Beep for Human-In-The-Loop Approval

  if (!approval_panel) {
    approval_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(approval_panel, 236, 235);
    lv_obj_align(approval_panel, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_bg_color(approval_panel, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(approval_panel, lv_color_hex(0xF59E0B), 0);
    lv_obj_set_style_border_width(approval_panel, 3, 0);
    lv_obj_set_style_radius(approval_panel, 14, 0);
    lv_obj_remove_flag(approval_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Title Tag
    approval_title_label = lv_label_create(approval_panel);
    lv_label_set_text(approval_title_label, "[ ACTION APPROVAL ]");
    lv_obj_set_style_text_color(approval_title_label, lv_color_hex(0xFBBF24), 0);
    lv_obj_align(approval_title_label, LV_ALIGN_TOP_MID, 0, 2);

    // Question Label (Bold, Centered, Montserrat 18)
    question_label = lv_label_create(approval_panel);
    lv_obj_set_width(question_label, 215);
    lv_label_set_long_mode(question_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(question_label, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_text_color(question_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(question_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(question_label, &lv_font_montserrat_18, 0);

    // Green Approve Button (Left: X=4, Width=102, Height=44)
    btn_approve_obj = lv_button_create(approval_panel);
    lv_obj_set_size(btn_approve_obj, 102, 44);
    lv_obj_align(btn_approve_obj, LV_ALIGN_BOTTOM_LEFT, 2, -2);
    lv_obj_set_style_bg_color(btn_approve_obj, lv_color_hex(0x16A34A), 0);
    lv_obj_set_style_radius(btn_approve_obj, 8, 0);
    lv_obj_t *lbl_app = lv_label_create(btn_approve_obj);
    lv_label_set_text(lbl_app, "APPROVE\n[BOOT]");
    lv_obj_set_style_text_align(lbl_app, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_app);

    // Red Deny Button (Right: X=118, Width=102, Height=44)
    btn_deny_obj = lv_button_create(approval_panel);
    lv_obj_set_size(btn_deny_obj, 102, 44);
    lv_obj_align(btn_deny_obj, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
    lv_obj_set_style_bg_color(btn_deny_obj, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(btn_deny_obj, 8, 0);
    lv_obj_t *lbl_den = lv_label_create(btn_deny_obj);
    lv_label_set_text(lbl_den, "DENY\n[PWR]");
    lv_obj_set_style_text_align(lbl_den, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_den);
  }

  lv_label_set_text(question_label, question.c_str());
  lv_obj_clear_flag(approval_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(approval_panel);

  // Update Agent Badge in Header
  String badge = (agent.length() > 0) ? agent : currentAgent;
  lv_label_set_text_fmt(agent_badge_label, "[ %s ]", badge.c_str());
  lv_obj_set_style_text_color(agent_badge_label, lv_color_hex(0xF59E0B), 0);
}

void sendApprovalResponse(const String& id, const String& choice, const String& source) {
  buzzer.clickTone();

  JsonDocument doc;
  doc["event"] = "approval_response";
  doc["id"] = id;
  doc["choice"] = choice;
  doc["source"] = source;

  String output;
  serializeJson(doc, output);
  Serial.println(output);
  apiServer.resolveApproval(choice, source);

  hideApprovalDialog();
  currentApprovalId = "";
  currentState = (choice == "Approve") ? "working" : "calm";
  updateCharacterAnimation(currentCharacter, currentState);

  if (choice == "Approve") {
    lv_label_set_text(status_label, "Approved! Working...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x4ADE80), 0);
  } else {
    lv_label_set_text(status_label, "Action Denied.");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xF87171), 0);
  }
}

void handleIncomingJson(const String& jsonStr) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    Serial.printf("[JSON Err] %s\n", err.c_str());
    return;
  }

  String event = doc["event"] | "";

  if (event == "approval_request") {
    String id = doc["id"] | "";
    String agent = doc["agent"] | currentAgent.c_str();
    String question = doc["question"] | "Approve execution?";
    showApprovalDialog(id, agent, question);
  }
  else if (event == "task_complete") {
    currentState = "done";
    String agent = doc["agent"] | currentAgent.c_str();
    String summary = doc["summary"] | "Task Complete!";

    lv_label_set_text_fmt(agent_badge_label, "[ %s ]", agent.c_str());
    lv_obj_set_style_text_color(agent_badge_label, lv_color_hex(0x10B981), 0);

    lv_label_set_text(status_label, summary.c_str());
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x34D399), 0);

    updateCharacterAnimation(currentCharacter, "done");
    buzzer.tripleBeepDone(); // Triple Beep Celebration Melody
  }
  else if (event == "agent_connected") {
    pairingModal.hide();
    String agent = doc["agent"] | "Agent";
    currentAgent = agent;
    lv_label_set_text_fmt(agent_badge_label, "[ %s ONLINE ]", agent.c_str());
    lv_obj_set_style_text_color(agent_badge_label, lv_color_hex(0x38BDF8), 0);

    lv_label_set_text(status_label, "Ready for prompts");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), 0);
    buzzer.connectChime();
  }
  else if (event == "character_switch") {
    String character = doc["character"] | "capy";
    if (character == "spark") character = "capy";
    currentCharacter = character;
    updateCharacterAnimation(currentCharacter, currentState);
    buzzer.clickTone();
  }
  else if (event == "telemetry_sync") {
    String agent = doc["agent"] | currentAgent.c_str();
    String msg = doc["message"] | "System Ready";
    currentAgent = agent;
    lv_label_set_text_fmt(agent_badge_label, "[ %s ONLINE ]", agent.c_str());
    lv_obj_set_style_text_color(agent_badge_label, lv_color_hex(0x38BDF8), 0);
    lv_label_set_text(status_label, msg.c_str());
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x38BDF8), 0);
    buzzer.clickTone();
  }
  else if (event == "notification" || event == "state_change") {
    String state = doc["state"] | "calm";
    String msg = doc["message"] | "";
    currentState = state;
    updateCharacterAnimation(currentCharacter, state);
    if (msg.length() > 0) {
      lv_label_set_text(status_label, msg.c_str());
      lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), 0);
    }
  }
  apiServer.setStatusData(currentAgent, currentCharacter, currentState, (status_label ? lv_label_get_text(status_label) : ""));
}

void setup() {
  // Set large CDC RX buffer (4KB) so large approval JSONs never get truncated
  Serial.setRxBufferSize(4096);
  Serial.begin(115200);

  // Power Latch: drive HIGH immediately
  pinMode(SYS_EN_PIN, OUTPUT);
  digitalWrite(SYS_EN_PIN, HIGH);

  // Buttons
  pinMode(BTN_BOOT, INPUT_PULLUP);
  pinMode(BTN_PWR, INPUT_PULLUP);

  // Buzzer & Touch
  buzzer.begin();
  touch.begin();

  // LittleFS (Partition: spiffs at 0xc90000 in 16MB table)
  littlefs_ready = LittleFS.begin(false, "/littlefs", 10, "spiffs");
  if (!littlefs_ready) {
    littlefs_ready = LittleFS.begin(true, "/littlefs", 10, "spiffs");
  }

  // LCD Init (Portrait: 240x280 matching Muse AI)
  gfx->begin();
  gfx->fillScreen(BLACK);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  gfx->setRotation(2); // Rotation 2: MX | MY (Portrait, 240x280)

  // LVGL Init
  lv_init();
  init_littlefs_for_lvgl();

  lv_display_t * disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, buf, NULL, DRAW_BUF_SIZE * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Background Style
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x09090B), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

  // 1. Agent Badge (Top Header)
  agent_badge_label = lv_label_create(lv_screen_active());
  lv_label_set_text(agent_badge_label, "[ SPARKAI V3 ]");
  lv_obj_set_style_text_color(agent_badge_label, lv_color_hex(0x38BDF8), 0);
  lv_obj_align(agent_badge_label, LV_ALIGN_TOP_MID, 0, 8);

  // 2. Character Animated GIF
  character_gif_obj = lv_gif_create(lv_screen_active());
  lv_obj_align(character_gif_obj, LV_ALIGN_CENTER, 0, -32);

  // 3. Fallback / Live Expressive Avatar Card (Y: 42..150)
  avatar_card_obj = lv_obj_create(lv_screen_active());
  lv_obj_set_size(avatar_card_obj, 140, 105);
  lv_obj_align(avatar_card_obj, LV_ALIGN_CENTER, 0, -32);
  lv_obj_set_style_bg_color(avatar_card_obj, lv_color_hex(0x18181B), 0);
  lv_obj_set_style_border_color(avatar_card_obj, lv_color_hex(0x3B82F6), 0);
  lv_obj_set_style_border_width(avatar_card_obj, 2, 0);
  lv_obj_set_style_radius(avatar_card_obj, 16, 0);
  lv_obj_remove_flag(avatar_card_obj, LV_OBJ_FLAG_SCROLLABLE);

  avatar_name_label = lv_label_create(avatar_card_obj);
  lv_label_set_text(avatar_name_label, "CAPY");
  lv_obj_set_style_text_color(avatar_name_label, lv_color_hex(0x94A3B8), 0);
  lv_obj_align(avatar_name_label, LV_ALIGN_TOP_MID, 0, -2);

  avatar_eyes_label = lv_label_create(avatar_card_obj);
  lv_label_set_text(avatar_eyes_label, "(^ . ^)");
  lv_obj_set_style_text_font(avatar_eyes_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(avatar_eyes_label, lv_color_hex(0xA7F3D0), 0);
  lv_obj_align(avatar_eyes_label, LV_ALIGN_CENTER, 0, 8);

  // 4. Prominent Notification Card (Center-Bottom: Y: 172..260)
  status_card_obj = lv_obj_create(lv_screen_active());
  lv_obj_set_size(status_card_obj, 226, 85);
  lv_obj_align(status_card_obj, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_bg_color(status_card_obj, lv_color_hex(0x18181B), 0);
  lv_obj_set_style_border_color(status_card_obj, lv_color_hex(0x27272A), 0);
  lv_obj_set_style_border_width(status_card_obj, 2, 0);
  lv_obj_set_style_radius(status_card_obj, 12, 0);
  lv_obj_remove_flag(status_card_obj, LV_OBJ_FLAG_SCROLLABLE);

  status_label = lv_label_create(status_card_obj);
  lv_obj_set_width(status_label, 206);
  lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
  lv_label_set_text(status_label, "Capy is resting...");
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(status_label);

  updateCharacterAnimation(currentCharacter, "calm");

  lastTick = millis();

  // Initialize Wi-Fi Manager (Connect to saved network or launch SoftAP setup portal)
  wifiManager.begin(
    [](const String& status, const String& details) {
      if (status_label) {
        lv_label_set_text_fmt(status_label, "%s\n%s", status.c_str(), details.c_str());
      }
    },
    [](const String& qrPayload, const String& title, const String& instructions, const String& subText) {
      pairingModal.show(lv_screen_active(), qrPayload, title, subText, "http://192.168.4.1", instructions);
    }
  );

  if (wifiManager.isWifiConnected()) {
    pairingModal.hide();
    apiServer.begin(handleIncomingJson);
    buzzer.connectChime();
    if (status_label) {
      lv_label_set_text_fmt(status_label, "sparkai.local\n%s", wifiManager.getLocalIp().c_str());
    }
  } else {
    buzzer.connectChime();
  }
}

void loop() {
  uint32_t currentTick = millis();
  uint32_t delta = currentTick - lastTick;
  if (delta > 0) {
    lv_tick_inc(delta);
    lastTick = currentTick;
  }
  lv_timer_handler();

  // 1. Process Wi-Fi Manager (Captive Portal DNS & Server in AP mode)
  wifiManager.handle();

  // 2. Process On-Chip HTTP REST API when connected to Wi-Fi
  if (wifiManager.isWifiConnected()) {
    apiServer.handle();
  }

  // 3. Process Serial JSON stream from USB (Gateway / Local Bridge)
  if (Serial.available()) {
    String payload = Serial.readStringUntil('\n');
    payload.trim();
    if (payload.length() > 0) {
      handleIncomingJson(payload);
    }
  }

  // 4. Process Physical Hardware Buttons for Approvals
  if (currentState == "waiting" && currentApprovalId.length() > 0) {
    if (digitalRead(BTN_BOOT) == LOW) {
      delay(40);
      if (digitalRead(BTN_BOOT) == LOW) {
        sendApprovalResponse(currentApprovalId, "Approve", "boot_button");
        while (digitalRead(BTN_BOOT) == LOW) delay(10);
      }
    }

    if (digitalRead(BTN_PWR) == LOW) {
      delay(40);
      if (digitalRead(BTN_PWR) == LOW) {
        sendApprovalResponse(currentApprovalId, "Deny", "pwr_button");
        while (digitalRead(BTN_PWR) == LOW) delay(10);
      }
    }
  }

  // 5. Process CST816T Capacitive Touch Screen
  TouchPoint pt = touch.read();
  if (pt.touched) {
    // A) If Pairing Modal is open: touch Dismiss button (bottom Y > 180) or outside closes it!
    if (pairingModal.isModalOpen()) {
      if (pt.y > 180 || pt.y < 45) {
        buzzer.clickTone();
        pairingModal.hide();
        delay(300);
      }
    }
    // B) Approval Buttons Touch (Bottom area Y > 170)
    else if (currentState == "waiting" && currentApprovalId.length() > 0 && pt.y > 170) {
      if (pt.x < 120) {
        sendApprovalResponse(currentApprovalId, "Approve", "touch_screen");
      } else {
        sendApprovalResponse(currentApprovalId, "Deny", "touch_screen");
      }
      delay(350);
    }
    // C) Tap Top Badge (Y < 50) to open on-screen Agent Pairing Token Modal!
    else if (currentState != "waiting" && pt.y < 50 && !pairingModal.isModalOpen()) {
      buzzer.clickTone();
      String pairToken = "SPARK-" + String(random(1000, 9999));
      String hostIp = wifiManager.getLocalIp();
      String hostUrl = "http://" + hostIp + ":7890";
      String promptText = "Connect to SparkAI at " + hostUrl + " token " + pairToken + ". Bind: POST /api/bind {\"agent\":\"Claude\",\"token\":\"" + pairToken + "\"}. Use /api/notify and /api/task_done.";
      pairingModal.show(lv_screen_active(), promptText, "[ PAIR AGENT ]", pairToken, hostUrl, "Scan QR to copy agent prompt");
      delay(350);
    }
  }

  delay(5);
}
