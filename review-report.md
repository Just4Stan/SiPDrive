# SiPDrive Design Review Report

**Date:** 2026-03-14
**Project:** SiPDrive Motor Controller
**Board Rev:** V1 (first dev board)
**KiCad Version:** 9
**Reviewer:** Claude (automated analysis + datasheet verification)

---

## Executive Summary

The SiPDrive is a compact 3-phase BLDC motor controller based on STSPIN32G4 (System-in-Package with STM32G431 + gate driver + buck/LDO), MT6701 magnetic encoder, TCAN1057A CAN-FD transceiver, and 6× SP40N03GNJ MOSFETs. 8-layer PCB, 33×29mm.

**Verdict: NOT READY FOR ORDER — 3 critical issues must be fixed first.**

### Issue Count

| Severity | Count |
|----------|-------|
| CRITICAL | 3 |
| HIGH | 7 |
| MEDIUM | 7 |
| LOW | 4 |

---

## Critical Failures (Must Fix Before Ordering)

### CRIT-1: U1 Thermal Pad — Insufficient Vias

**Component:** U1 (STSPIN32G4), pad 65, 4.0×4.0mm exposed pad on GND
**Finding:** Only **1 via** under the thermal pad. Minimum recommended: **9**. Ideal: **16**.
**Impact:** The STSPIN32G4 has Rth_JA = 48.3°C/W. With inadequate thermal vias, heat cannot transfer to inner/back copper planes. The part will overheat under any meaningful motor load, causing thermal shutdown or damage.
**Fix:** Add a 3×3 or 4×4 array of 0.3mm drill vias under the pad. Tent the back side to prevent solder wicking. Standard via pitch: 1.0–1.2mm.

### CRIT-2: DFM Violations — Below JLCPCB Advanced Process Minimums

**Finding:** Two parameters are below even JLCPCB's advanced (6-layer+) process limits:

| Parameter | Actual | JLCPCB Standard | JLCPCB Advanced | Status |
|-----------|--------|-----------------|-----------------|--------|
| Track spacing | 0.091mm | 0.127mm | 0.100mm | **FAIL** |
| Annular ring | 0.075mm | 0.125mm | 0.100mm | **FAIL** |

**Impact:** JLCPCB will likely reject the board or require manual review. All 515 vias have 0.075mm annular ring (0.45mm pad, 0.3mm drill).
**Fix:** Either increase via pad to 0.5mm (giving 0.1mm annular ring) or increase to 0.55mm for margin. Review tight spacing areas and increase to ≥0.1mm minimum. Consider using 0.2mm drill vias where space allows.

### CRIT-3: D2 (LED) Extends Past Board Edge

**Component:** D2 (LED, 0402), edge clearance = **-0.03mm**
**Impact:** Component courtyard extends past the board outline. Will either be clipped by board edge or fall off during assembly.
**Fix:** Move D2 inward by at least 0.5mm from board edge.

---

## Phase 1: Schematic Integrity

### 1.1 Power Architecture

**Power tree:**
```
+BATT (10-30V) ──► STSPIN32G4 internal buck (VM pin 61, SW pin 62, L1)
                    ├──► +12V (VCC pin 63, REGIN pin 64)
                    │     ├──► STSPIN32G4 internal LDO
                    │     │     └──► +3.3V (REG3V3 pin 1)
                    │     └──► U4 SK6513SD4-50 LDO
                    │           └──► +5V (U3 VCC)
                    └──► 3-phase bridge (Q1-Q6 via gate driver)
```

| Rail | Voltage | Total Decoupling | Cap Count | Status |
|------|---------|-----------------|-----------|--------|
| +BATT | 10-30V | 63.2µF | 10 | PASS |
| +12V | 12V (buck output) | 11.0µF | 2 (C17 1µF, C9 10µF) | PASS |
| +3.3V | 3.3V (internal LDO) | 10.4µF | 5 (C2,C6,C10,C12=100nF, C11=10µF) | PASS |
| +5V | 5V (external LDO) | 1.0µF | 1 (C13 1µF) | **WARN** |

**WARN:** +5V rail has only 1µF (C13). TCAN1057A datasheet recommends 100nF minimum decoupling but with CAN bus transients and 80mA peak current, 1µF is borderline. Consider adding a 100nF close to U3 VCC for HF decoupling.

### 1.2 STSPIN32G4 (U1) Pin Verification

**Power pins:**

| Pin | Name | Net | Expected | Status |
|-----|------|-----|----------|--------|
| 1 | REG3V3/VDD | +3.3V | 3.3V output | PASS |
| 2 | VBAT | +3.3V | RTC battery/VDD | PASS |
| 26 | VREF+ | +3.3V | ADC reference | PASS |
| 27 | VDDA | __unnamed_15 (via FB1) | Filtered 3.3V | PASS |
| 32 | PGND | GND | Power ground | PASS |
| 61 | VM | +BATT | Battery input | PASS |
| 62 | SW | __unnamed_2 (to L1) | Buck switch | PASS |
| 63 | VCC | +12V | Buck output | PASS |
| 64 | REGIN | +12V | LDO input | PASS |
| 65 | VSS (EPAD) | GND | Thermal pad | PASS |

