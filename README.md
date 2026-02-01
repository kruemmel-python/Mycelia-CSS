# Mycelia CSS — Native Style & Physics Engine

This repository hosts the **Mycelia I18n/Style Engine**, a hybrid translation/style catalog that is now also a **physics-aware layout driver**. The native C++ DLL stores `style_` tokens in the catalog, resolves placeholder values recursively, converts CSS concepts (e.g., `--mass`, `gap`, `transition`) into physical parameters, and exposes them for C#/Unity clients or native demos with zero-copy access via file mapping.

## Key Components

| Component | Purpose |
| --- | --- |
| `i18n_engine.cpp/.h` | Core engine: parsing, catalog snapshots, style registry, native style evaluation, binary export with FNV1a32 integrity. |
| `i18n_api.cpp/.h` | C ABI wrapper that exposes translation, native style lookup (`i18n_get_native_style`), catalog reloads, metadata queries, and helper constructors/destructors (`i18n_create`, `i18n_destroy`). |
| `I18n.cs` | .NET wrapper (Unity-friendly) that mirrors the API, adds `GetStyle`, and exposes `NativeStyle` (includes `Spacing`) along with helper methods to feed `Translate` with dynamic args. |
| `i18n_qa.py` | QA harness that loads `tailwind_style_catalog.i18n`, asserts style inheritance, placeholder injection, native physics translation, spacing, and exports a binary catalog for validation. |
| `main.cpp` + `mycelia_test.exe` | Zero-copy demo: prints mass/friction/spacing, shows ANSI color swatches, and implements a file-watch live-reload loop that hot-swaps the catalog when `tailwind_style_catalog.i18n` changes. |
| `tailwind_style_catalog.i18n` | Tailwind-inspired tokens with CSS properties, physics directives (`--mass`, `--friction`, `--spacing`), style inheritance, and hover/focus variants. |
| `tailwind_config_example.i18n` | Tailwind config-like source of truth that you can port directly into your catalog (extends colors, spacing, transitions). |

## Workflows

### 1. Build + QA

```bash
make
```

This compiles `i18n_engine.dll`, runs the Python QA script (ensuring `style_card` glyphs, placeholder injection, native physics, and spacing), and leaves the DLL ready for consumption.

### 2. Run the zero-copy/live-reload demo

```bash
make run
```

The first invocation compiles the test harness, then prints the current table of `style_cube-heavy`/`style_cube-ice` physical values alongside ANSI color patches. The binary stays alive in a loop — edit `tailwind_style_catalog.i18n` (e.g., change `transition` or `--friction` in `style_cube-ice`) and save. The watcher remaps the catalog instantly, re-evaluates the style, and prints the new friction/mass values before your eyes. Exit with `Ctrl+C`.

### 3. Export binary catalog

```bash
i18n_export_binary(final_vision.bin)
```

Use the DLL to emit a compact binary catalog (includes metadata and FNV1a32 checksum) that your UI/GPU layer can mmap with zero-copy safety.

## Live-Reload Explanation

`main.cpp` now hashes the `tailwind_style_catalog.i18n` modification time every 500ms (`get_last_write_time`) and triggers `load_catalog_force_text` when the file changes. The engine already atomically swaps snapshots internally, so your C++ process continues to use the old data until the new mappings are ready—this is the `Atomic Swapping + Zero Downtime` guarantee. After a reload, the demo also prints the refreshed `style_cube-ice` mass/friction values so you can validate the update in real time.

## Extending the System

* **Add new style tokens**: Extend the `.i18n` file with `style_*` tokens. The parser already understands `@style_*` references and `%n` placeholders.
* **Inject dynamic values**: Use the C# wrapper (`I18n.Translate`) or the native `i18n_translate` API with placeholder arguments to inject runtime data (colors, spacing, GPU uniforms).
* **Expose more physics**: Extend `apply_physical_property` in `i18n_engine.cpp` to map additional `--` properties (e.g., `--gravity-scale`, `--max-speed`) and propagate them to `NativeStyle`.
* **Unity + C# hot reload**: Mirror the file-watcher logic in C# by calling `TryGetNativeStyle` after `LoadFile`/`Reload`, then update GPU buffers as shown in `main.cpp`.

## Troubleshooting

* **“Unknown binary format” when running `make run`** – the loader expects the `.i18n` file to be in text form. The demo falls back to reading the file and calling `i18n_load_txt` if `i18n_load_txt_file` fails.
* **Native style values missing** – ensure the style token contains `--mass`/`--friction` pairs. The QA script (`i18n_qa.py`) asserts presence of these values, so fix your catalog if the QA run fails.
* **ANSI colors don’t render** – use a terminal that supports 24-bit ANSI escapes (most modern terminals do). The demo prints each detected hex color as a swatch plus RGB triple.

## Next steps

1. Build a Unity/WinUI overlay that listens to the catalog hot-reload events via `TryGetNativeStyle` and updates compute shaders with the new `mass`, `friction`, and `spacing`.
2. Add catalog validation presets (e.g., ensuring `style_card` inherits from `style_card-border`) to `i18n_qa.py`.
3. Automate binary deployment by invoking `i18n_export_binary` and committing `final_vision_vX.bin` or publishing it to your asset pipeline.
