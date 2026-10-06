// SDL shell: 240x320 device viewport plus a 40 px toolbarHud (window logical
// size 240x360, shown at x2 zoom). Device area renders layout rects from the
// shared Layout module; real LVGL pixels arrive in Stage 2 (Task 8).
//
// The window title always carries SIMULATED; stdout log lines too. This shell
// never opens serial/GPIO/audio devices.
//
// Exit codes: 0 ok, 2 bad usage, 3 invalid fixture.
#include <cstdlib>
#include <iostream>
#include <string>

#include <cstdint>

#include <SDL.h>

#include "passport_sim/button_input.h"
#include "passport_sim/lvgl_shell.h"
#include "passport_sim/scenario_runner.h"
#include "passport_sim/settings_store.h"
#include "passport_sim/virtual_clock.h"

namespace {

constexpr int kDeviceW = 240;
constexpr int kDeviceH = 320;
constexpr int kToolbarH = 40;
constexpr int kZoom = 2;

struct ToolbarButton {
  SDL_Rect rect;
  passport_sim::Button button;
  const char* label;
};

const char* ActivityName(PassportActivity a) {
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

const char* ButtonEventName(passport_sim::ButtonEvent e) {
  using passport_sim::ButtonEvent;
  switch (e) {
    case ButtonEvent::Press:
      return "press";
    case ButtonEvent::Click:
      return "click";
    case ButtonEvent::Double:
      return "double";
    case ButtonEvent::Long:
      return "long";
    default:
      return "-";
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::string scenario;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--scenario" && i + 1 < argc) {
      scenario = argv[++i];
    } else {
      std::cerr << "usage: passport-simulator --scenario <path>\n";
      return 2;
    }
  }
  if (scenario.empty()) {
    std::cerr << "usage: passport-simulator --scenario <path>\n";
    return 2;
  }

  passport_sim::VirtualClock clock;
  passport_sim::CaptionBuffers captions;
  passport_sim::ButtonInput buttons(clock);
  passport_sim::SettingsStore settings;
  passport_sim::TransportMock transport;
  passport_sim::AudioPipelineMock audio;
  passport_sim::ScenarioRunner runner(clock, captions, buttons, settings,
                                      transport, audio);
  const passport_sim::LoadResult loaded = runner.load(scenario);
  if (!loaded.ok) {
    std::cerr << "invalid fixture: " << loaded.error << "\n";
    return 3;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    return 2;
  }
  SDL_Window* window = SDL_CreateWindow(
      "AI Passport Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      kDeviceW * kZoom, (kDeviceH + kToolbarH) * kZoom, SDL_WINDOW_SHOWN);
  if (!window) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
    SDL_Quit();
    return 2;
  }
  SDL_Renderer* renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) {
    std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 2;
  }
  SDL_RenderSetLogicalSize(renderer, kDeviceW, kDeviceH + kToolbarH);
  SDL_Texture* device_tex = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, kDeviceW,
      kDeviceH);
  if (!device_tex) {
    std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << "\n";
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 2;
  }
  passport_sim::LvglShell shell;

  const ToolbarButton toolbar[] = {
      {{10, kDeviceH + 6, 60, 28}, passport_sim::Button::Up, "UP"},
      {{90, kDeviceH + 6, 60, 28}, passport_sim::Button::Down, "DOWN"},
      {{170, kDeviceH + 6, 60, 28}, passport_sim::Button::Ok, "OK"},
  };

  const char* video_driver = SDL_GetCurrentVideoDriver();
  const bool dummy_driver =
      video_driver && std::string(video_driver) == "dummy";

  bool quit = false;
  bool playing = true;
  bool mouse_held = false;
  passport_sim::Button mouse_button = passport_sim::Button::Ok;
  std::string last_title;
  Uint32 last_ticks = SDL_GetTicks();
  int frames = 0;

  while (!quit) {
    // Advance the virtual clock by real elapsed time every frame so that
    // press/release holds measure correctly in interactive mode.
    const Uint32 now_ticks = SDL_GetTicks();
    const Uint32 dt = now_ticks - last_ticks;
    last_ticks = now_ticks;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) {
        quit = true;
      } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
        passport_sim::Button b = passport_sim::Button::Ok;
        bool is_button = true;
        switch (ev.key.keysym.sym) {
          case SDLK_UP:
            b = passport_sim::Button::Up;
            break;
          case SDLK_DOWN:
            b = passport_sim::Button::Down;
            break;
          case SDLK_RETURN:
          case SDLK_KP_ENTER:
            b = passport_sim::Button::Ok;
            break;
          default:
            is_button = false;
            break;
        }
        if (is_button) {
          buttons.press(b);
        } else if (ev.key.keysym.sym == SDLK_SPACE) {
          playing = !playing;
        } else if (ev.key.keysym.sym == SDLK_n) {
          if (runner.next_at_ms() != UINT32_MAX) {
            clock.advance_to(runner.next_at_ms());
            runner.step();
          }
        } else if (ev.key.keysym.sym == SDLK_r) {
          runner.load(scenario);  // re-parse: full reset incl. log header
        } else if (ev.key.keysym.sym == SDLK_s) {
          std::cout << runner.log();
        }
      } else if (ev.type == SDL_KEYUP) {
        switch (ev.key.keysym.sym) {
          case SDLK_UP:
            buttons.release(passport_sim::Button::Up);
            break;
          case SDLK_DOWN:
            buttons.release(passport_sim::Button::Down);
            break;
          case SDLK_RETURN:
          case SDLK_KP_ENTER:
            buttons.release(passport_sim::Button::Ok);
            break;
          default:
            break;
        }
      } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        int lx = ev.button.x / kZoom;
        int ly = ev.button.y / kZoom;
        for (const auto& t : toolbar) {
          if (lx >= t.rect.x && lx < t.rect.x + t.rect.w && ly >= t.rect.y &&
              ly < t.rect.y + t.rect.h) {
            buttons.press(t.button);
            mouse_held = true;
            mouse_button = t.button;
          }
        }
      } else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) {
        if (mouse_held) {
          buttons.release(mouse_button);
          mouse_held = false;
        }
      }
    }

    if (playing) {
      const uint32_t target = clock.now_ms() + dt;
      while (runner.next_at_ms() <= target) {
        clock.advance_to(runner.next_at_ms());
        runner.step();
      }
      clock.advance_to(target);
    } else {
      clock.advance_to(clock.now_ms() + dt);
    }

    // LVGL device viewport: same pixels the headless --screenshot path writes.
    shell.render(captions, runner.activity());
    shell.tick(dt);
    SDL_UpdateTexture(device_tex, nullptr, shell.framebuffer(), kDeviceW * 2);
    SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    SDL_RenderClear(renderer);
    SDL_Rect device_rect{0, 0, kDeviceW, kDeviceH};
    SDL_RenderCopy(renderer, device_tex, nullptr, &device_rect);
    // Toolbar buttons (pressed button filled).
    for (const auto& t : toolbar) {
      const bool active =
          mouse_held && mouse_button == t.button &&
          (buttons.last_button() == t.button);
      SDL_SetRenderDrawColor(renderer, active ? 90 : 45, active ? 160 : 60,
                             active ? 90 : 70, 255);
      if (active) {
        SDL_RenderFillRect(renderer, &t.rect);
      } else {
        SDL_RenderDrawRect(renderer, &t.rect);
      }
    }
    SDL_RenderPresent(renderer);

    const std::string title =
        std::string("AI Passport Simulator - SIMULATED | ") +
        ActivityName(runner.activity()) + " | key=" +
        ButtonEventName(buttons.last_event());
    if (title != last_title) {
      SDL_SetWindowTitle(window, title.c_str());
      last_title = title;
    }

    if (dummy_driver && ++frames >= 30) quit = true;  // headless smoke path
    SDL_Delay(16);
  }

  SDL_DestroyTexture(device_tex);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
