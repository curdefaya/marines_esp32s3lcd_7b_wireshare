#include <Arduino.h>
#include "lvgl_port.h" 

// --- DEFINICJE STRUKTUR ---

typedef struct __attribute__((packed)) {
    uint8_t ID;      
    uint8_t size;    
    uint8_t dir;     
    uint16_t rpm;    
    float rounds;    
} manual_control_t;

typedef struct __attribute__((packed)) {
    uint8_t ID;      
    uint8_t size;    
    uint8_t dir;     
    uint16_t rpm;    
    float rounds;    
    uint32_t time;   
} test_mode_params_t;

// --- ZMIENNE GLOBALNE LVGL ---
static lv_obj_t * scr_boot;
static lv_obj_t * scr_menu;
static lv_obj_t * scr_manual;
static lv_obj_t * scr_test;
static lv_obj_t * kb; 

// Zmienne MANUAL MODE
static lv_obj_t * ta_manual_rpm;
static lv_timer_t * timer_manual = NULL;
static uint8_t manual_dir_current = 0; 

// Zmienne TEST MODE
static lv_obj_t * ta_test_time;
static lv_obj_t * ta_test_rpm;
static lv_obj_t * ta_test_rounds;
static lv_obj_t * lbl_test_status;
static bool test_in_progress = false;

// --- FUNKCJE POMOCNICZE ---

void send_mode_cmd(uint8_t cmd) {
    Serial.write(cmd);
}

void manual_timer_cb(lv_timer_t * timer) {
    manual_control_t packet;
    packet.ID = 0x02;
    packet.size = sizeof(manual_control_t);
    packet.dir = manual_dir_current;
    
    int rpm_val = atoi(lv_textarea_get_text(ta_manual_rpm));
    if (rpm_val > 240) { rpm_val = 240; lv_textarea_set_text(ta_manual_rpm, "240"); }
    if (rpm_val < 0) { rpm_val = 0; lv_textarea_set_text(ta_manual_rpm, "0"); }
    packet.rpm = (uint16_t)rpm_val;
    packet.rounds = 1000.0f;

    Serial.write((uint8_t*)&packet, packet.size);
}

// --- CALLBACKI ZDARZEŃ LVGL (LVGL v8.4 API) ---

static void event_kb_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e); // W v8 zwraca poprawnie lv_obj_t*
    
    if(code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN); // W v8 używamy clear_flag
    }
    if(code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void btn_mode_enter_cb(lv_event_t * e) {
    uint8_t mode_cmd = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    send_mode_cmd(mode_cmd);

    if (mode_cmd == 0x02) {
        lv_scr_load(scr_manual); // W v8 używamy lv_scr_load
        if(!timer_manual) {
            timer_manual = lv_timer_create(manual_timer_cb, 10, NULL);
        } else {
            lv_timer_resume(timer_manual);
        }
    } else if (mode_cmd == 0x04) {
        lv_scr_load(scr_test);
    }
}

static void btn_mode_exit_cb(lv_event_t * e) {
    if (test_in_progress) {
        lv_label_set_text(lbl_test_status, "STATUS: Test nie zostal ukonczony!");
        return; 
    }
    
    if (timer_manual) {
        lv_timer_pause(timer_manual);
        manual_dir_current = 0;
    }
    
    send_mode_cmd(0xFF);
    lv_scr_load(scr_menu);
}

static void manual_dir_btn_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint8_t dir_val = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    
    if (code == LV_EVENT_PRESSED) {
        manual_dir_current = dir_val;
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        manual_dir_current = 0;
    }
}

static void test_msgbox_cb(lv_event_t * e) {
    lv_obj_t * msgbox = lv_event_get_current_target(e);
    
    if(lv_msgbox_get_active_btn(msgbox) == 0) { // Przycisk "TAK" w v8
        test_in_progress = true;
        
        test_mode_params_t packet;
        packet.ID = 0x04;
        packet.size = sizeof(test_mode_params_t);
        packet.dir = 0;
        packet.rpm = (uint16_t)atoi(lv_textarea_get_text(ta_test_rpm));
        packet.rounds = atof(lv_textarea_get_text(ta_test_rounds));
        packet.time = (uint32_t)(atoi(lv_textarea_get_text(ta_test_time)) * 1000);

        Serial.write((uint8_t*)&packet, packet.size);
        lv_label_set_text(lbl_test_status, "STATUS: TEST W TOKU...");
    }
    
    lv_msgbox_close(msgbox);
}