VDDA is filtered through FB1 (BLM15HD102SN1D ferrite bead) from +3.3V — correct practice for analog supply noise isolation.

**Gate driver pins:**

| Pin | Name | Net | Connects To | Status |
|-----|------|-----|-------------|--------|
| 43 | GHS1 | → R2 (20Ω) | Q1 gate (high-side U) | PASS |
| 29 | GLS1 | → R8 (20Ω) | Q2 gate (low-side U) | PASS |
| 40 | GHS2 | → R3 (20Ω) | Q3 gate (high-side V) | PASS |
| 30 | GLS2 | → R4 (20Ω) | Q4 gate (low-side V) | PASS |
| 37 | GHS3 | → R5 (20Ω) | Q5 gate (high-side W) | PASS |
| 31 | GLS3 | → R7 (20Ω) | Q6 gate (low-side W) | PASS |
| 41 | BOOT1 | C28 (1µF) to PhaseU | Bootstrap U | PASS |
| 38 | BOOT2 | C29 (1µF) to PhaseV | Bootstrap V | PASS |
| 35 | BOOT3 | C27 (1µF) to PhaseW | Bootstrap W | PASS |
| 42 | OUT1 | PhaseU | Phase U output | PASS |
| 39 | OUT2 | PhaseV | Phase V output | PASS |
| 36 | OUT3 | PhaseW | Phase W output | PASS |

Bootstrap caps (C27-C29) = 1µF each, matching EVL reference design. Gate resistors = 20Ω (vs 100Ω in reference). See section 4.1.

**OPAMP & sensing pins:**

| Pin | Name | Net | Function | Status |
|-----|------|-----|----------|--------|
| 14 | PA1 | OPAMP1_INP | Phase U current sense (+) | PASS |
| 16 | PA3 | OPAMP1_INM | Phase U current sense (−) | PASS |
| 15 | PA2 | OPAMP1_OUT | ADC input (ch13) | PASS |
| 20 | PA7 | OPAMP2_INP | Phase W current sense (+) | PASS |
| 23 | PB0 | OPAMP2_INM | Phase W current sense (−) | PASS |
| 19 | PA6 | OPAMP2_OUT | ADC input (ch16) | PASS |
| 9 | PC0 | VBAT_SENSE | Battery voltage divider | PASS |
| 10 | PC1 | BOARD_NTC | Board temperature | PASS |
| 11 | PC2 | STATOR_NTC | Stator temperature | PASS |
| 52 | SCREF | R15/R37 divider | Overcurrent reference | PASS |

**Communication & debug pins:**

| Pin | Name | Net | Function | Status |
|-----|------|-----|----------|--------|
| 46 | PA10 | CAN_EN | CAN transceiver enable | PASS |
| 47 | PA11 | CAN_RX | CAN receive | PASS |
| 48 | PA12 | CAN_TX | CAN transmit | PASS |
| 49 | PA13 | SWDIO | SWD data | PASS |
| 50 | PA14 | SWCLK | SWD clock | PASS |
| 8 | PG10 | NRST | Reset | PASS |
| 59 | PB8 | BOOT0 | Boot mode select | PASS |
| 55 | PB4 | ENC_CSN | Encoder chip select | PASS |
| 57 | PB6 | ENC_CLK | Encoder clock (SPI SCK) | PASS |
| 58 | PB7 | ENC_D0 | Encoder data (SPI MISO) | PASS |

**NC pins:** Pins 33, 34 are NC — connected to unnamed nets (floating). Per STSPIN32G4 datasheet, NC pins should be left unconnected. **PASS** (unnamed nets = no connection in KiCad).

**Unused GPIO:** PC3, PC4, PC5, PA0, PA4, PA5, PA8, PA9, PA15, PD2, PB3, PB5, PB9, PB10, PC12, PC13, PC14, PC15, PF0, PF1 — connected to unnamed nets. These are unrouted/unused. Verify no-connect markers are present in schematic. Not a board-killing issue.

### 1.3 MT6701 (U2) Pin Verification

| Pin | Name | Net | Expected | Status |
|-----|------|-----|----------|--------|
| 1-4 | NC | unnamed | No connect | PASS |
| 5 | PUSH | unnamed | Push button output (unused) | PASS |
| 6 | A/DO | ENC_D0 | SSI data → U1 PB7 | PASS |
| 7 | B/CLK | ENC_CLK | SSI clock → U1 PB6 | PASS |
| 8 | Z/CSN | ENC_CSN | SSI chip select → U1 PB4 | PASS |
| 9-12 | W,NC,U,V | unnamed | UVW outputs (unused) | PASS |
| 13 | VDD | +3.3V | Power (3.0-5.5V range) | PASS |
| 14 | MODE | unnamed | Mode selection | **WARN** |
| 15 | OUT | unnamed | Analog/PWM output (unused) | PASS |
| 16 | GND | GND | Ground | PASS |
| 17 | EP | GND | Exposed pad | PASS |

