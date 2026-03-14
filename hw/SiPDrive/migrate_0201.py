#!/usr/bin/env python3
"""Migrate qualifying 0402 passives to 0201 footprints and update LCSC part numbers."""

from skip import Schematic

SCH_PATH = "SiPDrive.kicad_sch"

# 0201 resistor replacements: value -> (new_footprint, new_lcsc)
RESISTOR_MAP = {
    "20R":  ("Resistor_SMD:R_0201_0603Metric", "C473548"),   # 0201WMF200JTEE 20Ω 5% (fine for gate drive)
    "1.5k": ("Resistor_SMD:R_0201_0603Metric", "C270361"),   # 0201WMF1501TEE 1.5kΩ 1%
    "10k":  ("Resistor_SMD:R_0201_0603Metric", "C473048"),   # 0201WMF1002TEE 10kΩ 1%
    "15k":  ("Resistor_SMD:R_0201_0603Metric", "C473452"),   # 0201WMF1502TEE 15kΩ 1%
    "30k":  ("Resistor_SMD:R_0201_0603Metric", "C246933"),   # 0201WMF3002TEE 30kΩ 1%
    "100k": ("Resistor_SMD:R_0201_0603Metric", "C270364"),   # 0201WMF1003TEE 100kΩ 1%
}

# 0201 capacitor replacements: value -> (new_footprint, new_lcsc)
CAPACITOR_MAP = {
    "100n": ("Capacitor_SMD:C_0201_0603Metric", "C307380"),  # CL03A104KO3NNNC 100nF 16V X5R
    "33n":  ("Capacitor_SMD:C_0201_0603Metric", "C161475"),  # GRM033R61C333KE84D 33nF 16V X5R
}

# Components to SKIP (keep as-is even if they match a value above)
SKIP_REFS = {
    "R12",  # NTC thermistor - specialized part
    "R14",  # NTC thermistor - specialized part
    "FB1",  # Ferrite bead - keep 0402 for current rating
    "D2",   # LED - keep 0402
}

# Only migrate components currently in 0402 footprints
FOOTPRINTS_0402 = {
    "Resistor_SMD:R_0402_1005Metric",
    "Capacitor_SMD:C_0402_1005Metric",
    "SiPDrive:R0402",
}

VALUE_MAP = {**RESISTOR_MAP, **CAPACITOR_MAP}


def main():
    sch = Schematic(SCH_PATH)
    changed = []
    skipped = []

    for symbol in sch.symbol:
        prop = symbol.property
        ref = prop.Reference.value if hasattr(prop, 'Reference') else None
        val = prop.Value.value if hasattr(prop, 'Value') else None

        if not ref or not val or not hasattr(prop, 'Footprint'):
            continue

        current_fp = prop.Footprint.value

        # Skip non-0402 components
        if current_fp not in FOOTPRINTS_0402:
            continue

        # Skip explicitly excluded components
        if ref in SKIP_REFS:
            skipped.append(f"  {ref} ({val}) - excluded, keeping {current_fp}")
            continue

        if val in VALUE_MAP:
            new_fp, new_lcsc = VALUE_MAP[val]
            old_lcsc = prop.LCSC.value if hasattr(prop, 'LCSC') else "none"
            prop.Footprint.value = new_fp
            if hasattr(prop, 'LCSC'):
                prop.LCSC.value = new_lcsc
            changed.append(f"  {ref}: {val} | {current_fp} -> {new_fp} | LCSC: {old_lcsc} -> {new_lcsc}")
        else:
            skipped.append(f"  {ref} ({val}) - no 0201 mapping, keeping {current_fp}")

    print(f"\n=== 0402 -> 0201 Migration ===")
    print(f"\nChanged ({len(changed)}):")
    for c in sorted(changed):
        print(c)
    print(f"\nSkipped ({len(skipped)}):")
    for s in sorted(skipped):
        print(s)

    if changed:
        sch.write(SCH_PATH)
        print(f"\nSaved {len(changed)} changes to {SCH_PATH}")
    else:
        print("\nNo changes to save.")


if __name__ == "__main__":
    main()