static void btn_test_start_cb(lv_event_t * e) {
    if (test_in_progress) {
        lv_label_set_text(lbl_test_status, "STATUS: Test juz trwa!");
        return;
    }

    static const char * btns[] = {"TAK", "Anuluj", ""};
    // Składnia MsgBox dla LVGL v8
    lv_obj_t * mbox = lv_msgbox_create(NULL, "Rozpocznij test", "Czy na pewno chcesz rozpoczac test?", btns, true);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, test_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void boot_timer_cb(lv_timer_t * timer) {
    lv_scr_load(scr_menu);
    lv_timer_del(timer); // W v8 timer usuwamy przez lv_timer_del
}

// --- TWORZENIE INTERFEJSU ---

void build_ui(void) {
    scr_boot = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_boot, lv_color_hex(0x001133), 0);
    
    lv_obj_t * logo_label = lv_label_create(scr_boot);
    lv_label_set_text(logo_label, "PANEL BOJKI\nLadowanie...");
    lv_obj_set_style_text_color(logo_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(logo_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(logo_label);
    
    lv_timer_create(boot_timer_cb, 2000, NULL);

    kb = lv_keyboard_create(lv_layer_top());
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    scr_menu = lv_obj_create(NULL);
    lv_obj_set_flex_flow(scr_menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr_menu, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char * modes[] = {"MANUAL MODE", "TEST MODE", "MISSION MODE", "GET DATA MODE", "LIVE DATA MODE"};
    const uint8_t hex_codes[] = {0x02, 0x04, 0x06, 0x08, 0x0A};

    for(int i = 0; i < 5; i++) {
        lv_obj_t * btn = lv_btn_create(scr_menu); // W v8 lv_btn_create
        lv_obj_set_size(btn, 240, 45);
        lv_obj_t * label = lv_label_create(btn);
        lv_label_set_text(label, modes[i]);
        lv_obj_center(label);
        lv_obj_add_event_cb(btn, btn_mode_enter_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)hex_codes[i]);
    }

    scr_manual = lv_obj_create(NULL);
    
    lv_obj_t * btn_back1 = lv_btn_create(scr_manual);
    lv_obj_t * lbl_back1 = lv_label_create(btn_back1);
    lv_label_set_text(lbl_back1, LV_SYMBOL_LEFT " WROC");
    lv_obj_align(btn_back1, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(btn_back1, btn_mode_exit_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_rpm = lv_label_create(scr_manual);
    lv_label_set_text(lbl_rpm, "RPM (0-240):");
    lv_obj_align(lbl_rpm, LV_ALIGN_CENTER, -70, -50);

    ta_manual_rpm = lv_textarea_create(scr_manual);
    lv_textarea_set_one_line(ta_manual_rpm, true);
    lv_textarea_set_text(ta_manual_rpm, "100");
    lv_textarea_set_accepted_chars(ta_manual_rpm, "0123456789");
    lv_obj_set_width(ta_manual_rpm, 90);
    lv_obj_align(ta_manual_rpm, LV_ALIGN_CENTER, 50, -50);
    lv_obj_add_event_cb(ta_manual_rpm, event_kb_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * btn_up = lv_btn_create(scr_manual);
    lv_obj_set_size(btn_up, 90, 70);
    lv_obj_align(btn_up, LV_ALIGN_CENTER, -60, 40);
    lv_obj_t * lbl_up = lv_label_create(btn_up);
    lv_label_set_text(lbl_up, LV_SYMBOL_UP "\nGORA");
    lv_obj_set_style_text_align(lbl_up, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_up);
    lv_obj_add_event_cb(btn_up, manual_dir_btn_cb, LV_EVENT_ALL, (void*)1);

    lv_obj_t * btn_down = lv_btn_create(scr_manual);
    lv_obj_set_size(btn_down, 90, 70);
    lv_obj_align(btn_down, LV_ALIGN_CENTER, 60, 40);
    lv_obj_t * lbl_down = lv_label_create(btn_down);
    lv_label_set_text(lbl_down, LV_SYMBOL_DOWN "\nDOL");
    lv_obj_set_style_text_align(lbl_down, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_down);
    lv_obj_add_event_cb(btn_down, manual_dir_btn_cb, LV_EVENT_ALL, (void*)2);

    scr_test = lv_obj_create(NULL);

    lv_obj_t * btn_back2 = lv_btn_create(scr_test);
    lv_obj_t * lbl_back2 = lv_label_create(btn_back2);
    lv_label_set_text(lbl_back2, LV_SYMBOL_LEFT " WROC");
    lv_obj_align(btn_back2, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(btn_back2, btn_mode_exit_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_ttime = lv_label_create(scr_test);
    lv_label_set_text(lbl_ttime, "Czas [s]:");
    lv_obj_align(lbl_ttime, LV_ALIGN_TOP_LEFT, 30, 65);
    ta_test_time = lv_textarea_create(scr_test);
    lv_textarea_set_one_line(ta_test_time, true);
    lv_textarea_set_text(ta_test_time, "10");
    lv_obj_set_size(ta_test_time, 90, 40);
    lv_obj_align(ta_test_time, LV_ALIGN_TOP_LEFT, 130, 55);
    lv_obj_add_event_cb(ta_test_time, event_kb_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * lbl_trpm = lv_label_create(scr_test);
    lv_label_set_text(lbl_trpm, "RPM:");
    lv_obj_align(lbl_trpm, LV_ALIGN_TOP_LEFT, 30, 115);
    ta_test_rpm = lv_textarea_create(scr_test);
    lv_textarea_set_one_line(ta_test_rpm, true);
    lv_textarea_set_text(ta_test_rpm, "120");
    lv_obj_set_size(ta_test_rpm, 90, 40);
    lv_obj_align(ta_test_rpm, LV_ALIGN_TOP_LEFT, 130, 105);
    lv_obj_add_event_cb(ta_test_rpm, event_kb_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * lbl_trounds = lv_label_create(scr_test);
    lv_label_set_text(lbl_trounds, "Obroty:");
    lv_obj_align(lbl_trounds, LV_ALIGN_TOP_LEFT, 30, 165);
    ta_test_rounds = lv_textarea_create(scr_test);
    lv_textarea_set_one_line(ta_test_rounds, true);
    lv_textarea_set_text(ta_test_rounds, "50");
    lv_obj_set_size(ta_test_rounds, 90, 40);
    lv_obj_align(ta_test_rounds, LV_ALIGN_TOP_LEFT, 130, 155);
    lv_obj_add_event_cb(ta_test_rounds, event_kb_cb, LV_EVENT_ALL, NULL);

    lbl_test_status = lv_label_create(scr_test);
    lv_label_set_text(lbl_test_status, "STATUS: Gotowy");
    lv_obj_align(lbl_test_status, LV_ALIGN_BOTTOM_MID, 0, -70);

    lv_obj_t * btn_start = lv_btn_create(scr_test);
    lv_obj_set_size(btn_start, 200, 45);
    lv_obj_align(btn_start, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_color(btn_start, lv_color_hex(0xB22222), 0);
    lv_obj_t * lbl_start = lv_label_create(btn_start);
    lv_label_set_text(lbl_start, "START TEST");
    lv_obj_center(lbl_start);
    lv_obj_add_event_cb(btn_start, btn_test_start_cb, LV_EVENT_CLICKED, NULL);

    lv_scr_load(scr_boot);
}

// --- SETUP & LOOP ---

void setup() {
    Serial.begin(115200);

    static esp_lcd_panel_handle_t panel_handle = NULL; 
    static esp_lcd_touch_handle_t tp_handle = NULL;    

    tp_handle = touch_gt911_init();  
    panel_handle = waveshare_esp32_s3_rgb_lcd_init(); 
    wavesahre_rgb_lcd_bl_on();   

    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle));

    if (lvgl_port_lock(-1)) {
        build_ui();
        lvgl_port_unlock();
    }
}

void loop() {
    if (test_in_progress && Serial.available() >= sizeof(test_mode_params_t)) {
        test_mode_params_t incoming;
        Serial.readBytes((char*)&incoming, sizeof(test_mode_params_t));
        
        if (incoming.ID == 0x04) {
            test_in_progress = false;
            
            if (lvgl_port_lock(-1)) {
                lv_label_set_text(lbl_test_status, "STATUS: Test ukonczony!");
                lvgl_port_unlock();
            }
        }
    }
    
    delay(10);
}