# Fuzzy Slang Clock

A desk clock that tells you the time and then says something rude about your
day. Six-colour e-paper, five-minute frame, absolutely no way to make it stop
judging you.

Hardware: M5Stack M5PaperColor (C151), ESP32-S3, 400×600 Spectra 6 panel,
RX8130 RTC, M5PM1 power manager.

The watch works out where you are in the day — before work, at work, end of
shift, Friday, weekend, holiday — and picks a line to match. Some of them are
supportive. Most of them are not.

## Before you flash

You need:

* an M5PaperColor and a USB-C **data** cable. The charge-only kind will waste
  your evening and then blame you for it.
* Arduino IDE 2.x, or `arduino-cli` if the terminal is where you feel alive
* the **M5Stack** board package, version **3.3.7 or newer**
* one library: **M5Unified**

The board is called **M5PaperColor** in the board list, not ColorPaper. It
showed up in the M5Stack package at version 3.3.7; older packages only list
M5Paper and M5PaperS3, and those are different hardware.

Built with M5Unified 0.2.23 on esp32 core 3.3.11. The M5Stack 3.3.x package is
the same core with M5 board definitions on top. Newer versions will probably
work. Older ones get no promises from me.

The line generator, fonts and drawing code live in this repo, nothing else to
clone.

## Flash it

Arduino IDE:

1. Preferences → **Additional boards manager URLs** → add
   `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`
2. Boards Manager → install **M5Stack** (3.3.7 or newer).
3. Library Manager → install **M5Unified**.
4. Board: **M5PaperColor**. Then set, exactly:

   * PSRAM: **OPI PSRAM**
   * Flash size: **16MB**
   * Partition scheme: **16M Flash (3MB APP/9.9MB FATFS)**
   * USB CDC On Boot: **Enabled**

   The default partition gives the app 1.2 MB and this firmware is about
   1.3 MB, so a wrong partition is a failed upload and a wasted evening. Get
   the PSRAM wrong and you get a blank panel. That is the firmware politely
   refusing to draw.

5. Open `slang_clock_color.ino`, pick the port that looks like `usbmodem…`,
   hit **Upload**. That is it.

Terminal people:

```sh
arduino-cli core install m5stack:esp32 \
  --additional-urls https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
arduino-cli lib install M5Unified
arduino-cli compile -b m5stack:esp32:m5stack_papercolor:PSRAM=opi,PartitionScheme=app3M_fat9M_16MB .
arduino-cli upload -b m5stack:esp32:m5stack_papercolor:PSRAM=opi,PartitionScheme=app3M_fat9M_16MB -p /dev/cu.usbmodemXXXX .
```

No M5PaperColor in the list? Update the M5Stack package; anything below 3.3.7
does not have it. The plain **ESP32S3 Dev Module** from the esp32 package also
works with OPI PSRAM, 16 MB flash, 3MB app / 9.9MB FATFS and CDC On Boot
enabled, it is just more clicking for the same result.

No port in the list? It is the cable. Upload dies halfway? Unplug, plug back
in, retry. The board is not bricked, it is just being dramatic.

## First boot

The firmware ships with an empty Wi-Fi config. That is deliberate: my home
network does not belong in your repo, and yours does not belong in mine.

So on first run:

1. Hold **C** for about 2 seconds. The screen switches to `SET UP`, shows a QR
   code and raises its own access point, `SLANG-CLOCK`, password `slangclock`.
2. Point your phone camera at the QR, or join the network by hand and open
   `http://192.168.4.1`.
3. The page asks you to confirm you are 18+, then:

   * who the clock talks to: man or woman
   * work days
   * shift start and end
   * holidays as `MM-DD`, comma separated — optional
   * Wi-Fi name and password — needed for NTP
   * time zone, hours from UTC

4. Save. The clock shifts its RTC to the new time zone, redraws, and carries on.

Settings live in NVS on the device. Nothing is uploaded anywhere. The only
network traffic is NTP, and only when the clock needs the time.

Bail out of setup without saving: hold **A** for about 3 seconds. The portal
screen says so too, in case you forget.

## The frame