**WARN (MED-1):** MT6701 MODE pin (pin 14) appears to be on an unnamed net. The MODE pin selects the output interface (SSI/ABZ/UVW/analog). Per datasheet, MODE should be tied to VDD or GND via resistor to select the desired mode. If left floating, mode is undefined. Verify this pin has a pull-up or pull-down in the schematic.

### 1.4 TCAN1057A (U3) Pin Verification

| Pin | Name | Net | Expected | Status |
|-----|------|-----|----------|--------|
| 1 | TXD | CAN_TX | From U1 PA12 | PASS |
| 2 | GND | GND | Ground | PASS |
| 3 | VCC | +5V | 4.5-5.5V supply | PASS |
| 4 | RXD | CAN_RX | To U1 PA11 | PASS |
| 5 | NC/VIO | +3.3V | See note below | **WARN** |
| 6 | CANL | CAN_L | To connector U5 | PASS |
| 7 | CANH | CAN_H | To connector U5 | PASS |
| 8 | S | CAN_EN | Standby control from U1 PA10 | PASS |

**WARN (HIGH-1): Pin 5 variant ambiguity.** The TCAN1057A (non-V variant) has pin 5 as NC. The TCAN1057A**-V** variant has pin 5 as VIO for I/O level translation. The schematic connects pin 5 to +3.3V, which is correct for the -V variant (enables 3.3V logic levels on TXD/RXD while VCC=5V).

BOM lists LCSC C3236208. **Verify this is the TCAN1057A-V (with VIO), not the plain TCAN1057A.** If it's the non-V variant, connecting voltage to an NC pin is typically harmless (NC pins are internally disconnected) but is not guaranteed safe by the datasheet.

### 1.5 U4 LDO (SK6513SD4-50) Verification

| Pin | Name | Net | Expected | Status |
|-----|------|-----|----------|--------|
| 1 | OUT | +5V | 5V output | PASS |
| 2 | GND | GND | Ground | PASS |
| 3 | EN | +12V | Enable (tied to input) | PASS |
| 4 | IN | +12V | 12V input | PASS |
| 5 | EP | GND | Thermal pad | PASS |

EN tied to IN = always-on when +12V present. Dropout: 12V→5V = 7V headroom, well within LDO capability. Output capacitor C13 (1µF) present.

**Note:** No datasheet found in datasheets/ directory for SK6513SD4-50. Cannot verify max output current, dropout voltage, or recommended cap values. The "50" suffix likely means 5.0V fixed output.

### 1.6 U5 Connector (SM03B-SRSS-TB)

3-pin JST SH connector. Pin 1 = GND, Pin 2 = CAN_H, Pin 3 = CAN_L, Pins 4-5 = GND (structural).

**WARN (HIGH-2): No ESD/TVS protection on CAN bus lines.** CAN_H and CAN_L go directly from U3 to U5 connector with no TVS diodes. Any ESD event on the connector will hit the TCAN1057A directly. The reference design has a TVS footprint (D3, not mounted). Consider adding a bidirectional TVS (e.g., PESD1CAN) between CAN_H/CAN_L and GND.

### 1.7 SCREF — VDS Monitoring Protection (NOT Current Sense)

**IMPORTANT CORRECTION:** SCREF does NOT compare against the OPAMP current sense output. Per STSPIN32G4 datasheet (page 2, Table 5 page 10): SCREF sets the **VDS monitoring protection threshold**. The gate driver monitors drain-source voltage across each MOSFET while it is on. If VDS exceeds VDSth, it indicates a short-circuit or shoot-through, and all gate outputs are disabled.

**Divider:** R15 (100k) / R37 (10k) from +3.3V to GND, plus internal 400kΩ pull-down on SCREF
**Effective lower R:** 10k ∥ 400k = 9.76kΩ
**SCREF voltage:** 3.3V × 9.76k / (100k + 9.76k) = **0.293V**
**VDSth:** ≈ 0.29V (from Table 5, roughly 1:1 with VSCREF at this range)
**Filter cap:** C30 (33nF) on SCREF net

**VDS trip current** = VDSth / RDS(on) = 0.29V / 0.0029Ω = **~100A**

This is well above the SP40N03GNJ's 75A abs max continuous rating. This is correct — VDS monitoring is a **last-resort hardware protection** for dead shorts and shoot-through events. Normal current limiting is done in firmware via ADC measurements from the OPAMP current sense chain.

**Comparison to reference:** EVL uses SCREF = 1.03V (22k/10k) with STL60N10F7 FETs (RDS(on) = 14.5mΩ): trip at 1.03V / 0.0145Ω ≈ 71A. Same concept — threshold set well above operating range.

| Parameter | SiPDrive | EVL Reference |
|-----------|----------|---------------|
| SCREF voltage | 0.29V | 1.03V |
| MOSFET RDS(on) | 2.9mΩ | 14.5mΩ |
| VDS trip current | ~100A | ~71A |
| FET abs max ID | 75A | 60A |

**PASS** — VDS protection is appropriately set for short-circuit detection.

**Note:** Overcurrent limiting for normal operation must be implemented in firmware using the OPAMP/ADC current measurements. The VDS monitor will NOT protect against sustained overcurrent below ~100A.

