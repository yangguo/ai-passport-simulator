// LVGL 9.5 presentation: widgets follow shared Layout rects; captions come
// from CaptionBuffers. No text is ever drawn outside the glass safe area.
//
// Fonts (P0): built-in Montserrat 14 only. CJK codepoints have no glyphs yet
// and are skipped by LVGL; mixed-text goldens pin that behavior until a
// licensed CJK font lands (see Task 10 docs).
#include "passport_sim/lvgl_shell.h"

#include <cstring>
#include <string>

#include "layout.h"
#include "lvgl.h"
#include "screen_rounding.h"

namespace passport_sim {
namespace {

bool lv_ready = false;

void FlushCb(lv_display_t* disp, const lv_area_t* /*area*/, uint8_t* /*px*/) {
  // Single full-frame buffer: LVGL renders directly into frame_.
  lv_display_flush_ready(disp);
}

const char* StatusText(PassportActivity a) {
  switch (a) {
    case PassportActivity::kListening:
      return "Listening";
    case PassportActivity::kThinking:
      return "Thinking";
    case PassportActivity::kSpeaking:
      return "Speaking";
    default:
      return "Idle";
  }
}

void PlacePlain(lv_obj_t* obj, const passport_widget_place_t& p) {
  lv_obj_set_align(obj, p.anchor == PASSPORT_ANCHOR_TOP_MID
                              ? LV_ALIGN_TOP_MID
                              : LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(obj, p.x, p.y);
}

lv_obj_t* PlainBox(lv_obj_t* parent, int32_t x, int32_t y, int32_t w,
                   int32_t h, lv_color_t border) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_size(box, w, h);
  lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, border, 0);
  lv_obj_set_style_pad_all(box, 0, 0);
  lv_obj_set_style_radius(box, 0, 0);
  return box;
}

}  // namespace

// Same mask point as firmware: the rounded glass zeroes flush output outside
// the visible span (firmware screen_rounding.c, mask on the RGB565 buffer).
void LvglShell::MaskFrame() {
  passport_mask_rgb565_area(
      reinterpret_cast<uint8_t*>(LvglShell::frame_), LvglShell::kWidth * 2, 0,
      0, LvglShell::kWidth - 1, LvglShell::kHeight - 1, LvglShell::kWidth,
      LvglShell::kHeight, PASSPORT_SCREEN_RADIUS);
}

uint16_t LvglShell::frame_[kWidth * kHeight];

struct LvglShell::Impl {
  Layout layout;
  lv_display_t* disp = nullptr;
  lv_obj_t* status = nullptr;
  lv_obj_t* face = nullptr;
  lv_obj_t* sub_cont = nullptr;
  lv_obj_t* wrap = nullptr;
  lv_obj_t* user = nullptr;
  lv_obj_t* assistant = nullptr;
  lv_obj_t* alert_box = nullptr;
  lv_obj_t* alert_label = nullptr;
  std::string last_user;
  std::string last_assistant;
  uint32_t page = 0;
  uint32_t page_timer_ms = 0;
  int32_t content_h = 0;
  int32_t viewport_h = 0;
};

