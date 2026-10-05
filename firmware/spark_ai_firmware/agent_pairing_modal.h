#ifndef AGENT_PAIRING_MODAL_H
#define AGENT_PAIRING_MODAL_H

#include <Arduino.h>
#include <lvgl.h>

class AgentPairingModal {
public:
    AgentPairingModal() : modal(NULL), qr_obj(NULL), title_label(NULL), token_label(NULL), url_label(NULL), instr_label(NULL), close_btn(NULL), isOpen(false) {}

    void show(lv_obj_t *parent, const String& qrPayload, const String& titleText = "[ PAIR AGENT ]", const String& tokenText = "", const String& urlText = "", const String& instrText = "Scan QR to copy prompt") {
        currentToken = tokenText.length() > 0 ? tokenText : qrPayload;
        isOpen = true;

        if (!modal) {
            modal = lv_obj_create(parent);
            lv_obj_set_size(modal, 236, 264);
            lv_obj_align(modal, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_style_bg_color(modal, lv_color_hex(0x0F172A), 0);
            lv_obj_set_style_border_color(modal, lv_color_hex(0x38BDF8), 0);
            lv_obj_set_style_border_width(modal, 2, 0);
            lv_obj_set_style_radius(modal, 12, 0);
            lv_obj_remove_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

            // Title
            title_label = lv_label_create(modal);
            lv_obj_set_style_text_color(title_label, lv_color_hex(0x38BDF8), 0);
            lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 2);

            // QR Code (116x116)
            #if LV_USE_QRCODE
            qr_obj = lv_qrcode_create(modal);
            lv_qrcode_set_size(qr_obj, 116);
            lv_qrcode_set_dark_color(qr_obj, lv_color_hex(0x000000));
            lv_qrcode_set_light_color(qr_obj, lv_color_hex(0xFFFFFF));
            lv_obj_align(qr_obj, LV_ALIGN_TOP_MID, 0, 22);
            #endif

            // Token in Large Gold Font
            token_label = lv_label_create(modal);
            lv_obj_set_style_text_font(token_label, &lv_font_montserrat_18, 0);
            lv_obj_set_style_text_color(token_label, lv_color_hex(0xFBBF24), 0);
            lv_obj_align(token_label, LV_ALIGN_TOP_MID, 0, 142);

            // Host URL in Cyan
            url_label = lv_label_create(modal);
            lv_obj_set_style_text_color(url_label, lv_color_hex(0x38BDF8), 0);
            lv_obj_align(url_label, LV_ALIGN_TOP_MID, 0, 164);

            // Instructions
            instr_label = lv_label_create(modal);
            lv_obj_set_width(instr_label, 220);
            lv_label_set_long_mode(instr_label, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_align(instr_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(instr_label, lv_color_hex(0x94A3B8), 0);
            lv_obj_align(instr_label, LV_ALIGN_TOP_MID, 0, 184);

            // Close button
            close_btn = lv_button_create(modal);
            lv_obj_set_size(close_btn, 110, 30);
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

        if (title_label) {
            lv_label_set_text(title_label, titleText.c_str());
        }

        #if LV_USE_QRCODE
        if (qr_obj) {
            lv_qrcode_update(qr_obj, qrPayload.c_str(), qrPayload.length());
        }
        #endif

        if (token_label) {
            lv_label_set_text(token_label, currentToken.c_str());
        }

        if (url_label) {
            lv_label_set_text(url_label, urlText.c_str());
        }

        if (instr_label) {
            lv_label_set_text(instr_label, instrText.c_str());
        }

        lv_obj_clear_flag(modal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(modal);
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
    lv_obj_t *title_label;
    lv_obj_t *token_label;
    lv_obj_t *url_label;
    lv_obj_t *instr_label;
    lv_obj_t *close_btn;
    bool isOpen;
    String currentToken;
};

#endif // AGENT_PAIRING_MODAL_H