# Real-Device Checklist (what simulation cannot verify)

The simulator is a host UI/event harness (evidence label: **Host-tested**).
It must never be cited as hardware verification. Before any release that
touches device behavior, check each item below on a physical AI Passport and
record: firmware SHA, simulator SHA, device model, scenario, network path,
logs, and what was NOT tested.

| # | Item | Why simulation cannot cover it | How to verify on device |
|---|------|-------------------------------|-------------------------|
| 1 | LCD color, contrast, font readability | Host renders with SDL + Montserrat 14; real ST7789 gamma, backlight, and viewing angle differ; CJK renders as placeholder boxes in the simulator | Read all golden screens on device in normal indoor light; confirm captions legible at arm's length |
| 2 | Physical UP/DOWN/OK buttons | Simulator synthesizes PRESS/CLICK/DOUBLE/LONG from keyboard/mouse; ADC resistor-ladder voltages, key noise, debounce, and wake-on-GPIO timing are not modeled | Press each key short/long/double on device; confirm menu and PTT behavior |
| 3 | Microphone / speaker / codec path | No ES8311 model, no audio capture or playback; TTS sentences are text fixtures | Run a full PTT turn on device; confirm record, playback, volume, and echo behavior |
| 4 | Real network path | Transport symptoms are injected notes; no Wi-Fi RF, router/proxy/NAT, UDP jitter/loss, server availability, or credentials | Run PTT + interruption scenarios on the target Wi-Fi network; record route and server |
| 5 | Battery gauge and power behavior | Battery/power are virtual-time notes; no CW2017 measurement, charging state, current draw, or wake reliability | Drain/charge cycle; confirm low-battery alert, dim, sleep, and wake-key on device |
| 6 | ESP32-C3 resources | Host has gigabytes of RAM; no heap/stack pressure, DMA limits, or flash partition behavior | Check firmware build size report; soak-test long-text and repeated-turn scenarios on device |
| 7 | Rounded-glass optics | The 30 px mask is geometry only; physical glass curvature, lens distortion, and edge visibility differ | Compare golden screenshots against the physical glass; note any safe-area mismatch |
| 8 | Full-firmware boot and OTA | Simulator never links the board Application; no secure boot, OTA rollback, or deep-sleep current | Flash the release build; confirm boot, settings persistence across reboot, and OTA path if used |
| 9 | CJK font rendering | Simulator has no licensed CJK font (placeholder boxes); firmware ships its own font stack | Confirm Chinese/Latin mixed text, emoji, and truncation render correctly on device |

Provenance for this release:

- Firmware (`yangguo/xiaozhi-esp32`): `1636ac336a1f257bad4ba8d7e5ea04966aac836a`
- Official reference (`FoloToy/ai-passport`): `33d3d1d93a1125b356b47b6d83a7a60121be801e`
- LVGL: `v9.5.0` (`85aa60d18b3d5e5588d7b247abf90198f07c8a63`)