One line of hero text, one punchline card, a few colours. That is the whole
design and it is not changing because someone on the internet wants themes.

The panel is Spectra 6: six colours, no partial refresh, and a full repaint
takes around 35 seconds. So the firmware does not try to be a fast watch. It
draws one frame per five-minute slot, then the ESP32 goes into deep sleep until
the next one. The image stays on the glass because that is what e-paper does.

On Fridays the card changes colour and the jokes get worse. On weekends and
holidays it stops pretending you have a job.

While a frame is being painted the RGB LED is blue. It goes off when the paint
is done. That is the only progress bar this thing has.

## Buttons

| Button | Action |
| --- | --- |
| **A** or **C**, tap | Redraw the current frame right now |
| **C**, hold ~2 s | Open setup: access point, QR, web form |
| **A**, hold ~3 s in setup | Cancel setup without saving |
| **B**, hold ~1.5 s | Show `POWER OFF`, park the panel, cut power |
| **A**, hold outside setup | Nothing. It is a button, not a toy |
| **PWR**, tap when off | Power on |
| **PWR**, hold ~1.5 s | Cut power instantly, no farewell screen |

Any button wakes the board from deep sleep, and the frame gets redrawn. That is
fine. Standing there pressing buttons is also fine, just not productive.

Give it up to a minute on the first frame: Wi-Fi, NTP, then 35 seconds of the
panel chewing on the image. After that it wakes itself every five minutes.

## Power

Everything runs off the board's Li-Po. Between frames the ESP32 is in deep
sleep, the panel is parked, the LED is off. Battery life depends on how often
you poke it and how bright your idea of a desk is.

Hold **B** and the screen switches to `POWER OFF` with a reminder to press
**PWR** to bring it back. Then the panel is parked and the power is cut.

Do not expect a percentage on screen. This is a clock, not a battery gauge.

## The case

Three STL files in `enclosure/`:

* `front1.stl` — the front frame, 87.8 × 128.4 × 1.5 mm, sized around the
  panel
* `base.stl` — the body that holds the board
* `button.stl` — the button cap

Print them, drop the board into the base, fit the front frame, and press the
cap on. It beats leaving a bare PCB on your desk and pretending that is a
design choice.

## Version one, and honest about it

This is v1. It draws a clock, it generates lines, it shuts down when asked. No
themes, no menus beyond the setup page, no cloud account, no app. It does one
thing and it does it on a panel that takes half a minute to agree.

Version one means version one: there are rough edges. The phrase generator can
repeat itself. The NTP fallback chain is three public servers and a prayer. The
setup portal is a web form from 2003 wearing a dark theme. If the board behaves
strangely after a bad upload, reflash it before writing an angry issue.

Issues and sane pull requests are welcome. Insults about my colour choices will
be taken as feedback.

## Legal, or the part everyone skips

* **18+.** The clock contains sarcasm, rude jokes and mild insults. It is not
  for children. The setup page makes you tick a box saying you are old enough,
  and that box is not decorative.
* **No affiliation.** This is an independent project. It is not made, endorsed,
  sponsored or supported by M5Stack. M5Stack and M5PaperColor are trademarks of
  their owners.
* **Your data stays on the device.** Wi-Fi credentials and schedule settings
  are stored in NVS on the board. The firmware has no telemetry, no analytics,
  no update check, no account. The only outbound traffic is NTP.
* **The access point is not a secret.** Setup mode broadcasts `SLANG-CLOCK`
  with the published password `slangclock`. Anyone in radio range can join it
  while setup is open. Do not configure the clock in a crowded café and then
  complain to me about it.
* **As-is.** This firmware is provided under the MIT license, without warranty
  of any kind. It controls a lithium battery and a power management IC. If you
  modify the power path, the PMIC configuration or the shutdown code, you are on
  your own. See `LICENSE`.
* **Fonts.** Unbounded Black and JetBrains Mono are used under the SIL Open Font
  License 1.1. Full texts in `licenses/`.

## Watch it move

An e-paper repaint refuses to photograph well, so there is a video of a whole
frame grinding out: [@aamuilmacraft on Instagram](https://instagram.com/aamuilmacraft).