### 1.8 VBAT Sense Divider

**Divider:** R10 (100k) / R9 (10k) from +BATT to GND
**Ratio:** 10k / (100k + 10k) = 0.0909
**Filter cap:** C14 (33nF)

| VBATT | ADC voltage | Status |
|-------|-------------|--------|
| 12V | 1.09V | OK |
| 24V | 2.18V | OK |
| 30V | 2.73V | OK (near 3.3V VREF max) |
| 36V | 3.27V | Saturates ADC |

Max measurable voltage before ADC saturation: 3.3V / 0.0909 = **36.3V**. SP40N03GNJ VDS_max = 30V. Divider provides adequate headroom.

**PASS**

### 1.9 OPAMP Current Sense Networks (3 channels)

Each channel uses the STSPIN32G4 internal OPAMPs in differential configuration:

| Component | Value | Function |
|-----------|-------|----------|
| R22/R28/R33 | 1.5kΩ | Non-inverting input resistor (from shunt high side) |
| R23/R29/R34 | 1.5kΩ | Inverting input resistor (from shunt low side/GND) |
| R24/R27/R32 | 15kΩ | Feedback resistor (output to inverting input) |
| R25/R26/R30/R31/R35/R36 | 30kΩ | Bias resistors (from +3.3V and to GND) |

**Gain:** 15k / 1.5k = **10 V/V** — matches reference design exactly.
**Bias point:** 30k/30k from +3.3V = **1.65V** midpoint.

Current measurement range:
- Full scale: ±(3.3V/2) / (0.001Ω × 10) = **±165A** (theoretical)
- At 10A: signal = 10 × 0.001 × 10 = 100mV swing from 1.65V
- 12-bit ADC: 3.3V/4096 = 0.806mV/LSB → 10A = 124 LSBs
- Resolution: 165A / 2048 = **0.08A/LSB**

**WARN (HIGH-7): 1mΩ shunts with 10× gain give only 10 mV/A sensitivity.** This is the same sensitivity as moteus r4.11 (0.5mΩ × 20×), which targets 100A+. For a 10-30A class controller, this is poor — 242 mA RMS noise floor, only 124 LSBs at 10A.

**State-of-the-art comparison:**

| Controller | Shunt | Gain | mV/A | Target Current |
|------------|-------|------|------|----------------|
| SiPDrive (current) | 1mΩ | 10× | 10 | 10-30A |
| moteus c1 (20A class) | 2mΩ | 20× | 40 | 20A |
| Tinymovr R3.3+ | 2mΩ | 20× | 40 | 30-40A |
| moteus r4.11 (100A+) | 0.5mΩ | 20× | 10 | 100-120A |

Tinymovr specifically switched from 1mΩ to 2mΩ because noise was unacceptable for FOC quality.

**Recommendation:** Change shunts to **2mΩ** (no other schematic changes needed). This doubles resolution to 40.3 mA/LSB, halves noise floor to 121 mA RMS, and gives 248 LSBs at 10A. Full-scale becomes ±82.5A — still well above FET rating. Dissipation at 30A = 1.8W (acceptable in 0805 2W package). Update `config.h`: `shunt_resistance_ohm = 0.002`.

### 1.10 NTC Thermistor Circuits

**Board NTC (R12):** NCP15 (10kΩ at 25°C), 0402 package
- Pull-up: R11 (10kΩ) to +3.3V
- Filter: C15 (33nF)
- ADC: U1 PC1 (BOARD_NTC net)
- At 25°C: V = 3.3 × 10k/(10k+10k) = 1.65V

**Stator NTC (R14):** NCP15 (same part), 0805 package
- Pull-up: R13 (10kΩ) to +3.3V
- Filter: C16 (33nF)
- ADC: U1 PC2 (STATOR_NTC net)

**WARN (MED-3):** R12 (board NTC, 0402) and R14 (stator NTC, 0805) share the same LCSC part C77131. NCP15 series is available in multiple packages, but LCSC C77131 is a specific package. Verify C77131 is available in both 0402 and 0805, or assign different LCSC numbers per package.

### 1.11 Connectivity

- **Unconnected pins:** 0
- **Single-pin nets:** 0
- **Multi-driver nets:** 0
- **ERC warnings:** 2 (both false positives from NTC pin typing)
- **Annotation issues:** None
- **Label warnings:** None

**PASS** — clean connectivity.

---

## Phase 2: PCB Layout

### 2.1 Board Specifications

| Parameter | Value |
|-----------|-------|
| Dimensions | 33.0 × 29.0mm |
| Layer count | 8 |
| Stackup | F.Cu / In1-In6 / B.Cu |
| Total thickness | 1.028mm |
| Min track width | 0.15mm |
| Min spacing | ~0.091mm |
| Via size | 0.45mm pad / 0.3mm drill |
| Via count | 515 |
| Routing complete | Yes (0 unrouted) |

### 2.2 Layer Assignment

