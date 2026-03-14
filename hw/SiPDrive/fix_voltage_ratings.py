#!/usr/bin/env python3
"""Fix 100nF cap voltage ratings (16V -> 35V) and replace bulk caps."""

from skip import Schematic

SCH_PATH = "SiPDrive.kicad_sch"


def main():
    sch = Schematic(SCH_PATH)
    changed = []

    for symbol in sch.symbol:
        prop = symbol.property
        ref = prop.Reference.value if hasattr(prop, 'Reference') else None
        val = prop.Value.value if hasattr(prop, 'Value') else None

        if not ref or not val:
            continue

        # Fix 1: 100nF caps - upgrade from 16V (C307380) to 35V (C181043)
        if val == "100n" and hasattr(prop, 'LCSC') and prop.LCSC.value == "C307380":
            prop.LCSC.value = "C181043"
            changed.append(f"  {ref}: 100nF LCSC C307380 (16V) -> C181043 (35V GRM033R6YA104KE14D)")

        # Fix 2: Bulk caps C18-C23 - replace with GCM31CD71H106KE36L (50V 1206)
        if ref in {"C18", "C19", "C20", "C21", "C22", "C23"}:
            if hasattr(prop, 'Footprint'):
                old_fp = prop.Footprint.value
                prop.Footprint.value = "Capacitor_SMD:C_1206_3216Metric"
                if hasattr(prop, 'LCSC'):
                    old_lcsc = prop.LCSC.value
                    prop.LCSC.value = "C5119295"
                    prop.Value.value = "10uF/50V"
                    changed.append(f"  {ref}: {old_fp} -> C_1206 | LCSC: {old_lcsc} -> C5119295 (GCM31CD71H106KE36L 50V)")

    print(f"\n=== Voltage Rating Fixes ===")
    print(f"\nChanged ({len(changed)}):")
    for c in sorted(changed):
        print(c)

    if changed:
        sch.write(SCH_PATH)
        print(f"\nSaved {len(changed)} changes to {SCH_PATH}")
    else:
        print("\nNo changes needed.")


if __name__ == "__main__":
    main()