LvglShell::LvglShell() : impl_(new Impl()) {
  if (!lv_ready) {
    lv_init();
    lv_ready = true;
  }
  Impl& m = *impl_;
  m.disp = lv_display_create(kWidth, kHeight);
  lv_display_set_buffers(m.disp, frame_, nullptr, sizeof(frame_),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(m.disp, FlushCb);

  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x17202A), 0);
  lv_obj_set_style_text_color(screen, lv_color_white(), 0);

  m.status = lv_label_create(screen);
  lv_obj_set_style_text_color(m.status, lv_color_hex(0xD9E7EC), 0);

  m.face = PlainBox(screen, 0, 0, 32, 32, lv_color_white());

  m.sub_cont = lv_obj_create(screen);
  lv_obj_set_scrollbar_mode(m.sub_cont, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(m.sub_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(m.sub_cont, 0, 0);
  lv_obj_set_style_pad_all(m.sub_cont, 0, 0);
  lv_obj_set_style_radius(m.sub_cont, 0, 0);
  lv_obj_set_style_clip_corner(m.sub_cont, true, 0);

  m.wrap = lv_obj_create(m.sub_cont);
  lv_obj_set_scrollbar_mode(m.wrap, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(m.wrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(m.wrap, 0, 0);
  lv_obj_set_style_pad_all(m.wrap, 0, 0);
  lv_obj_set_style_radius(m.wrap, 0, 0);

  m.user = lv_label_create(m.wrap);
  lv_label_set_long_mode(m.user, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(m.user, lv_color_hex(0x82BE2D), 0);
  m.assistant = lv_label_create(m.wrap);
  lv_label_set_long_mode(m.assistant, LV_LABEL_LONG_WRAP);

  m.alert_box = PlainBox(screen, 0, 0, 10, 10, lv_color_hex(0xE43B2F));
  m.alert_label = lv_label_create(m.alert_box);
  lv_obj_set_style_text_color(m.alert_label, lv_color_hex(0xE43B2F), 0);
  lv_obj_add_flag(m.alert_box, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* sim = lv_label_create(screen);
  lv_label_set_text(sim, "SIM");
  lv_obj_set_style_text_color(sim, lv_color_hex(0x55707A), 0);
  lv_obj_align(sim, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
}

LvglShell::~LvglShell() {
  if (impl_->disp) lv_display_delete(impl_->disp);
  delete impl_;
}

void LvglShell::set_alert(const char* text) {
  Impl& m = *impl_;
  lv_label_set_text(m.alert_label, text);
  lv_obj_clear_flag(m.alert_box, LV_OBJ_FLAG_HIDDEN);
  lv_refr_now(m.disp);
  MaskFrame();
}

void LvglShell::clear_alert() {
  lv_obj_add_flag(impl_->alert_box, LV_OBJ_FLAG_HIDDEN);
  lv_refr_now(impl_->disp);
  MaskFrame();
}

void LvglShell::apply_page() {
  Impl& m = *impl_;
  const int32_t pages =
      passport_subtitle_page_count(m.content_h, m.viewport_h);
  if (m.page >= static_cast<uint32_t>(pages)) m.page = 0;
  lv_obj_set_y(m.wrap,
               -passport_subtitle_page_offset(static_cast<int32_t>(m.page),
                                              m.content_h, m.viewport_h));
}

void LvglShell::render(const CaptionBuffers& captions, PassportActivity activity) {
  Impl& m = *impl_;
  const LayoutRects rects = m.layout.compute(activity);

  passport_widget_place_t status_place{};
  passport_status_bar_place(&status_place);
  PlacePlain(m.status, status_place);
  lv_label_set_text(m.status, StatusText(activity));

  // Expression placeholder: centered 32x32 idle, shrunk 16x16 top when
  // caption-priority.
  if (rects.caption_priority) {
    lv_obj_set_pos(m.face, kWidth / 2 - 8, rects.safe.y + 2);
    lv_obj_set_size(m.face, 16, 16);
  } else {
    lv_obj_set_pos(m.face, kWidth / 2 - 16, rects.safe.y + 24);
    lv_obj_set_size(m.face, 32, 32);
  }

  lv_obj_set_pos(m.sub_cont, rects.subtitle_viewport.x,
                 rects.subtitle_viewport.y);
  lv_obj_set_size(m.sub_cont, rects.subtitle_viewport.width,
                  rects.subtitle_viewport.height);

  const std::string user_text = std::string("U: ") + std::string(captions.user());
  const std::string ai_text =
      std::string("AI: ") + std::string(captions.assistant());
  if (user_text != m.last_user || ai_text != m.last_assistant) {
    m.last_user = user_text;
    m.last_assistant = ai_text;
    m.page = 0;
    m.page_timer_ms = 0;
  }
  lv_obj_set_width(m.user, rects.subtitle_viewport.width);
  lv_label_set_text(m.user, m.last_user.c_str());
  lv_obj_set_pos(m.user, 0, 0);
  lv_obj_update_layout(m.user);
  const int32_t user_h = lv_obj_get_height(m.user);
  lv_obj_set_width(m.assistant, rects.subtitle_viewport.width);
  lv_label_set_text(m.assistant, m.last_assistant.c_str());
  lv_obj_set_pos(m.assistant, 0, user_h);
  lv_obj_update_layout(m.assistant);
  m.content_h = user_h + lv_obj_get_height(m.assistant);
  m.viewport_h = rects.subtitle_viewport.height;
  lv_obj_set_size(m.wrap, rects.subtitle_viewport.width, m.content_h);
  apply_page();

  lv_refr_now(m.disp);
  MaskFrame();
}

void LvglShell::tick(uint32_t ms) {
  Impl& m = *impl_;
  lv_tick_inc(ms);
  lv_timer_handler();
  MaskFrame();
  m.page_timer_ms += ms;
  if (m.page_timer_ms >= kPageMs) {
    m.page_timer_ms = 0;
    ++m.page;
    apply_page();
    lv_refr_now(m.disp);
    MaskFrame();
  }
}

}  // namespace passport_sim
