# ESPond32 Hardware

KiCad projects and supporting documents for the ESPond32 control board. The board carries
the ESP32-S3-WROOM-1 module, per-channel 3-position switches, the float-sensor input, a
WS2812 status LED chain, and the output-driver stage for four channels (pump1, pump2,
valve1, light1).

Designed by hand with only minimal AI assistance.

## Two board variants

There are two KiCad projects under `kicad/`, differing only in how the mains load is
switched:

| Project | Output stage | Mains on board? | Status |
|---|---|---|---|
| `kicad/espond32-offboard-ssr/` | 3.3V/5V logic-level control signal to an **external** SSR / relay module, driven through a MOSFET buffer | No - board is entirely SELV/low-voltage | **Finalized, in manufacturing** |
| `kicad/espond32-onboard-ssr/` | On-board optotriac + power triac SSR stage, mains terminals on the PCB | Yes | Design variant / reference |

The off-board variant is the primary build. It keeps all mains wiring off the PCB (each
channel just sinks an external module's control line to ground), which greatly simplifies
safety, layout, and enclosure work. The on-board variant is kept for anyone who wants a
single-board solution and is comfortable with mains-on-PCB design; it carries fuse, MOV,
isolated AC-DC, and per-channel triac + snubber parts.

Both variants match the firmware exactly: four output channels (not three), the WS2812 LED
chain on GPIO10, and the switch/float pin map from the firmware's `config.h`.

## Folder layout

```
hardware/
  kicad/
    espond32-offboard-ssr/   # primary board - schematic, PCB, exported BOM/CPL, JLCPCB output
    espond32-onboard-ssr/    # on-board-SSR variant
  bom/                       # STALE - see below
  datasheets/                # ESP32-S3-WROOM-1 datasheet
```

## Bill of materials

**The `bom/` folder at the top level is stale. Do not order from it.** The authoritative BOM
for each board is the one exported from its KiCad project (the `.csv` files inside each
`kicad/espond32-*/` directory, and the `jlcpcb/` outputs for the off-board variant). Treat
the KiCad project as the single source of truth for parts, footprints, and designators; the
top-level `bom/` CSVs predate the current schematics and are kept only for reference.

Key part choices worth knowing before a re-spin (from the current design):

- **Module:** ESP32-S3-WROOM-1-**N8** specifically (quad flash, no PSRAM). The N8R8/N16R8
  PSRAM variants use GPIO33-37, which collide with the firmware's debug test points.
- **Off-board output stage:** logic-level N-channel MOSFET (e.g. 2N7000) per channel buffers
  the GPIO up to a full logic swing before it reaches the external SSR/relay module, so the
  design works with the widest range of off-the-shelf 3-32 VDC modules. The external SSR
  modules themselves are not part of the board BOM.
- **Switches:** one 3-position center-off (ON/AUTO/OFF) toggle per channel; common to 3V3,
  the two throws to the SW-A/SW-B GPIOs. No external pulls (firmware configures pull-downs).
- **Float input:** TVS + series resistor + RC filter, sized for a long outdoor sensor run.
- **Indicators:** a single 6-pixel WS2812B chain (pump1, pump2, valve1, light1, float, sys),
  not discrete LEDs.

## KiCad libraries

The ESP32-S3-WROOM-1 symbol/footprint come from Espressif's official KiCad library
(install via KiCad's Plugin & Content Manager). Standard passives use the built-in KiCad
libraries. A few parts (3-position switch, some connectors) use custom footprints built from
their datasheets - see the project's footprint library.

## Manufacturing (off-board variant)

The `kicad/espond32-offboard-ssr/jlcpcb/` directory holds the Gerber/BOM/CPL output prepared
for JLCPCB assembly. Regenerate it from KiCad if you change the board; do not hand-edit the
exported files.

## Safety

The off-board board is low-voltage only and safe to handle and power from USB-C or a 5V
supply. Mains switching happens entirely in the external SSR modules and their wiring
harness - fusing, strain relief, and enclosure/creepage are the integrator's responsibility.

The on-board-SSR variant puts mains on the PCB and is only for people competent with
mains-voltage design: verify fuse rating, MOV placement, isolation/creepage clearances, and
snubber sizing against your actual loads before powering it.

## Provenance and disclosure

This hardware was designed by hand with minimal AI assistance. Note that `ai_design_doc.pdf`
(repo root) is an early planning document - it specifies three output channels and discrete
indicator LEDs, whereas the shipping design has four channels and a WS2812 chain to match the
firmware. Where the design doc and the KiCad projects disagree, the KiCad projects are
correct.