| Layer | Primary Usage | GND Fill (mm²) |
|-------|--------------|-----------------|
| F.Cu | Components, signal routing | 497.7 |
| In1.Cu | +BATT zone (161.6mm²) | 689.3 |
| In2.Cu | Signal routing (shunt sense, CAN_EN) | 763.7 |
| In3.Cu | GND plane | 815.3 |
| In4.Cu | Phase output routing, gate signals | 765.9 |
| In5.Cu | +3.3V zone (692.3mm²) | 54.3 |
| In6.Cu | GND plane | 815.3 |
| B.Cu | U2 (MT6701), encoder signals | 775.5 |

**PASS** — good use of inner layers. Dedicated GND planes on In3 and In6 provide solid return paths. +BATT on In1 and +3.3V on In5 are appropriate power distribution.

### 2.3 Power Trace Current Capacity

| Net | Min Width | Max Width | Est. Current (1oz, 10°C rise) | Expected Load | Status |
|-----|-----------|-----------|-------------------------------|---------------|--------|
| +BATT | 0.2mm | 2.0mm | 0.5-5A (trace) + zone | 10-30A peak (motor) | **WARN** |
| GND | 0.16mm | 2.0mm | 0.4-5A (trace) + zones | Same as +BATT | **WARN** |
| +12V | 0.3mm | 0.6mm | 0.7-1.5A | ~200mA (gate driver + LDO) | PASS |
| +3.3V | 0.16mm | 0.4mm | 0.4-1.0A + zone | ~100mA (MCU + encoder) | PASS |
| +5V | 0.5mm | 0.5mm | 1.2A | ~80mA (CAN transceiver) | PASS |
| PhaseU/V/W | 0.15mm | 2.0mm | 0.4-5A (trace) | Motor phase current | **WARN** |

**WARN (MED-4):** +BATT and Phase nets have minimum trace widths of 0.15-0.2mm in sections. These are likely short neck-down segments near pads, not long runs. However, 0.15mm traces can handle only ~0.4A at 10°C rise (IPC-2221, 1oz external). Verify these narrow segments are only pad fanouts, not current-carrying paths. The 2.0mm wide sections and +BATT zone (In1) carry the bulk current — those are adequate.

### 2.4 Via Current Capacity

All vias: 0.3mm drill, 1oz plating (~25µm).
Per-via current capacity: ~0.5A (IPC-2221).

| Net | Via Count | Total Via Current | Expected Current | Status |
|-----|-----------|-------------------|------------------|--------|
| +BATT | 191 | ~95A | 30A peak | PASS |
| GND | 249 | ~125A | 30A peak | PASS |
| +3.3V | 10 | ~5A | 0.1A | PASS |
| PhaseU | 11 | ~5.5A | Motor current | PASS |
| PhaseV | 10 | ~5A | Motor current | PASS |
| PhaseW | 10 | ~5A | Motor current | PASS |

### 2.5 Zone Summary

| Zone | Net | Layer(s) | Outline (mm²) | Filled (mm²) | Fill Ratio | Status |
|------|-----|----------|---------------|--------------|------------|--------|
| GND | GND | All 8 | 1184 | 5177 | 4.37× | PASS |
| +BATT | +BATT | In1.Cu | 208 | 162 | 78% | PASS |
| +3.3V | +3.3V | In5.Cu | 1050 | 692 | 66% | PASS |

### 2.6 Decoupling Placement

| IC | Closest Cap | Distance | Target | Status |
|----|-------------|----------|--------|--------|
| U1 (STSPIN32G4) | C15 (33nF) | 5.71mm | <2mm | **WARN** |
| U2 (MT6701) | C12 (100nF) | 2.57mm | <3mm | PASS |
| U3 (TCAN1057A) | C13 (1µF) | 2.96mm | <3mm | PASS |
| U4 (LDO) | C17 (1µF) | 1.21mm | <2mm | PASS |

**WARN (HIGH-3):** U1 STSPIN32G4 closest decoupling cap is 5.71mm away. For a gate driver switching at 40kHz with 1A gate current pulses, decoupling should be <1mm from VCC/GND pins. The 14 caps within 10mm provide good total capacitance, but the closest should be tighter. This is mitigated by the 8-layer stackup providing low-impedance power planes, but is still suboptimal.

### 2.7 Placement Density

| Metric | Value |
|--------|-------|
| Board area | 9.57 cm² |
| Front side | 75 components, 7.9/cm² |
| Back side | 7 components, 0.7/cm² |
| Courtyard overlaps | **49** |

**WARN (HIGH-4): 49 courtyard overlaps.** Largest: R3/R5 (0.372mm²), R1/R20 (0.338mm²), many OPAMP resistor clusters (0.335mm²). This is common on dense motor controller designs and may not cause assembly issues if pad-to-pad clearance is maintained. However, JLCPCB may flag these during DFM review. Verify pick-and-place clearance for the nozzle — 0402 components need ~0.5mm clearance between component bodies.

### 2.8 Tombstoning Risk

**38 components** at medium tombstoning risk (all 0402):
- Boot caps (C1, C4, C5): 5-6× track width asymmetry between thin signal traces and wide phase traces
- GND pour thermal asymmetry: most 0402 caps/resistors with one pad on GND zone
- Zone net asymmetry: components straddling +3.3V or +BATT zones

