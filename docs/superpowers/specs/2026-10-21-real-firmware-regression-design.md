# Real-Firmware Regression Testing for AI Passport (spec)

- Date: 2026-10-21
- Scope: ESP32-C3 firmware regression on Ubuntu CI, integrated with the existing host harness; game corpus first, XiaoZhi conversation coverage behind a compatibility spike
- Status: M1 source lock/fetch wiring is implemented; M2 harness CI gate, firmware-bound screenshot goldens, warning allowlists, and fail-red offline checks are implemented. The three-game two-repeat local gate passed on 2026-10-23; evidence is in `/workspace/m2-runs/REPORT.md`. The first GitHub workflow run remains pending. xiaoxiaole text quality still fails on known tofu and is reported separately from navigation.
- Approach: retain fast host tests and add a separate headless passportsim execution backend with shared scenario intent and firmware-specific oracles

## 1. Goals and non-goals

Goals:

- Execute actual AI Passport flash images, including their ESP-IDF application and display path, in repeatable Linux CI jobs.
- Catch boot, UP/DOWN/OK routing, menu/game navigation, display, text, and firmware integration regressions that the host-built UI cannot catch.
- Reuse the versioned JSON fixture vocabulary and review workflow around `ScenarioRunner`, without treating host mocks as firmware execution.
- Make the previous Linux batch experiment reproducible from tracked inputs: emulator source pin, batch support, `runone.sh`, `corpus.toml`, firmware artifacts, and expected results.
- Exercise real XiaoZhi protocol handling with scripted ASR/STT, TTS, interruption, and network failure events without production services, credentials, or cloud inference.
- Produce useful failure artifacts and explicit coverage labels; fail on new emulator warnings and unsupported required capabilities.

Non-goals:

- Replacing the SDL/LVGL host harness, its unit tests, or firmware repositories' build/size gates.
- Proving physical RF, BLE interoperability, NFC, ADC noise/debounce accuracy, microphone quality, speaker/codec fidelity, battery measurements, power consumption, or wake reliability. These remain device tests under `docs/TEST_STRATEGY.md`.
- Building a general ESP32 emulator, supporting every game immediately, or adding browser automation as the primary CI path.
- Running real ASR, TTS synthesis, or an LLM. The local protocol peer supplies reviewed text and small licensed audio fixtures.
- Claiming unmodified IDF v6.1 XiaoZhi coverage before a matching build and radio path have passed the spike.

## 2. Evidence and gap map

### 2.1 Baseline and provenance

The following is supplied experimental evidence, not a test run performed for this design. The prior local prototype and its output artifacts are no longer on disk; its results must be reproduced before becoming a CI baseline.

| Firmware | Supplied identity | IDF | Observed result |
|---|---|---|---|
| Go | v0.1.1 / `8a588c5` | 5.5.3 | Boots to menu; UP/DOWN/OK respond |
| chess | `93dd170` | 5.5.3 | Boots to menu; UP/DOWN/OK respond |
| xiaoxiaole | `cac7eb1` | 5.5.3 | Boots to menu; UP/DOWN/OK respond; digits render as tofu although tree text is correct |
| XiaoZhi main firmware | main source, no tested commit/image supplied | 6.1 | Untested; no prebuilt image |

The game experiment ran approximately 37 seconds of emulated time in 1.5 seconds of wall time and repeated deterministically for the same inputs. This is feasibility evidence for those runs, not a general performance or determinism guarantee. Abbreviated source identities must be resolved to full commits and exact artifact hashes. Resolved LVGL versions must come from each build's lockfile; do not infer them from the IDF version.

