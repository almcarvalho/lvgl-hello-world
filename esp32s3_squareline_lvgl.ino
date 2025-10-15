/*
  ESP32-S3 + LCD 7" 800x480 RGB565 (Sunton-style pin map) + LVGL 8.3 + SquareLine
  Baseado na pinagem/frequências do arquivo que você enviou (o que já funciona).
  - Tela: 800x480, 16 bits (RGB565)
  - LVGL: 8.3
  - SquareLine: Screen1 com btnPay e lblMsg
  Ação: tocar no btnPay -> lblMsg = "Funcionando..."
*/

#define LGFX_USE_V1
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/touch/Touch_GT911.hpp>
#include <lvgl.h>

// ===== SquareLine =====
extern "C" {
  #include "ui.h"
}
extern lv_obj_t *ui_btnPay;
extern lv_obj_t *ui_lblMsg;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_RGB   _panel;
  lgfx::Bus_RGB     _bus;
  lgfx::Light_PWM   _light;
  lgfx::Touch_GT911 _touch;
public:
  LGFX() {
    { // painel 800x480
      auto pc = _panel.config();
      pc.memory_width  = 800;
      pc.memory_height = 480;
      pc.panel_width   = 800;
      pc.panel_height  = 480;
      _panel.config(pc);
    }
    { // === PINOS e TIMINGS (iguais ao projeto que funciona) ===
      auto bc = _bus.config();
      bc.panel = &_panel;

      // Data RGB565
      bc.pin_d0  = 15;
      bc.pin_d1  = 7;
      bc.pin_d2  = 6;
      bc.pin_d3  = 5;
      bc.pin_d4  = 4;
      bc.pin_d5  = 9;
      bc.pin_d6  = 46;
      bc.pin_d7  = 3;
      bc.pin_d8  = 8;
      bc.pin_d9  = 16;
      bc.pin_d10 = 1;
      bc.pin_d11 = 14;
      bc.pin_d12 = 21;
      bc.pin_d13 = 47;
      bc.pin_d14 = 48;
      bc.pin_d15 = 45;

      // Controle
      bc.pin_henable = 41;   // DE
      bc.pin_vsync   = 40;   // VSYNC
      bc.pin_hsync   = 39;   // HSYNC
      bc.pin_pclk    = 42;   // PCLK

      // Timings do seu projeto
      bc.freq_write        = 14000000;  // 14 MHz
      bc.hsync_front_porch = 40;
      bc.hsync_pulse_width = 48;
      bc.hsync_back_porch  = 40;
      bc.vsync_front_porch = 13;
      bc.vsync_pulse_width = 2;
      bc.vsync_back_porch  = 32;
      bc.pclk_idle_high    = 1;         // mantém igual

      _bus.config(bc);
      _panel.setBus(&_bus);
    }
    { // backlight
      auto lc = _light.config();
      lc.pin_bl = 2;
      lc.invert = false;
      lc.freq   = 48000;
      _light.config(lc);
      _panel.light(&_light);
    }
    { // Touch GT911 iguais ao projeto
      auto tc = _touch.config();
      tc.i2c_port = 0;
      tc.pin_sda  = 19;
      tc.pin_scl  = 20;
      tc.freq     = 400000;
      tc.i2c_addr = 0x14;
      tc.pin_int  = -1;
      tc.pin_rst  = 38;
      tc.x_min = 0;  tc.x_max = 799;
      tc.y_min = 0;  tc.y_max = 479;
      _touch.config(tc);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
} lcd;

// ===== LVGL <-> LovyanGFX glue =====
static const uint16_t SCREEN_W = 800;
static const uint16_t SCREEN_H = 480;

// buffers de desenho (40 linhas)
static lv_color_t buf1[SCREEN_W * 40];
static lv_color_t buf2[SCREEN_W * 40];
static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t      disp_drv;
static lv_indev_drv_t     indev_drv;
static unsigned long      last_lv_tick = 0;

static void disp_flush_cb(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *p) {
  int32_t w = (a->x2 - a->x1 + 1);
  int32_t h = (a->y2 - a->y1 + 1);
  lcd.pushImage(a->x1, a->y1, w, h, (const uint16_t *)&p->full);
  lv_disp_flush_ready(d);
}

static void touch_read_cb(lv_indev_drv_t *, lv_indev_data_t *data) {
  lgfx::touch_point_t tp;
  bool touched = lcd.getTouch(&tp);
  if (touched) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = (tp.x < 0) ? 0 : (tp.x >= SCREEN_W ? SCREEN_W - 1 : tp.x);
    data->point.y = (tp.y < 0) ? 0 : (tp.y >= SCREEN_H ? SCREEN_H - 1 : tp.y);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// compat LVGL 8.x (evita erro com lv_tick_inc ausente)
static void lv_tick_update() {
  unsigned long now = millis();
  uint32_t diff = now - last_lv_tick;
  last_lv_tick = now;
  lv_timer_handler_run_in_period(diff);
}

// ===== Evento do SquareLine =====
static void on_btnPay(lv_event_t *e) {
  LV_UNUSED(e);
  if (ui_lblMsg) lv_label_set_text(ui_lblMsg, "Funcionando...");
}

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(255);
  lcd.setSwapBytes(true); // se cores inverterem, mude para false

  lv_init();
  lv_disp_draw_buf_init(&disp_buf, buf1, buf2, sizeof(buf1)/sizeof(buf1[0]));

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = SCREEN_W;
  disp_drv.ver_res  = SCREEN_H;
  disp_drv.flush_cb = disp_flush_cb;
  disp_drv.draw_buf = &disp_buf;
  lv_disp_drv_register(&disp_drv);

  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touch_read_cb;
  lv_indev_drv_register(&indev_drv);

  last_lv_tick = millis();

  // cria UI do SquareLine
  ui_init();

  // conecta o botão
  if (ui_btnPay) lv_obj_add_event_cb(ui_btnPay, on_btnPay, LV_EVENT_CLICKED, NULL);
  if (ui_lblMsg) lv_label_set_text(ui_lblMsg, ""); // inicial
}

void loop() {
  lv_tick_update();
  lv_timer_handler();
  delay(5);
}