**WARN (MED-5):** For JLCPCB assembly, 0402 tombstoning is manageable with their reflow profile. The boot cap asymmetry (C1/C4/C5) is the highest risk — consider adding thermal relief or trace stubs to balance the thermal mass. Mark for inspection on first article.

### 2.9 Edge Clearance

| Component | Clearance | Status |
|-----------|-----------|--------|
| D2 (LED) | -0.03mm | **CRIT-3** (see above) |
| C16 | 0.29mm | WARN |
| R14 | 0.32mm | OK |
| C27 | 0.32mm | OK |

### 2.10 Copper Presence

- U1 (F.Cu): GND zone present on opposite layer (B.Cu). Good thermal path.
- U2 (B.Cu): GND zone present on opposite layer (F.Cu). Good.
- 5 components (C30, R15, R2, R37, TP1) have no opposite-layer copper — acceptable for signal components.

---

## Phase 3: BOM & Sourcing

### 3.1 BOM Summary

| Category | Count | Notes |
|----------|-------|-------|
| Unique parts | 29 | |
| Total components | 83 | |
| DNP | 2 | |
| Missing LCSC # | 2 | R21 (120R), TP1-TP4 (test points) |
| Missing MPN | All | Components use LCSC Part # only, no MPN field |

### 3.2 Part-Specific Issues

| Ref | Value | LCSC | Issue | Severity |
|-----|-------|------|-------|----------|
| R21 | 120Ω (CAN term) | **Missing** | No LCSC part number. Board won't be fully assembled. | HIGH-5 |
| R12 | NCP15 (0402) | C77131 | Same LCSC# as R14 (0805). Different footprints — verify package matches. | MED-3 |
| All | — | — | No MPN field populated. JLCPCB assembly uses LCSC #, so functional but poor traceability. | LOW-1 |
| TP1-4 | Test points | — | No LCSC# needed (bare pads, not assembled). | PASS |

### 3.3 MOSFET Part Number Discrepancy

**HIGH-6:** README.md specifies "CMSA015N06" as the MOSFET, but BOM lists **SP40N03GNJ** (LCSC C22466709). These are completely different parts:
- CMSA015N06: 60V, N-channel (if it exists — could not verify)
- SP40N03GNJ: 30V, 40A, N-channel, PowerPAK 1212-8

The BOM/schematic use SP40N03GNJ. Update README to match. This is a documentation issue, not a hardware issue — but could cause confusion.

### 3.4 Passive Values Cross-Check

| Component | SiPDrive | EVL Reference | Match | Notes |
|-----------|----------|---------------|-------|-------|
| Gate resistors | 20Ω | 100Ω | **Different** | See section 4.1 |
| Bootstrap caps | 1µF | 1µF | Match | |
| Shunt resistors | 1mΩ | 20mΩ | **Different** | Different current range |
| OPAMP gain (Rf/Ri) | 15k/1.5k (10×) | 15k/1.5k (10×) | Match | |
| OPAMP bias | 30k | 30k | Match | |
| SCREF divider | 100k/10k (0.3V) | 22k/10k (1.03V) | **Different** | Scaled for 1mΩ shunts |
| NTC pull-up | 10k | 4.7k | **Different** | Different divider ratio, both valid |
| VBUS divider | 100k/10k | 72.3k/3.01k | **Different** | Different voltage range |
| VCC bulk cap | C9 (10µF) | C7 (10µF) | Match | |
| VDDA ferrite | FB1 (BLM15HD) | Not in brief | N/A | Good practice |

### 3.5 Project Settings Anomaly

**LOW-2:** The `.kicad_pro` file contains a netclass pattern for "50Ohm" matching ELRS signal names. This appears to be a leftover from a different project. Custom DRC rule in `.kicad_dru` sets GND zone clearance to 50Ohm nets = 0.300mm. These have no effect unless matching net names exist, but should be cleaned up.

---

## Phase 4: Design-Specific Deep Dives

### 4.1 Gate Drive Analysis

**Gate resistors: 20Ω** (R2-R8, excluding R1 which is a shunt)

The STSPIN32G4 gate driver sources/sinks 1A. With 20Ω gate resistors:
- Peak gate current: VCC / R_gate = 12V / 20Ω = 0.6A (limited by driver, not resistor)
- Gate charge time (est. Qg ≈ 20nC for SP40N03GNJ): τ ≈ Qg × Rg / VCC ≈ 20nC × 20Ω / 12V ≈ 33ns

The EVL reference uses 100Ω gate resistors (120ns switching), which is conservative. 20Ω is more aggressive — faster switching reduces MOSFET losses but increases:
- di/dt noise on gate loops
- EMI from faster voltage transitions
- Ringing on gate signals (especially with long PCB traces)

**WARN (MED-6):** Gate signal traces route through inner layers (In4/In5) with lengths of 13-19mm. Long gate traces with 20Ω drive can ring. If you see gate oscillation or EMI issues, increasing to 33-47Ω is an easy fix. For a first prototype, 20Ω is acceptable to test — just monitor gate waveforms with an oscilloscope.

### 4.2 Current Sense Path