The emulator assessment is scoped to BlackHole1/passportsim v1.0.3 and the supplied experiment. Its public repository describes a Rust implementation, MIT license, CLI/MCP interface, and macOS/Windows native use; Linux support is a proposed addition here. See [passportsim](https://github.com/BlackHole1/passportsim). The other reference projects are [VOID001/FoloToy-Passport-Simulator](https://github.com/VOID001/FoloToy-Passport-Simulator) and [espressif/esp-emulator](https://github.com/espressif/esp-emulator). Their moving default branches are background references, never CI dependency pins.

### 2.2 Gaps and disposition

| Gap | How addressed, or explicitly deferred |
|---|---|
| passportsim v1.0.3 daemon platform layer supports macOS/Windows, with no official Linux path | Add an Ubuntu headless batch path that does not require a daemon; upstream Linux platform handling separately where needed. Use a pinned integration fork until upstream changes are available (§3). |
| Local `PASSPORTSIM_BATCH` patch, `PASSPORTSIM_HOME`, `runone.sh`, and `corpus.toml` are missing | Re-create their specified contracts as tracked source and acceptance fixtures; no dependency on a developer's workspace (§3.2). |
| `ui` tree inspection works on LVGL 9.5 but fails on 9.6: `struct _lv_obj_t has no member flags` | Derive field/bitfield access from the matching ELF DWARF; explicit inspection fallback and capability failures (§6). |
| Wi-Fi/BLE support only for IDF v5.5.3 in the assessed passportsim version | Use 5.5.3 games first; time-box 6.1 radio compatibility and alternate XiaoZhi builds. BLE functionality remains outside the initial gate (§9). |
| No server-free conversation event injection | Add a local fake XiaoZhi server with scripted WebSocket and MQTT+UDP peers through the emulated network (§9). |
| Fixed boot detection produces timeout false negatives | Match per-firmware UART readiness and/or validated frame-stability markers, with independent hard budgets (§7). |
| Unmodelled-register warnings produce exit 10 | Preserve raw exit status; accept only reviewed per-firmware warning signatures and bounded counts; fail new warnings (§7). |
| VOID001 simulator is browser-oriented, with a closed custom esp-emu WASM core and no firmware CLI/headless golden workflow in the assessment | Do not base the CI backend on it. Its README documents missing battery-gauge and BLE models; battery/BLE/NFC coverage remains deferred. See [upstream limitations](https://github.com/VOID001/FoloToy-Passport-Simulator#readme). |
| espressif/esp-emulator distributes a closed core without the Passport display/audio/external-chip models needed here | Keep as a future alternative; no dependency in the first gate. Its native Linux/networking support alone does not supply those board models. See [upstream feature inventory](https://github.com/espressif/esp-emulator#features). |
| This harness runs shared host UI, SDL/LVGL, transport/audio mocks, and goldens, rather than firmware | Add firmware execution alongside existing CTest checks; keep host and firmware results separate (§3–5). |
| Host goldens contain CJK tofu/missing glyphs; xiaoxiaole digits are also tofu | Fix host font coverage and firmware font selection separately; never mask text or bless tofu as correct rendering (§10). |
| Physical peripherals, radio fidelity, battery/power, and NFC remain unverified | Explicitly deferred to device evidence; emulated success never promotes a result to Device-tested. |

## 3. Architecture and reproducible Linux execution

### 3.1 Backend choice and integration

Recommend an upstreamable Linux/batch-mode PR, delivered initially through a minimal pinned fork. Upstream-only delivery would make the gate depend on review timing; a permanent independent fork would accumulate emulator maintenance. Keep the fork delta limited to batch execution, Linux headless dependencies, machine-readable output, inspection fixes, and later compatibility work. Submit those as separable upstream changes. Once accepted, move to a full upstream commit only after the corpus passes unchanged.

Pin the full 40-character emulator commit, upstream base commit corresponding to v1.0.3, Rust toolchain, Cargo lockfile, Linux build image digest, and any separately loaded ROM/model assets with hashes. A release tag is provenance, not the checkout selector. The lost prototype was uncommitted and has no patch commit to pin; milestone M0 must resolve the upstream base and create a reviewable fork commit. Never invent a SHA or run against `main`/`latest` while calling it pinned.

The existing host executable retains its shared presentation modules and mocks. A separate firmware runner consumes validated scenario intent, drives the emulator, and collects observations. It does not link firmware into the host process or invoke `ConversationEvents` to make firmware appear to pass.

Data flow:

1. The corpus resolver verifies the firmware image, ELF, manifest, and pinned emulator build.
2. The scenario validator and backend capability check accept the entire run before starting it.
3. The adapter translates key events and, later, conversation scripts into emulator input and local peer actions on emulated time.
4. One emulator process owns a fresh machine for the complete scenario. UART, framebuffer, optional LVGL tree, protocol trace, and warning observations feed checkpoints.
5. A result evaluator checks readiness, semantics, pixels, warning policy, exit status, determinism, and budgets. CI publishes the result with provenance.

Use the existing Ubuntu 24.04 CI family, with a pinned userspace/container for firmware jobs. Batch mode needs no SDL window, desktop session, daemon, or GPU. Keep the current host CTest job independent. A firmware smoke job becomes required after M2; heavier corpus and protocol matrices can initially run nightly and on affected changes.

### 3.2 Re-create the missing prototype

The proposed file names below are implementation deliverables, not files supplied by this documentation change.

- Re-create the batch entry path in `crates/pemu-cli/src/main.rs` from the pinned source. Preserve `PASSPORTSIM_BATCH` as the compatibility switch: value `1` selects batch mode and reads a bounded, versioned JSON command document from standard input; unset preserves normal CLI behavior; other values fail validation. The lost prototype's exact command grammar is unknown, so this is the replacement contract.
- Batch mode directly constructs the emulator and reuses command handlers for load, advance, button press/release, frame capture, optional tree inspection, and finish. It must not spawn/reconnect a daemon for each command or reset between commands. Invalid commands fail before execution; runtime failures stop the sequence and identify the command index.
- Structured responses carry command index, emulated timestamp, outcome, and output paths. UART and emulator warnings remain distinguishable from command results. Shutdown flushes artifacts and reports raw exit status, including warning exit 10.
- `PASSPORTSIM_HOME` selects an absolute, job-private state directory for every persistent path that batch execution uses. CI sets it to a fresh temporary directory and fails if state escapes it. Reset means a fresh image/NVS copy and machine, not only a cleared screenshot buffer.
- Track `tools/firmware/runone.sh` as a thin driver: select a corpus entry and scenario, verify hashes and capabilities, allocate isolated state, translate the scenario, invoke one batch process, evaluate results, and clean up. Preserve diagnostic artifacts on failure and terminate child peers on cancellation. The driver must not contain game-specific expected behavior.
- Track `tests/firmware/corpus.toml` as the reviewed inventory: repository and full source commit, artifact run/release identity, hashes, manifest path, scenario set, readiness profile, capabilities, LVGL inspection requirement, warning allowlist, fonts/known issues, and golden set. Batch commands are generated from these tracked inputs, never hand-edited in CI.

M0 must document the actual invocation and JSON command schema after it has been verified on a clean Ubuntu checkout. This spec deliberately advertises no already-working shell command. Its acceptance sequence is load → advance to readiness → press/release DOWN → UP → OK → capture/check → finish, all in one machine, with a second identical run from fresh state.

## 4. Firmware corpus and artifact sourcing

Each game repository's CI owns firmware compilation. The M1 inventory is tracked in `tools/passportsim/firmware-sources.toml`; the fetcher selects these exact immutable sources and never resolves a moving branch:

| Corpus | Repository and source | Published input | Current status |
|---|---|---|---|
| `gov011` | [yangguo/ai-passport-go](https://github.com/yangguo/ai-passport-go), tag `v0.1.1`, source `0762e467c8b1759294aa78694fa918f81683930c` | Release asset `ai-passport-go-v0.1.1-release.zip` | Two fresh runs passed boot, framebuffer navigation, and screenshot determinism. No ELF in the release or inspected CI zips, so `ui` tree inspection remains unavailable. |
| `chessfd8fa19` | [yangguo/ai-passport-chess](https://github.com/yangguo/ai-passport-chess), run `38233745799`, source `fd8fa19718acb60971bcb33ef9738c1d959d4914` | Artifact `firmware-09ab758e7edb5fb57b230a26b98d7f1f9aab3a12` (PR artifact name uses the merge commit SHA; `fd8fa19` is the PR head source) | Two fresh runs passed boot, framebuffer navigation, and screenshot determinism. Artifact contains `.elf`; `ui` inspection is unsupported for its LVGL 9.6 layout. It currently expires 2027-01-10. |
| `xxlaba64c1` | [yangguo/ai-passport-xiaoxiaole](https://github.com/yangguo/ai-passport-xiaoxiaole), run `38265044174`, source `aba64c1a41613e8cbc60289dd9c57d1f431bed5a` | Artifact `firmware-aba64c1a41613e8cbc60289dd9c57d1f431bed5a` | Two fresh runs passed boot, framebuffer navigation, and screenshot determinism; `ui` tree inspection passes. The known tofu/digit issue remains. Artifact currently expires 2027-01-10; this run is from `feat/level-variety`, because its firmware workflow is not yet on `main`. |

The Go release asset has no scheduled CI-artifact expiry, but it remains dependent on the release being retained. Chess and xiaoxiaole are ordinary workflow artifacts and expire on the dates above unless promoted. Fetch failures on expired/missing artifacts direct maintainers to pin and review a replacement source. The local fetch cache is private (`0700`) and defaults to `$PASSPORTSIM_FIRMWARE_STORE` or `~/.cache/apsim/firmware`; firmware bytes must never enter this repository, consistent with Decision 2 below. The lock records SHA-256 for each consumed bin/ELF, and the run manifest records repository, run/tag, source commit, and actual hashes.

Every artifact bundle contains:

- `merged.bin`, including the bootloader, partition table, application, and required flash resources at recorded offsets; flash size and board must identify ESP32-C3 AI Passport.
- The matching unstripped application ELF with DWARF and symbol information. Record the application image digest/build identity so an ELF from another build is rejected even if its source SHA matches.
- A versioned manifest containing repository URL, full source SHA/tag, producer workflow/run ID, build variant, ESP-IDF commit/version, resolved LVGL commit/version, dependency-lock and sdkconfig hashes, compiler/toolchain and builder image digest, merge layout, and font/asset provenance.
- SHA-256 for every payload, including external resources and deterministic NVS template if present. The corpus pins the manifest's own SHA-256 and immutable artifact identity outside the bundle, so hashes inside a replaced bundle cannot authorize themselves.

Downloads use exact workflow run/artifact IDs or immutable release assets with expected hashes. Never select the most recent successful run by branch name. Preserve promoted bundles in an approved durable store because CI artifacts expire; license review covers firmware, fonts, artwork, and emulator assets independently of passportsim's MIT license. Caches are keyed by verified content hashes, not game names.

If the historical CI run lacks ELF/DWARF, rebuild that exact source using recorded dependencies and publish a new bundle with its own identity. Do not describe the rebuilt binary as the lost experimental binary or reuse its missing hash. A missing required image, ELF, or manifest is a failed corpus-resolution gate; optional entries report unsupported with a reason, never a green execution result.

New firmware promotion is a reviewed corpus change. Keep the prior baseline available for comparisons. Candidate artifacts can be tested against the current golden set before deciding whether a behavior change is intentional. XiaoZhi needs its own source build and manifest before any conversation result can count.

## 5. Scenario reuse and backend semantics

### 5.1 Common contract and adapter

Reuse `tests/scenarios/*.json`: `scenario_version: 1`, non-empty `name`, optional object `meta`, and ordered `events` with `at_ms`. `src/scenario_runner.cc` currently allows 4,096 events, unsigned 32-bit timestamps, equal timestamps in file order, UTF-8 text up to 8 KiB, and only the known top-level/event fields. Preserve that contract and its rejection-before-playback behavior. Reuse validation through a shared parser or a tested equivalent; do not broaden the host parser merely to accept firmware controls.

Put backend bindings, checkpoints, readiness, and capability requirements in a versioned corpus-referenced sidecar. They reference the same scenario file/hash and event indices. Do not insert new `wait`, `capture`, or protocol-specific event types into v1 fixtures. Reject a missing binding for any event requiring firmware behavior before booting. Reports enumerate consumed events, assertions, and any explicitly unsupported scenario.

| Existing event | Host behavior | Firmware adapter behavior |
|---|---|---|
| `key` press/release | `ButtonInput` synthesizes click/long/double | Drive the modeled Passport button/ADC input for the specified emulated duration; firmware performs classification. If only click-level input exists, implement press/release or declare the capability unsupported. |
| `note` | Log annotation | Annotation only; never proof of a battery or sleep transition |
| `stt`, `tts_start`, `tts_sentence`, `tts_stop` | Direct caption/activity injection | Fake server emits the pinned firmware protocol's matching messages/audio; observe firmware text and state (§9) |
| `state` | Direct presentation state input | Treat as an expected state at a bounded checkpoint; initiate listening using an explicit key/session binding where needed. Never write firmware state memory. |
| `transport` | Mock channel/error/deadline | Apply a bound peer failure/recovery script, with explicit failure layer and expected outcome; a server disconnect is not proof of RF loss |
| `audio_packet` | Queue counter manipulation | Initially host-only; later map only supported incoming packet faults to real encoded traffic. Encode/send saturation requires a separate supported mechanism and cannot be inferred from packet counts. |

Games initially use the key/note subset and game-specific navigation fixtures. Reuse `menu-open-close.json` as an input contract where appropriate, but its long UP hold and OK meaning must be checked per game. Host and firmware need not produce the same menu or image. Existing battery and dim/sleep fixtures mostly use `note`; they do not provide firmware assertions.

### 5.2 Time, checkpoints, and observations

Firmware `at_ms` is relative to the declared scenario-ready barrier, with the absolute boot/emulator timestamp recorded. Host scenarios still start at virtual time zero. The adapter advances CPU, timers, and display work between events; it cannot skip execution by assigning the host virtual clock. Equal-time events retain file order. A checkpoint captures only after due input, guest work, and display flushes have run under a bounded settling rule.

The current host timeout implementation polls at the next fixture event; a trailing timeout does not fire merely because a screenshot uses `--at-ms`. Preserve that behavior and use explicit later events in shared timeout fixtures. Real firmware timers run continuously, so firmware expectations use a documented deadline/window rather than claiming byte-identical timing across backends. Unsupported timing equivalence is reported as such.

Assert observed menu selection, expected label/caption text, bounds/visibility, state markers, and final recovery in addition to pixels. Observations come from framebuffer, safely read tree, UART, and protocol traces. Log normalized semantic results separately from raw UART. Add **Firmware-emulated** as a report label alongside the existing Simulated, Host-tested, Firmware-built, and Device-tested labels; simulated network/peripheral inputs are listed explicitly. This extends reporting vocabulary without claiming device validation.

## 6. LVGL inspection, goldens, and masks

### 6.1 LVGL 9.6 tree support

Replace assumptions about `_lv_obj_t.flags` and hardcoded object layout with type resolution from the exact ELF's DWARF. Resolve typedefs, nested records, members, pointer widths, child storage, and bitfield location/width using the target's endianness and DWARF conventions. Locate the actual representation used by the resolved 9.5 or 9.6 build; do not assume a renamed field is sufficient. Symbols locate roots and class/text data only where their representation is known for that build.

Generate an inspection descriptor keyed by ELF SHA-256, LVGL version, and extractor version. Validate member bounds, RAM/flash address ranges, child counts, traversal depth, string length/UTF-8, and cycles. Pause the guest at a safe checkpoint before traversal to avoid reading half-updated objects. Never execute guest functions merely to fetch a label.

Missing/optimized-out DWARF or an unsupported layout produces `tree_unavailable`, not an empty successful tree. A reviewed layout descriptor generated from the same build can be a fallback when packaged with its ELF/config hashes; never fall back to an unverified 9.5 layout for 9.6. Otherwise use UART plus framebuffer assertions only for scenarios that explicitly permit them. Scenarios requiring caption/tree assertions fail capability validation until a debug artifact or validated descriptor exists. Keep LVGL 9.5 working while adding 9.6 conformance cases with known labels and flags.

### 6.2 Golden comparison

Firmware screenshots have a separate proposed namespace, `tests/firmware/golden/<firmware>/<baseline>/<scenario>/<checkpoint>.png`; host images remain under `tests/golden/`. Never compare a game or XiaoZhi framebuffer directly against the host UI as a pixel oracle. Pin display dimensions, orientation, RGB565-to-RGB888 conversion, capture timing, fonts, resources, and emulator version. Store the raw display and the reviewed visible 240×320 viewport; exclude desktop chrome.

Start with the existing `tools/compare_png.cc` rule: a pixel differs when the maximum absolute RGB channel difference exceeds 8; fail when more than 0.1% of compared pixels differ. Require dimensions to match. These are initial firmware thresholds to validate, not permission to increase tolerance until a flaky job passes. Deterministic repeat checks remain stricter: exact hashes for equivalent captures after declared normalization.

The current comparator has no region-mask support. Add a firmware comparison wrapper or explicit comparator extension during implementation; this design does not assume the capability already exists. A versioned mask file uses viewport-coordinate rectangles or an exact-size bitmap and records owner, reason, and affected checkpoint. Apply it identically to expected/actual and compute the percentage over unmasked pixels only. Reject out-of-bounds, wrong-size, or all-masked inputs and report the masked area. Apply the fixed rounded-glass mask only for physically invisible pixels; assert safe geometry separately.

Prefer fixing clocks/seeds and disabling irrelevant animations deterministically over masking. A small clock/blink region may be masked with review; captions, menu selection, game digits, alerts, and crash indicators must remain visible to the oracle. No automatic learned masks or masks generated from failing pixels. Text assertions remain required even when pixels pass.

Golden updates require a PR with source/artifact identities, expected/actual/diff images, masks, semantic assertions, and a reason tied to the intended behavior. Review firmware, emulator, and font changes independently where possible. CI never auto-accepts new images. A known broken tofu baseline may be retained only as a clearly labeled diagnostic artifact with an owner and issue; it cannot satisfy the readable-text acceptance gate. Prove the gate by deliberately changing a menu label or selection and observing failure.

## 7. Readiness, exit status, and warning policy

Boot progress and scenario readiness are distinct. A generic bootloader/application UART message is not proof that a menu is interactive. Each corpus readiness profile declares an existing UART menu/application marker and/or a frame condition: nonblank expected anchor/content plus three stable samples 100 ms apart in emulated time. A static black screen or a frozen crash screen cannot pass stability alone. Animated menus use a stable region/text anchor instead of full-frame equality. Optional test-build UART markers are allowed only when the manifest labels the instrumentation.

Do not fail solely because passportsim's old boot heuristic expires if the explicit readiness profile passes. Record the heuristic discrepancy. Conversely, ignore neither guest panics/reset loops nor a missing required marker. Start with a 60-second emulated boot budget, a 120-second emulated scenario budget, and a 60-second wall watchdog per run. These are safety bounds, not sleeps or readiness signals; slow hosts may hit the wall bound with a distinct diagnostic. On timeout capture last UART, frame sequence, emulator time/progress, warnings, and completed command index.

Raw exit 0 is necessary only when no warning policy applies; it is never sufficient for success. Exit 10 is accepted solely when all checkpoints pass, the run completed normally, and every unmodelled-register warning matches that firmware's reviewed allowlist. Distinguish clean pass from pass-with-known-warnings in reports and preserve raw exit 10. Every other nonzero exit, signal, panic, incomplete output, or missing artifact fails.

Allowlist entries identify register/address, operation (read/write and width), stable warning category, rationale, issue/owner, expiry/review date, and maximum count per run. Scope them to the firmware/build and emulator pin. Normalize only volatile presentation fields, not addresses or warning categories. Fail on a new signature or count above its bound; flag disappeared warnings for cleanup. A truncated/unparseable warning stream fails evaluation. No blanket suppression of exit 10 or wildcard register ranges. Re-review on any emulator or firmware upgrade, and include a deliberately introduced warning in the fail-red checks.

## 8. Determinism and CI resource limits

Start every run from an immutable image plus a hashed NVS/config template copied into private storage. Fix emulator time/RTC epoch, machine identity/MAC, RNG seed where supported, button timing, and game seed/settings. Do not share saved flash, cache directories containing mutable guest state, or protocol sessions between tests. If the emulator cannot control a required entropy source, record the limitation and add deterministic emulation hooks or use invariant assertions; do not claim reproducible full-frame goldens for that entry.

Drive the fake peer from the emulator scheduler: pause at event/network barriers, drain relevant local I/O, queue scripted responses, then advance the guest in fixed bounded quanta. Host socket delivery must not decide guest-visible timestamps. A small control channel carries emulator time and barrier acknowledgements. Proving this scheduling hook is part of the conversation spike; an uncontrolled wall-clock server cannot become a deterministic required golden gate.

For each promoted corpus entry, run ten fresh replays on Ubuntu and compare normalized semantic logs, warning signatures/counts, protocol order, checkpoint times, and screenshot hashes. Repeat representative cases under host CPU contention. Preserve raw traces; normalization may remove host paths/run IDs and declared volatile fields only. Do not normalize away guest resets, packet ordering, missing events, or UI differences. Firmware and host logs are compared within their respective backend, not byte-for-byte against each other.

Initial CI allocation:

| Resource | Proposed bound / policy |
|---|---|
| Runner | Ubuntu 24.04 x86-64; pinned container, emulator/toolchain and dependency hashes |
| Parallelism | At most two emulator processes per job; independent scenario state and peer ports |
| Per-process memory | 1 GiB emulator; 512 MiB local peer; allocation failure/OOM is a failed run |
| Time | Per-run budgets in §7; 10-minute firmware smoke job timeout including setup |
| PR set | Three games, one boot/navigation scenario each, two fresh repetitions; measure and target under five minutes after cache warm-up |
| Extended set | Nightly full scenarios, ten-run determinism checks, LVGL 9.5/9.6 cases, and supported protocol matrix |
| Storage | At most 100 MiB output per scenario and 1 GiB per job; stop excessive traces with explicit failure rather than silent truncation |
| Retention | Diagnostics 14 days; promoted corpus bundles durable and content-addressed; cache eviction must not change behavior |

The observed 37s/1.5s ratio is not used to size guarantees. Measure cold builds/downloads and warm execution separately at M1/M2. Cache verified emulator builds and immutable artifact bundles. Capture checkpoints rather than continuous video; full instruction traces are opt-in bounded diagnostics. Cancellation kills the emulator and local peers and removes temporary state. PRs receive no production secrets; artifact selection is reviewed and test execution has only the network route needed for the isolated peer.

## 9. Server-free XiaoZhi conversation injection

### 9.1 Local protocol peer

Here, server-free means no external service dependency. A test-local fake XiaoZhi server is still required. It runs beside the emulator and implements the exact protocol contracts read from the selected XiaoZhi source commit, with separate scripted WebSocket and MQTT-control/UDP-audio modes. The MQTT mode includes a private broker/control peer and UDP endpoint; the WebSocket mode includes the upgrade, hello/session setup, control messages, and binary audio framing that this firmware expects. Neither uses the harness's mock wire format.

Give the firmware deterministic Wi-Fi, endpoint, activation/provisioning, and test identity configuration through a documented test build or hashed NVS template. Stub any startup OTA/time/activation response needed to reach the session. If TLS is required, use a test CA and endpoint binding recorded in the build variant; do not silently bypass production certificate checks. No production tokens, server addresses, or account activation.

The peer must be reachable from the guest through passportsim's emulated network. Guest loopback is not the CI host. The spike must demonstrate guest-to-host TCP and UDP routing, DNS/endpoint resolution, and local peer readiness using an unprivileged Linux network path. A successful host-side socket connection is insufficient. Pin the emulator's supported gateway/forwarding configuration after verification; avoid requiring TAP privileges in normal PR CI. If a mapping or network bridge change is needed, it is an emulator deliverable rather than an assumed existing feature.

The scenario adapter compiles the same `stt` and TTS events into protocol messages and small valid encoded audio packets appropriate to the negotiated format. It waits for bounded session/listen prerequisites before releasing events and fails if their scheduled window is missed. `state` events assert the guest's observed lifecycle; sidecar bindings supply PTT keys/session actions for fixtures such as `ptt-normal.json` that currently start with direct host state injection. `tts_start` must cause real firmware playback handling, not only set a caption.

Run scripted cases for normal PTT, retained user STT, empty STT, mixed-text pagination, interruption, and disconnect/recovery during Thinking/Speaking. Map `transport.timeout` to withholding the specific response/heartbeat that the selected protocol times out on; map down/error/recover to a declared socket, broker, UDP, or handshake fault. Report actual firmware error text rather than forcing the fixture's mock reason into firmware memory. MQTT UDP loss/reordering can later map incoming `audio_packet` intents once framing, encryption/session values, and sequence semantics are verified. Exact mock queue counters and send-queue saturation remain host-only until an observable firmware test mechanism exists.

Assertions include guest connection and handshake, expected outbound listen/abort messages, incoming text accepted by the application, caption retention, TTS lifecycle, failure prompt, and successful next turn. Capture protocol transcripts with synthetic identities and payload hashes. Audio packet handling is firmware evidence; acoustic quality remains deferred.

### 9.2 IDF compatibility spike and decision gate

The assessed passportsim radio path supports IDF v5.5.3, whereas the requested XiaoZhi main firmware uses IDF v6.1 and has no tested prebuilt image. Treat this as a blocker to conversation claims, not an artifact-download inconvenience.

| Option | Value | Cost / evidence limit |
|---|---|---|
| Build selected XiaoZhi source against IDF 5.5.3 for tests | Fastest plausible use of the demonstrated radio path; retains application/protocol execution if dependencies permit | May require dependency/config patches or fail to build. Mark as a separate compatibility variant; it does not validate the production 6.1 binary. |
| Extend passportsim Wi-Fi/radio interception for IDF 6.1 | Preferred long-term path to testing the production source/toolchain with modeled networking | Unknown ABI/symbol/peripheral work; maintain a versioned compatibility table and upstream the changes. Never reuse 5.5.3 offsets blindly. |
| Test-only transport shim in a XiaoZhi test build | Fallback for application/caption lifecycle if network emulation is blocked | Bypasses some ESP-IDF/radio/possibly protocol layers. Label every bypass and keep the variant separate; it cannot satisfy the real-network protocol acceptance gate. |

Time-box the initial spike to three engineering days. First obtain a reproducible ESP32-C3 main build at a full source SHA and establish boot/display behavior. Then attempt the least invasive viable route, starting with a 5.5.3 compatibility build only if its dependency constraints permit, while measuring the 6.1 radio delta. Reserve the shim as an explicitly scoped fallback, not an automatic downgrade of the acceptance gate.

Pass criteria: a manifest-backed build boots, the guest reaches the local peer without external access, WebSocket completes two scripted STT/TTS turns including interruption and disconnect/recovery, and ten clean runs have identical normalized observations/checkpoint images. Demonstrate MQTT control plus UDP audio separately with a normal turn and one sequence/failure case before claiming that transport supported. Any protocol not demonstrated stays unsupported. Document wall time, warnings, entropy/scheduling controls, firmware patches, and layers bypassed.

At the time box, record one of: production 6.1 route passed; 5.5.3 compatibility route passed with production coverage still open; shim-only application coverage; or blocked with a concrete emulator/build issue. A blocked or partial spike does not delay the game smoke gate or silently turn XiaoZhi scenarios green. Estimate and seek a scope decision before expanding 6.1 emulator work beyond the spike.

## 10. Font correctness

The host shell currently uses placeholder font coverage, and existing mixed-text goldens preserve missing CJK behavior. Add a redistributable CJK font subset for the actual fixture character inventory, including Chinese punctuation, Latin, digits, and degree symbols; pin source license/hash, conversion tool/options, subset inventory, and generated artifact hash. Define explicit coverage/fallback for emoji separately. Review line metrics, wrapping, page count, caption retention, and safe bounds when changing fonts; a font substitution is a layout change, not merely new pixels.

Add a glyph-coverage preflight for supported fixture code points and a reviewed mixed Chinese/Latin/digit rendering checkpoint. Correct tree text alone does not prove readable glyphs. Update affected host goldens through the normal diff review after the font fix; keep that work distinct from firmware goldens.

For xiaoxiaole, correct LVGL tree text with tofu digits is supplied evidence of a firmware font issue. Inspect the actual label font/fallback selection, generated font tables, resource packaging, and digit coverage in that repository. Fix and rebuild there, publishing a new artifact/source identity and reviewing the changed framebuffer here. Do not patch the emulator screenshot, substitute host text, or mask the digits. The historical image can support boot/navigation diagnostics while its readable-digit check remains a tracked failure; promotion to a text-correct baseline requires visible digits on both emulated capture and a device spot check.

## 11. Phased delivery and acceptance

| Milestone | Deliverables | Exit criteria |
|---|---|---|
| M0 — source and experiment recovery | Resolve full emulator/game commits and game repository URLs; obtain or rebuild all three bundles; tracked batch path, driver, corpus, documented schema | Clean Ubuntu checkout loads each game and performs DOWN/UP/OK in one process without desktop/daemon; hashes and raw evidence recorded; no local prototype dependency remains |
| M1 — reliable execution | Source lock/fetcher and offline fake-`gh` test implemented. Two-repeat clean-box results are recorded in `/workspace/m1-runs/REPORT.md`; all three games pass boot, framebuffer-change navigation, and determinism. Readiness profiles, semantic input evidence, warning parser/allowlists, and ten-run acceptance remain in progress. | Each game reaches its expected menu and semantically responds to all three keys; ten fresh repetitions pass; historical tofu is explicitly diagnosed; false timeout reproduced and avoided by readiness markers; new warning/crash/stall each fails usefully |
| M2 — harness CI gate | Scenario adapter and capability matrix; separate firmware goldens, masks, comparator; pinned Ubuntu smoke job | Implemented: sequential two-repeat smoke runs for all three games passed locally on 2026-10-23; exact repeat hashes and warning allowlists passed; deliberate screenshot and warning regressions fail offline tests. Ubuntu 24.04 workflow has PR, main push, manual, and reusable-call triggers, a 10-minute timeout, and 14-day diagnostics. First hosted run and host CTest verification remain pending. xiaoxiaole navigation passes while its tofu text-quality check remains a visible non-blocking failure. |
| M3 — inspection and readable text | DWARF descriptors for LVGL 9.5/9.6; host CJK subset; xiaoxiaole upstream font fix/artifact or explicit unresolved text gate | Same known label/visibility cases inspect correctly on both LVGL versions; stripped/mismatched ELF fails or declares allowed fallback; reviewed host mixed-text goldens; xiaoxiaole digit gate passes only after genuine font correction |
| M4 — XiaoZhi spike | Three-day compatibility/network experiment and source-built bundle (§9.2) | Written route decision with pass/fail evidence; supported protocols and all bypasses explicit; no production-6.1 claim from a compatibility build or shim |
| M5 — conversation regression gate | Supported peer transports, shared fixture bindings, lifecycle/failure goldens and traces | PTT, retained/empty STT, mixed text, interruption, timeout/disconnect/recovery pass on the declared firmware variant; each claimed protocol has its own evidence; ten-run determinism and one deliberate protocol regression fail-red check |

M0–M2 implement the useful firmware integration extension of roadmap P1-04. M4–M5 bring the roadmap P2 mock-server idea into firmware testing with explicit compatibility gates. They do not mark unrelated P1 battery/settings/power mocks complete. M3 can proceed alongside the conversation spike. Enable the three-game navigation gate with a visible font issue if needed, but keep the text-correctness acceptance outstanding; never report the whole milestone complete while a required criterion remains open.

M2 implementation and local firmware execution are recorded above; hosted CI execution is still pending. Each phase retains firmware build/size validation in its owning repository and the physical-device checklist for actual display, controls, sound, network, battery, and power behavior.

## 12. Risks and mitigations

| Risk | Mitigation / stopping condition |
|---|---|
| Fork maintenance or upstream rejection | Small separable delta, full commit pins, reproducible corpus; continue pinned fork only with an identified maintainer |
| Missing historical artifacts or expired CI retention | Rebuild with explicit new artifact identity; durable promoted bundles; fail resolution rather than fetching latest |
| Emulator bug mistaken for firmware regression | Retain prior emulator/firmware bundles; compare one variable at a time and reproduce on device for release decisions |
| DWARF optimized away or LVGL internals change | Matching debug ELF, bounded descriptor reader, explicit capability failure and permitted pixel/UART fallback |
| Socket scheduling or guest entropy changes screenshots | Emulator-time barriers, fixed state/seeds, ten-run checks; do not make uncontrolled runs a required golden gate |
| Allowlist or masks hide defects | Exact bounded warnings, owners/expiry, area reporting, mandatory semantic assertions, fail-red checks |
| IDF 6.1 networking is larger than expected | Three-day spike, explicit compatibility/shim labels, separate decision before extended emulator work |
| Correct strings but unreadable fonts | Glyph preflight plus image review; firmware font fixes published at new identity; no text masks |
| Artifact licenses restrict redistribution | Inventory source/assets/fonts and choose private durable storage where needed before promotion |
| CI capacity or diagnostic growth exceeds estimates | Measured smoke budget, two-process cap, bounded traces/storage, nightly extended matrix; timeout/OOM cannot pass |

## 13. Decisions (approved 2026-10-22)

The user accepted every recommendation in the previous open-decision list:

1. One named maintainer owns the passportsim fork and upstream PRs. Changes are upstream-first, with a pinned fork used for delivery. Until a fork repository exists, delivery is pinned to the upstream commit plus a tracked patch in this repository.
2. Firmware comes from pinned game-repository CI artifacts. Restricted firmware and fonts stay in an approved private store and are never committed here.
3. The XiaoZhi ESP-IDF 5.5.3 compatibility build is accepted as explicitly labeled interim evidence. Production ESP-IDF 6.1 coverage remains open.
4. WebSocket is first. MQTT+UDP remains unsupported until its own acceptance checks pass.
5. xiaoxiaole navigation checks are required. Text quality is a separately visible failing check with an owner; tofu is never approved as a golden.
6. PR budgets and 14-day diagnostic retention are adopted provisionally.
