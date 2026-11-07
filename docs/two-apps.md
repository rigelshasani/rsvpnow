# Two apps on one board

The 16 MB flash layout (`default_16MB.csv`) has two app slots of 6.25 MB each: `app0` at
`0x10000` and `app1` at `0x650000`. rsvpnow lives in `app0`; a second firmware, such as a game,
can live in `app1`. Settings and saves share the NVS partition, each app in its own namespace.

**Switching:** hold **BOOT + PWR together for 3 seconds**. The reader saves your place and
settings, then boots the other slot (`src/update/AppSwitch.cpp`). The other firmware needs the
same check to switch back; Alisa's Cake Dash has it in `src/board/AppSwitch.cpp`. While both
buttons are down their usual actions (play, menu, standby, power off) are suppressed. With
nothing in the other slot the reader says "No second app" and stays put.

**Flashing:**
- The reader: `pio run -e waveshare_esp32s3_touch_lcd_349_rev2 -t upload`. This writes `app0` and
  resets the boot choice to it; the other slot is left alone.
- The second app: write it to `0x650000` only, never with its own `pio run -t upload` (that writes
  `app0` and replaces the reader). Cake Dash has `tools/flash_slot2.py` for this.

**Updates:** an over-the-air update writes the inactive slot, so installing one would replace the
second app. rsvpnow only takes updates from its own repository, which publishes none, so the
reader won't offer any.