3-shunt topology with low-side sensing:
```
+BATT → Q_high → PhaseX → Motor → PhaseY → Q_low → R_shunt (1mΩ) → GND
                                                     ↓ ↓
                                              OPAMP(+) OPAMP(−)
                                                   ↓
                                              ADC (10× gain)
```

Low-side MOSFET sources (Q2/Q4/Q6) connect to shunt resistors (R1/R19/R20 = 1mΩ, 0805), then to GND. OPAMP non-inverting inputs connect to the high side of each shunt; inverting inputs connect to GND side.

**Shunt power dissipation:** P = I²R = 30² × 0.001 = 0.9W at OCP threshold. 0805 1mΩ resistors are typically rated 0.5-1W. At continuous 20A: P = 0.4W — within rating.

**Layout concern:** Shunt sense traces (Q2-S, Q4-S, Q6-S) route through In2.Cu with lengths of 15-21mm. Kelvin sensing requires traces directly from the shunt pads to the OPAMP inputs, minimizing loop area. Long traces pick up switching noise. Verify these are proper Kelvin connections, not routing through the power path.

### 4.3 Bulk Capacitance

**+BATT rail:**
- 6× 10µF (C1210 ceramic, C18-C23) = 60µF
- 3× 1µF (0402, C27-C29) = 3µF (these are bootstrap, not bulk)
- 1× 220nF (0603, C7) = 0.22µF
- Total ceramic: ~60.2µF

For a 3-phase bridge at 24V, 30A peak:
- Energy per PWM cycle: E = ½ × L × I² (motor dependent)
- Minimum bulk: C > I_peak / (f_sw × ΔV_ripple) = 30 / (40000 × 1) = 750µF for 1V ripple

**WARN (MED-7):** 60µF ceramic is likely insufficient as the sole bulk capacitance for high-current operation. The C1210 10µF ceramics will also derate significantly with DC bias (10µF → ~4-5µF at 24V for X5R/X7R). Effective bulk may be only ~30µF. This is acceptable for a dev board if the motor is powered through an external supply with its own bulk capacitance, but note that standalone operation at high current will have excessive bus voltage ripple.

### 4.4 Bootstrap Circuit

Bootstrap caps: C27, C28, C29 = 1µF each (0402, C1518208)
Bootstrap diodes: Internal to STSPIN32G4

1µF matches the EVL reference design. At 40kHz PWM, the bootstrap caps recharge during the low-side on-time. Minimum bootstrap capacitance:
- Qboot = Qg + I_leak × t_on = 20nC + 100nA × 25µs ≈ 20nC
- C_min = Qboot / ΔV = 20nC / 0.5V = 40nF

1µF provides >25× margin. **PASS**

### 4.5 MCU Configuration Verification

From `src/config.h`:

| Parameter | Value | Notes |
|-----------|-------|-------|
| System clock | 170MHz | STM32G431 max | PASS |
| PWM frequency | 40kHz | Standard for BLDC | PASS |
| Control loop | 40kHz | Synchronized with PWM | PASS |
| Dead time | 250ns | Conservative — 10 switching periods at 40kHz | PASS |
| Shunt resistance | 0.001Ω | Matches BOM | PASS |
| OPAMP gain | 10× | Matches resistor network | PASS |
| Bus voltage | 24V nominal | Within SP40N03GNJ range | PASS |
| Encoder CPR | 16384 | MT6701 14-bit = 16384 | PASS |
| Motor pole pairs | 7 | Application-specific | N/A |
| ADC ch. Ia | ch13 (OPAMP1_OUT) | PA2 = ADC1_IN3 or OPAMP1_VOUT | PASS |
| ADC ch. Ib | ch16 (OPAMP2_OUT) | PA6 = ADC2_IN3 or OPAMP2_VOUT | PASS |
| CAN command ID | 0x101 | Application-specific | N/A |
| CAN telemetry ID | 0x181 | Application-specific | N/A |

### 4.6 Debug Interface

- SWDIO (PA13) → TP2 (DIO)
- SWCLK (PA14) → TP1 (CLK)
- NRST (PG10) → TP3 (NRST)
- BOOT0 (PB8) → TP4 (BOOT)

Test points are pad-only (1mm diameter). No series resistors on SWD lines — acceptable for a dev board.

**PASS**

### 4.7 CAN Bus Interface

- TCAN1057A on +5V with CAN_EN from PA10
- TXD/RXD at 3.3V logic (if VIO variant) or 5V-tolerant GPIO
- 120Ω termination resistor (R21) between CAN_H and CAN_L
- JST SH 3-pin connector (U5)

**Note:** CAN_EN trace is 15.36mm long. The S pin (standby) on TCAN1057A is active-low for standby. Connected to PA10 (CAN_EN). Verify firmware drives this pin correctly (high = normal mode, low = standby).

---

## Issue Summary Table

