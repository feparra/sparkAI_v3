#ifndef AGENT_PAIRING_MODAL_H
#define AGENT_PAIRING_MODAL_H

#include <Arduino.h>
#include <lvgl.h>

class AgentPairingModal {
public:
    AgentPairingModal() : modal(NULL), qr_obj(NULL), token_label(NULL), instr_label(NULL), close_btn(NULL), isOpen(false) {}

    void show(lv_obj_t *parent, const String& token) {
        currentToken = token;
        isOpen = true;

        if (!modal) {
            modal = lv_obj_create(parent);
            lv_obj_set_size(modal, 236, 260);
            lv_obj_align(modal, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_style_bg_color(modal, lv_color_hex(0x0F172A), 0);
            lv_obj_set_style_border_color(modal, lv_color_hex(0x38BDF8), 0);
            lv_obj_set_style_border_width(modal, 2, 0);
            lv_obj_set_style_radius(modal, 12, 0);
            lv_obj_remove_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

            // Title
            lv_obj_t *title = lv_label_create(modal);
            lv_label_set_text(title, "[ PAIR NEW AGENT ]");
            lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

            // QR Code (110x110)
            #if LV_USE_QRCODE
            qr_obj = lv_qrcode_create(modal);
            lv_qrcode_set_size(qr_obj, 110);
            lv_qrcode_set_dark_color(qr_obj, lv_color_hex(0x000000));
            lv_qrcode_set_light_color(qr_obj, lv_color_hex(0xFFFFFF));
            lv_obj_align(qr_obj, LV_ALIGN_TOP_MID, 0, 28);
            #endif

            // Token in Large Font
            token_label = lv_label_create(modal);
            lv_obj_set_style_text_font(token_label, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(token_label, lv_color_hex(0xFBBF24), 0);
            lv_obj_align(token_label, LV_ALIGN_TOP_MID, 0, 146);

            // Instructions
            instr_label = lv_label_create(modal);
            lv_obj_set_width(instr_label, 220);
            lv_label_set_long_mode(instr_label, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_align(instr_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(instr_label, lv_color_hex(0x94A3B8), 0);
            lv_label_set_text(instr_label, "Tell your AI harness:\n'Connect with this token'");
            lv_obj_align(instr_label, LV_ALIGN_TOP_MID, 0, 174);

            // Close button
            close_btn = lv_button_create(modal);
            lv_obj_set_size(close_btn, 110, 32);
            lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, -4);
            lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x334155), 0);
            lv_obj_set_style_radius(close_btn, 6, 0);
            lv_obj_t *close_lbl = lv_label_create(close_btn);
            lv_label_set_text(close_lbl, "Dismiss");
            lv_obj_center(close_lbl);

            lv_obj_add_event_cb(close_btn, [](lv_event_t * e) {
                AgentPairingModal *self = (AgentPairingModal *)lv_event_get_user_data(e);
                if (self) self->hide();
            }, LV_EVENT_CLICKED, this);
        }

        #if LV_USE_QRCODE
        if (qr_obj) {
            lv_qrcode_update(qr_obj, token.c_str(), token.length());
        }
        #endif

        if (token_label) {
            lv_label_set_text(token_label, token.c_str());
        }

        lv_obj_clear_flag(modal, LV_OBJ_FLAG_HIDDEN);
    }

    void hide() {
        if (modal) {
            lv_obj_add_flag(modal, LV_OBJ_FLAG_HIDDEN);
        }
        isOpen = false;
    }

    bool isModalOpen() const {
        return isOpen;
    }

    String getToken() const {
        return currentToken;
    }

private:
    lv_obj_t *modal;
    lv_obj_t *qr_obj;
    lv_obj_t *token_label;
    lv_obj_t *instr_label;
    lv_obj_t *close_btn;
    bool isOpen;
    String currentToken;
};

#endif // AGENT_PAIRING_MODAL_H
