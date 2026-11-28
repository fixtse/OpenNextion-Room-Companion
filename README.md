# ESPHome Room Companion by fixtSE

**The Home Assistant controls you reach for every day, right where you use them.**

Turn a rectangular OpenNextion touchscreen into a dedicated control panel for a
room. Dim the lights from your bedside, start a playlist in the kitchen, talk to
Home Assistant Assist, or see who’s at the door without reaching for your phone.
Choose the controls you want on screen and arrange them around your routine.

Made by [fixtSE](https://fixtse.com/).

[Key features](#key-features) · [Get started](#get-started) · [User guide](docs/web-configuration.md) · [Firmware releases](https://github.com/fixtse/esp-ota/releases) · [All guides](#find-your-guide) · [Home Assistant integration](https://github.com/fixtse/ha-room-companion)

## Key features

### Home Assistant controls

Control and monitor more than 20 Home Assistant entity types directly from main
cards or full-screen detail views:

| Category | What you can do |
| --- | --- |
| **Lights** | Toggle on/off, drag a color-tinted brightness slider, adjust color temperature, or pick colors on a color wheel (modes follow what Home Assistant reports). |
| **Climate & fans** | Drag the target-temperature arc, step with − / +, switch HVAC modes, presets, fan, swing, and humidity; control fan power, speed bars, oscillation, direction, and presets. |
| **Covers & blinds** | Drag a live window/blind visual to set position, tap Open / Stop / Close, and configure up to **4 position presets** per cover (15%, 25%, 50%, 75% by default). |
| **Switches, helpers & automations** | Toggle switches, input booleans, and automations, or run scenes, scripts (with Stop while running), and buttons. Pair switches with a power/energy sensor (W or kWh) for live readings and a 12-hour graph. |
| **Numbers, selects & sirens** | Adjust numbers and input numbers with sliders or − / + steps, pick from scrollable select lists, and configure siren tone, volume, and duration. |
| **Media players** | View album artwork, title, artist, and a live playback progress bar; use play/pause/skip transport and swipe volume; launch up to **8 saved playlists or radio stations** per player. |
| **Cameras** | View live **MJPEG streams** on the full screen (via Home Assistant’s `/api/camera_proxy_stream/`) or full-frame-rate video from the **WebRTC Camera** integration, **Frigate** or **go2rtc** through the Remote WebView server—tap it for **full screen** (turned to fill a portrait screen), with the **camera’s sound** on the panel speaker and automatic recovery when a camera comes back on ([live cameras](docs/room-companion/dashboard-stream.md#live-cameras))—and auto-refreshing snapshots on main cards, a **Camera is off** notice for switched-off cameras, toggle power or motion detection where supported, and optionally link an **ONVIF PTZ** camera entity for pan, tilt, and zoom icon controls. |
| **Vacuums** | Monitor cleaning status and battery ring, trigger play/pause, stop, dock, locate, and spot clean with icon buttons, and cycle fan speed presets. |
| **Locks & alarms** | Lock, unlock, or unlatch doors (`lock.*`), and arm or disarm `alarm_control_panel.*` entities with an on-screen **PIN keypad** when a code is required. |
| **Weather & sensors** | Check current conditions, feels-like, humidity, wind, pressure, visibility, UV, and a 5-day forecast. View sensor values, percentage bars, secondary readings with custom templates (such as relative reset times), **1 h / 12 h / 24 h / 7 d history graphs** (min / avg / max), and persisted recent history for binary sensors and events. |

Main cards include direct touch controls (switches, brightness sliders, climate
− / +, cover buttons, vacuum and lock controls, media transport, and Run buttons) so common actions take
one tap; tap a card outside its controls to open the full screen.

### Pages, layouts & appearance

- **Up to 250 configured entities across multiple pages.** Browse pages by
  swiping left/right or jumping straight there from the bottom navigation dock.
- **Flexible page layouts.** Choose **Full** (1 entity), **Split** (2 entities),
  **Large + 3** (1 main control + 3 compact sensors), **Stack** layouts (presets
  like *Card + chips*, *Sensor board*, and *List*, or custom stacks of up to 6
  rows mixing full cards, split pairs, compact rows, and 3-item chip strips), or
  an **Action grid** (adjustable 1×1 to 4×4 grid of one-tap keys — tap to run,
  hold to open full controls), with up to 16 entities per page.
- **Custom icons, animations & key artwork.** Sync Material Design Icons from
  Home Assistant (downloaded on demand with built-in offline fallbacks), pick
  from the icon browser, or let a page’s dock icon follow a placed entity’s live
  state icon. On action-grid keys, enable icon-only mode, active-state icon
  animations (**pulse**, **blink**, **spin**), or upload custom cropped artwork
  with separate on/off images and **animated GIF/WebP tap feedback**.
- **Pinned dock item.** Pin a favorite destination—such as the streamed page or
  Assist—to the left or right edge of the bottom dock.
- **Native hardware orientation & theming.** Choose **Portrait**, **Landscape**,
  **Portrait · upside down**, or **Landscape · flipped** (applied in hardware by
  the display controller with no software rotation overhead). Customize the
  **panel background** (presets or any hex color with automatic light/dark
  contrast) and **accent color** across controls, graphs, timers, and
  notifications.
- **Rich text & emoji.** Supports accented Latin characters and on-demand **Noto
  Color Emoji** (with monochrome fallbacks offline).

### Synced lyrics

Turn the media screen into a live lyrics display over a soft, blurred copy of
the album art—showing the previous, current, and next lines in time with the
music. Works with Spotify and any other Home Assistant media player that reports
title, artist, duration, and position:

- **Synced locally on the panel.** The Room Companion integration fetches
  timed lyrics from [LRCLIB](https://lrclib.net) and sends them to the panel.
  The panel tracks the active line locally.
- **Instant toggle & custom sources.** Tap the lyrics at any time to switch back
  to the album art and quick actions (lyrics return on the next track), or send
  LRC text from your own automation with the panel’s `set_lyrics` action.
- [Synced lyrics setup guide](docs/lyrics.md)

### Streamed page (Home Assistant dashboard)

Stream an interactive Home Assistant dashboard—or any web page—directly to the
touchscreen using
[RemoteWebViewServer](https://github.com/strange-v/RemoteWebViewServer):

- **Interactive tile streaming.** Headless Chromium renders your dashboard and
  streams changed tiles to the panel while forwarding touches back, with
  configurable zoom and per-model defaults.
- **Native handoff & flexible access.** Open the streamed page from a pinned
  shortcut on the left or right edge of the bottom dock, from Home Assistant’s
  **Streamed page** switch, or automatically as your idle screen. Native
  overlays for Assist, timers, and notifications smoothly take over and return
  to the stream when finished.
- **Live cameras in the native camera screen.** The same server plays cameras
  at full frame rate inside the panel's own camera screen—no dashboard to build.
  Pick the **WebRTC Camera** integration (each camera plays its own stream,
  nothing else to set up) or a **go2rtc / Frigate** server. Tap the picture for
  full screen—on a portrait panel only the video turns, the panel keeps its
  orientation—and tap the speaker button to hear the camera on the panel (the
  settings page finds the panel’s speaker in Home Assistant for you). The
  refresh button reloads the stream, and the panel reloads it by itself when a
  camera is turned back on or the picture freezes. [Live cameras](docs/room-companion/dashboard-stream.md#live-cameras)
- **Easy deployment.** Run RemoteWebViewServer as a Home Assistant OS add-on or
  with the included Docker Compose setup. [Dashboard streaming setup](docs/room-companion/dashboard-stream.md)

[![Add the Remote WebView Server repository to Home Assistant](https://my.home-assistant.io/badges/supervisor_add_addon_repository.svg)](https://my.home-assistant.io/redirect/supervisor_add_addon_repository/?repository_url=https%3A%2F%2Fgithub.com%2Fstrange-v%2FRemoteWebViewServer)

### ImmichFrame photo frame

Turn the panel into a dedicated photo frame backed by your Immich library using
the [fixtSE ImmichFrame fork](https://github.com/fixtse/ImmichFrame):

- **Native-resolution photos & mDNS discovery.** Automatically discovers
  ImmichFrame on your network and fetches screen-sized photos matching the
  panel’s current orientation.
- **Flexible overlays & framing.** Choose **Large clock**, **Small clock**, or
  **Photo only** overlays; pick **Blurred background**, **Fill screen**, or
  **Whole photo** fit modes; enable split-view **photo pairs** when two photos
  suit the screen better than one, fade transitions, and a countdown bar.
- **Metadata & swipe history.** Optionally display up to three photo details
  (date, place, album, people, description) and swipe left/right to browse
  forward or back through the last 20 screens. [ImmichFrame setup](docs/room-companion/panel-ui.md#immichframe)

### Timers, notifications & quick-access shade

- **Multi-source timers without clutter.** Track Home Assistant `timer.*`
  helpers, local Assist voice timers, and compatible Voice PE / SoundSense /
  Google Home timer sensors—even without giving them a page. Active timers show
  in a top-bar countdown chip and open a multi-timer list or countdown ring with
  **Pause/Resume**, **+1 min**, and **Stop**.
- **Rich notification center.** Send reminders, calendar alerts, or doorbell
  snapshots from Home Assistant automations with `show_message` (custom timeout
  or persistent, optional thumbnail image, HTML formatting, color emoji, and
  automatic in-place updates for repeated threads) and `dismiss_message`. Pull
  down from the top edge on any screen—or tap the bell—to browse up to 12 recent
  notifications.
- **Phone & desktop notifications via KDE Connect.** Mirror notifications (with
  app icons and automatic dismissal when cleared on your phone or PC) using
  [KDE for Home Assistant](https://github.com/Radeaus/kdeforhomeassistant) and
  the [Room Companion integration](https://github.com/fixtse/ha-room-companion)
  ([setup guide](docs/display-notifications.md#phone-and-desktop-notifications-kde-connect)).
  On **Linux** and **Windows**, pair it with the
  [KDE Connect Rich Notifications](https://github.com/fixtse/kdeconnect-rich-notifications)
  plugin to send desktop notifications with chat avatars, website icons, and
  screenshot previews (Linux) or app logos (experimental Windows port).

- **Pull-down quick toggles & status chips.** The notification shade includes
  quick tiles to toggle the standby clock timer, **switch presets on the
  panel**, and rotate screen orientation. Live status chips in the top bar let
  you jump straight to active timers or playing music.

### Voice Assist, wake word & ambient sound

- **Home Assistant Assist satellite.** With a compatible PDM microphone and I²S
  speaker, tap **Talk**, press the board’s **BOOT** button, trigger the Home
  Assistant **Talk** entity, or use a wake word to speak a request, view live
  transcript and reply bubbles, and hear the response. Includes an optional
  setting to **pause (or mute) playing media players** while Assist listens, a
  software **Microphone muted** switch, and an exposed **Speaker** media player
  entity.
- **Local wake word & voice timer controls.** The wake-word firmware variants
  support on-device **Okay Nabu** (default), **Hey Jarvis**, **Hey Mycroft**,
  and **Kenobi** with adjustable sensitivity, plus contextual **Stop** and
  **Snooze** voice commands for ringing timers and announcements.
- **Dual-microphone auto selection.** Supports the Nextion Dual MIC board,
  automatically picking the microphone with the better signal-to-noise ratio and
  falling back seamlessly if one goes silent.
- **Ambient sound sensing & dynamic volume.** Built-in SoundSense-compatible
  **Ambient Sound Level** (dB) and **Sound Detection** (silence, quieting,
  active, noise, presence) sensors run locally without recording or uploading
  audio, with optional **Dynamic Volume** to adapt idle speaker volume to the
  room.

### Clock idle screen & power management

- **Adaptive clock idle screen.** Choose presets (*Time only*, *Time & weather*,
  *Weather & forecast*, *Everything*) or toggle individual parts with adaptive
  font sizing, weather or custom sensor readings, 12/24-hour and date formats,
  optional edge seconds indicator, notification badge, and an option to show the
  active **media player** while music plays. (Switch the idle screen to
  **ImmichFrame** or **Streamed page** at any time from the **Idle screen** tab
  or Home Assistant.)
- **Scheduled sleep & connected low-power sleep.** Configure a daily sleep
  schedule to switch off the display overnight while staying connected to Home
  Assistant, or automate the **Low Power Sleep** switch to sleep the display
  controller, pause LVGL rendering, and enable Wi-Fi modem power saving until
  tapped or woken from Home Assistant.

### Custom actions, presets & Home Assistant automation

- **Custom gestures & quick actions.** Bind scripts or automations to per-entity
  **Press actions**, **Swipe up / down**, and **Hold + swipe up / down**, with
  configurable step sizes, optional feedback entities, brief value popups,
  direct script response reading, or `esphome.panel_gesture` events. Add up to
  **8 quick actions** (playlists, radio stations, or scenes) to a control.
- **Named presets.** Save up to **8 presets** capturing your pages, controls,
  icons, layouts, playlists, and actions, and switch between routines without
  restarting—from the pull-down shade, the web UI, or Home Assistant’s
  **Preset** select and `apply_preset` / `save_preset` / `delete_preset`
  actions.
- **Home Assistant automation hooks.** Control the panel from automations using
  exposed entities for **Page** and **Show entity** selection, **Screen Light** (brightness),
  **Screen Off**, **Low Power Sleep**, **Idle screen**, **Streamed page**,
  **Preset**, **Talk**, **Microphone muted**, **Speaker**, and **Firmware
  Update**.
- **Assist control.** Expose the **Page**, **Show entity** and **Preset**
  selects and Assist can show a page, open any configured entity's detail
  screen or switch presets; the Assist overlay closes once the change is done.
  See [Assist: show pages and entities](docs/room-companion/assist-show-entity.md).

### Browser setup, backups & maintenance

- **Built-in web configurator (“Visit”).** Open **Visit** on the Home Assistant
  device page to arrange pages by drag-and-drop with a live preview (matching
  your panel’s size, orientation, and colors), undo (`Ctrl+Z`), and unsaved
  draft recovery. Automatic mDNS discovery finds Home Assistant and ImmichFrame
  on your network; browser-side OAuth sign-in searches entities by name, room,
  or type without storing a long-lived token on the panel.
- **Backups, diagnostics & updates.** Export your configuration and presets to
  JSON (excluding credentials) and selectively import or apply backups. Download
  diagnostics with uptime, memory, Wi-Fi stats, icon/artwork status, and
  retained crash/panic breadcrumbs. Install wireless updates through Home
  Assistant’s **Firmware Update** entity, provision Wi-Fi via **Improv Serial**
  (USB), **Improv BLE** (authorized with BOOT), or the setup hotspot, and reset
  saved Wi-Fi and settings with a 10-second touch-and-hold.

## Choose your panel

Both models run the same Room Companion app. Pick the firmware that matches
the model number on your panel.

| OpenNextion model | Touchscreen | ESPHome firmware |
| --- | --- | --- |
| **ONX2432G028** | 2.8 inches · 240 × 320 | [ONX2432G028.yaml](ONX2432G028.yaml) · [Wake-word variant](ONX2432G028.wake-word.yaml) · [Lite (no mic/speaker)](ONX2432G028.lite.yaml) |
| **ONX3248G035** | 3.5 inches · 320 × 480 | [ONX3248G035.yaml](ONX3248G035.yaml) · [Wake-word variant](ONX3248G035.wake-word.yaml) · [Lite (no mic/speaker)](ONX3248G035.lite.yaml) |

## 3D-printable desktop enclosures

Print a portrait or landscape desktop case for either OpenNextion panel.
Each enclosure has a narrow rim, an integrated base, printed BOOT and RESET
buttons, and a printed power slider. Choose a plain case or an audio version with a top-facing mic and
rear speaker mount. [STLs, CAD previews, and assembly guide](hardware/enclosures/README.md)

[![3.5-inch portrait audio enclosure, front and rear](hardware/enclosures/renders/onx3248g035-portrait-audio.png)](hardware/enclosures/README.md)

## Get started

1. [Install the firmware and connect to Wi-Fi](docs/setup.md). For a first
   install, use the factory image for your model, built with **ESPHome 2026.11.1**.
   Wi-Fi setup is available over USB or Bluetooth with Improv, or through the
   panel’s setup hotspot.
2. Add the panel through Home Assistant’s **ESPHome** integration and enable
   **Allow the device to perform Home Assistant actions**.
3. Open **Visit** on the device page, or use the address shown on the panel.
   Sign in to browse your Home Assistant entities, add your first page and
   controls, and select **Save changes**.

For synced lyrics, notification forwarding, and images served over your local
network, [set up the Room Companion integration](docs/setup.md#set-up-the-room-companion-integration).

Already running Room Companion? Check the panel’s **Firmware Update** entity
in Home Assistant for available updates. [Setup and update guide](docs/setup.md#keep-it-updated)

## Find your guide

| I want to… | Guide |
| --- | --- |
| Install the firmware, connect Wi-Fi, or update my panel | [Setup and connection help](docs/setup.md) |
| Add controls, set up presets, or change the display | [Web configuration](docs/web-configuration.md) |
| Learn the touch controls or set up shortcuts | [Using the panel](docs/controls.md) · [Custom gestures](docs/gesture-controls.md) · [Panel screens](docs/room-companion/panel-ui.md) |
| Show reminders, photos, or calendar events | [Display notifications](docs/display-notifications.md) |
| Customize icons, album art, emoji, or synced lyrics | [Icons and media](docs/icons-and-media.md) · [Synced lyrics](docs/lyrics.md) · [Fonts & emoji](docs/display-fonts.md) |
| Print a desktop case | [Enclosure downloads and assembly](hardware/enclosures/README.md) |
| Connect the microphone and speaker or set up wake word | [Hardware and Assist](docs/room-companion/hardware-and-assist.md#commissioning) |
| Show a Home Assistant dashboard | [Dashboard streaming](docs/room-companion/dashboard-stream.md) |
| Watch and listen to cameras live (WebRTC Camera, Frigate, go2rtc) | [Live cameras](docs/room-companion/dashboard-stream.md#live-cameras) |
| Extend the firmware or see what has been verified on hardware | [Architecture](docs/room-companion/architecture.md) · [Extensions](docs/room-companion/extensions.md) · [Validation record](docs/room-companion/validation.md) |

## Development

See [AGENTS.md](AGENTS.md) for the devserv workflow. In the pinned ESPHome
development container, run `bash scripts/validate-room-companion.sh` to
validate both models, their wake-word variants, and the extension example. Add
`--compile` to build the same matrix.

## License

Room Companion is licensed under **AGPL-3.0-only** and preserves the Git history
of the earlier fixtSE OpenNextion ESPHome project. See [LICENSE](LICENSE) and
[COPYRIGHT](COPYRIGHT).

OpenNextion’s original [MIT notice](LICENSES/OpenNextion-MIT.txt) is preserved.
Third-party code, fonts, and assets retain their own terms. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
