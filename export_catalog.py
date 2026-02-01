import argparse
import os
import sys

from i18n_qa import I18nEngine


def main():
    parser = argparse.ArgumentParser(description="Export the Mycelia CSS catalog as a binary.")
    parser.add_argument("output", nargs="?", default="final_vision_v1.2.bin", help="Target filename for the exported catalog.")
    args = parser.parse_args()

    lib_name = "i18n_engine.dll" if os.name == "nt" else "libi18n_engine.so"
    lib_path = os.path.join(os.getcwd(), lib_name)
    if not os.path.exists(lib_path):
        print(f"{lib_name} not found, build the engine (`make`) first.", file=sys.stderr)
        sys.exit(1)

    engine = I18nEngine(lib_path)
    engine.load_file("tailwind_style_catalog.i18n")
    abs_output = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(abs_output) or ".", exist_ok=True)
    engine.export_binary(abs_output)
    print(f"Exported binary catalog to {abs_output}")


if __name__ == "__main__":
    main()