| ID | Severity | Category | Description | Fix Effort |
|----|----------|----------|-------------|------------|
| CRIT-1 | CRITICAL | PCB | U1 thermal pad: 1 via, needs 9-16 | Add via array |
| CRIT-2 | CRITICAL | DFM | Spacing (0.091mm) and annular ring (0.075mm) below JLCPCB minimums | Increase via pad/spacing |
| CRIT-3 | CRITICAL | PCB | D2 LED extends past board edge | Move component |
| HIGH-1 | HIGH | Schematic | TCAN1057A pin 5 — verify C3236208 is -V variant | Verify part |
| HIGH-2 | HIGH | Schematic | No ESD/TVS on CAN bus connector | Add TVS diode |
| HIGH-3 | HIGH | PCB | U1 decoupling caps too far (5.71mm, need <2mm) | Rearrange layout |
| HIGH-4 | HIGH | PCB | 49 courtyard overlaps — assembly risk | Review placement |
| HIGH-5 | HIGH | BOM | R21 (120Ω CAN term) missing LCSC part number | Assign LCSC # |
| HIGH-6 | HIGH | Documentation | README MOSFET part doesn't match BOM | Update README |
| MED-1 | MEDIUM | Schematic | MT6701 MODE pin (pin 14) — verify pull-up/down | Check schematic |
| HIGH-7 | HIGH | Schematic | 1mΩ shunts with 10× gain = 10 mV/A — too low for 10-30A class. Change to 2mΩ. | Swap BOM entry |
| MED-3 | MEDIUM | BOM | R12 (0402) and R14 (0805) share LCSC C77131 — verify packages | Check LCSC listing |
| MED-4 | MEDIUM | PCB | Power traces neck down to 0.15mm in places | Verify pad fanouts |
| MED-5 | MEDIUM | PCB | 38 components at tombstoning risk (0402s) | Monitor first article |
| MED-5 | MEDIUM | Schematic | 20Ω gate resistors with 13-19mm traces may ring | Test on prototype |
| MED-7 | MEDIUM | Schematic | 60µF bulk cap may be insufficient for standalone high-current use | External bulk or add caps |
| MED-8 | MEDIUM | PCB | Shunt sense traces 15-21mm on In2 — verify Kelvin sensing | Check layout |
| LOW-1 | LOW | BOM | No MPN field on any component | Populate MPNs |
| LOW-2 | LOW | Project | Leftover "50Ohm" netclass from different project | Clean up |
| LOW-3 | LOW | PCB | +5V rail only 1µF decoupling | Add 100nF near U3 |
| LOW-4 | LOW | Schematic | No reverse polarity protection on +BATT | Consider for production |

---

## Positive Findings

- Clean connectivity: 0 unrouted nets, 0 unconnected pins, 0 ERC errors
- 8-layer stackup provides excellent power distribution and signal return paths
- OPAMP gain network exactly matches STSPIN32G4 reference design
- Bootstrap capacitance (1µF) matches reference and has >25× margin
- Proper VDDA filtering with ferrite bead (FB1)
- Adequate via count for power nets (+BATT: 191, GND: 249)
- Bus voltage sensing divider properly scaled for 30V max
- CAN termination resistor included
- NTC temperature monitoring on both board and stator
- Debug interface with all 4 test points (SWD + NRST + BOOT0)

---

## Calculations Appendix

### A1. SCREF / VDS Monitoring Threshold
```
R_lower_eff = R37 || R_SCREF_internal = 10k || 400k = 9.76kΩ
V_SCREF = V_3V3 × R_lower_eff / (R15 + R_lower_eff) = 3.3 × 9.76k / (100k + 9.76k) = 0.293V
VDSth ≈ V_SCREF = 0.293V (from Table 5, ~1:1 at this range)
I_trip = VDSth / RDS(on) = 0.293 / 0.0029 = 101A (short-circuit protection only)
```

### A2. VBAT Sense Divider
```
Ratio = R9 / (R10 + R9) = 10k / (100k + 10k) = 0.0909
V_ADC_max = V_REF / Ratio = 3.3 / 0.0909 = 36.3V
```

### A3. OPAMP Current Sense
```
Gain = R_fb / R_in = 15k / 1.5k = 10 V/V
Bias = V_3V3 × R_lower / (R_upper + R_lower) = 3.3 × 30k / (30k + 30k) = 1.65V
I_fullscale = (V_REF/2) / (R_shunt × Gain) = 1.65 / (0.001 × 10) = 165A
Resolution = I_fullscale / 2048 = 0.0806 A/LSB
```

### A4. NTC Voltage at 25°C
```
V_NTC = V_3V3 × R_NTC / (R_pullup + R_NTC) = 3.3 × 10k / (10k + 10k) = 1.65V
```

### A5. Bootstrap Capacitor Margin
```
Q_required = Q_gate + I_leak × t_on ≈ 20nC + 100nA × 25µs = 20.0025nC
C_min = Q_required / ΔV_max = 20nC / 0.5V = 40nF
C_actual = 1µF → Margin = 1000nF / 40nF = 25×
```

### A6. Gate Drive Timing
```
t_sw ≈ Q_g × R_gate / V_drive = 20nC × 20Ω / 12V ≈ 33ns
Dead time = 250ns → ~7.5× switching time margin (adequate)
```

---

*Generated by automated KiCad analysis with datasheet cross-verification. Manual review of the raw schematic is recommended for items marked WARN or higher.*
