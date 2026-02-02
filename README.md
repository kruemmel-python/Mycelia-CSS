# Mycelia CSS — Native Style & Physics Engine
<img width="2752" height="1536" alt="infogEN" src="https://github.com/user-attachments/assets/7e8f40b8-1529-4f18-84c9-6c96798a8ded" />

This repository hosts the **Mycelia I18n/Style Engine**, a hybrid translation/style catalog that is now also a **physics-aware layout driver**. The native C++ DLL stores `style_` tokens in the catalog, resolves placeholder values recursively, converts CSS concepts (e.g., `--mass`, `gap`, `transition`) into physical parameters, and exposes them for C#/Unity clients or native demos with zero-copy access via file mapping.

## Key Components


https://github.com/user-attachments/assets/9cb503e4-137f-4fc1-832e-ca694b636cfd


| Component | Purpose |
| --- | --- |
| `i18n_engine.cpp/.h` | Core engine: parsing, catalog snapshots, style registry, native style evaluation, binary export with FNV1a32 integrity. |
| `i18n_api.cpp/.h` | C ABI wrapper that exposes translation, native style lookup (`i18n_get_native_style`), catalog reloads, metadata queries, and helper constructors/destructors (`i18n_create`, `i18n_destroy`). |
| `I18n.cs` | .NET wrapper (Unity-friendly) that mirrors the API, adds `GetStyle`, and exposes `NativeStyle` (includes `Spacing`) along with helper methods to feed `Translate` with dynamic args. |
| `i18n_qa.py` | QA harness that loads `tailwind_style_catalog.i18n`, asserts style inheritance, placeholder injection, native physics translation, spacing, and exports a binary catalog for validation. |
| `main.cpp` + `mycelia_test.exe` | Zero-copy demo: prints mass/friction/spacing, shows ANSI color swatches, and implements a file-watch live-reload loop that hot-swaps the catalog when `tailwind_style_catalog.i18n` changes. |
| `tpl_main-layout` + `render_to_html` | Template token demonstrating HTML layout and the new `RenderToHtml` API that composes CSS class definitions for referenced `style_` tokens, producing a ready-to-serve HTML page. |
| `tailwind_style_catalog.i18n` | Tailwind-inspired tokens with CSS properties, physics directives (`--mass`, `--friction`, `--spacing`), style inheritance, and hover/focus variants. |
| `tailwind_config_example.i18n` | Tailwind config-like source of truth that you can port directly into your catalog (extends colors, spacing, transitions). |
| `write_assets.py` | Helper that generates placeholder SVG artwork under `www/assets/`, ensuring `tpl_image-card` tokens have actual images without fetching external files. |

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

## Rendering Tailwind Templates

Templates live under tokens prefixed with `tpl_` (see `tpl_main-layout` in `tailwind_style_catalog.i18n`). They contain the HTML skeleton, Tailwind-style `@style_*` references, and placeholders (`%0` … `%n`) for dynamic content.

The new `RenderToHtml` API (native: `i18n_render_to_html`; managed: `I18n.RenderToHtml`) resolves `%n` placeholders, collects every `@style_*` reference, emits a `<style>` block with real CSS definitions, and swaps each `@style_*` into a sanitized CSS class name. Call this API to render a full page, for example:

```csharp
var html = i18n.RenderToHtml("tpl_main-layout", new[] { "#0f172a", "#f8fafc", "#0ea5e9", "#fecdd3", "Willkommen", "<p>Physikalischer Content</p>" });
```

Because this happens inside the DLL, the resulting string contains fully resolved CSS and is safe to stream to a UI context (Unity, web socket, Electron, etc.).

## Generating a standalone HTML page

The repository now includes `write_assets.py` plus the enhanced `generate_html.py`. `write_assets.py` emits twelve SVGs into `www/assets/` so `tpl_image-card` can resolve real artwork. `generate_html.py` then loads `tailwind_style_catalog.i18n`, renders `tpl_full-page` for every entry in `PAGE_DEFINITIONS`, and writes `index.html`, `services.html`, `matrix.html`, and `insights.html` into the `www/` directory. Each page keeps the shared animation + `MyceliaPhysics` JSON payload of the 1.2 release, while the nav/menu buttons stay fully functional because `tpl_full-page` points to the generated files.

```bash
python write_assets.py
python generate_html.py
```

Open any file under `www/` (or serve the entire folder) to inspect the layout that the DLL directly computed. The generator appends `<style>` blocks derived from your `style_*` catalog, replaces every `@style_*` reference with sanitized class names, and inserts a `<script>` that logs the physics data plus animates the floating cube using the same friction/mass values you defined—so editing `.i18n` and rerunning the scripts keeps HTML, CSS, and JS perfectly in sync with the native engine.

## Automated binary export

Whenever you ship a release, run:

```bash
make export
```

This target builds `i18n_engine` (if needed) and invokes `export_catalog.py final_vision_v1.2.bin`, which loads the fresh catalog and calls the native `i18n_export_binary` entry point. The resulting `final_vision_v1.2.bin` is suited for your asset pipeline, and you can commit or publish it alongside the generated HTML site.

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

## Release Highlight

Mycelia CSS v1.2 “The Mesh” is now shipping with:

1. **Hybrid HTML Generator** – `write_assets.py` + `generate_html.py` now churn out four fully linked pages (`index.html`, `services.html`, `matrix.html`, `insights.html`) complete with image cards, navigation, and the `MyceliaPhysics` JSON payload that mirrors your DLL tokens.
2. **Unity Overlay Sample** – `samples/UnityPhysicsOverlay.cs` demonstrates how to `LoadFile`, watch the catalog, call `TryGetNativeStyle`, and push `mass`, `friction`, `spacing`, `restitution` into a Unity ComputeShader before dispatch.
3. **Physics-Aware Assets** – Tailwind tokens such as `style_card-image`, `style_cube-*`, and `tpl_image-card` embed SVG URLs from `www/assets/`, letting the DLL deliver both design and physical behavior from the same catalog.
4. **Animated Hero + Web+Native Sync** – The generated pages inject a `<script>` that animates `#hero-ice` via friction/mass values; the same values feed Unity/C# via `TryGetNativeStyle` and the zero-copy `main.cpp` demo.
5. **Live Reload & QA** – `make` runs `i18n_qa.py` and the live watcher in `main.cpp` now samples the catalog every 500ms, hot-swapping snapshots atomically while the rest of the system keeps running.

## Pipeline Visualization

```
    tailwind_style_catalog.i18n
               │
               ▼
     ┌──────────────────────┐
     │     i18n_engine.dll   │
     │ - parse templates     │
     │ - build style registry│
     │ - emit NativeStyle    │
     └──────────────────────┘
               │
               ▼
   write_assets.py ──┐
               │     ▼
 generate_html.py    ├──> (tpl_full-page for each PAGE_DEFINITIONS)
        │           │
        ▼           └──> inject <style>, MyceliaPhysics JSON, animation script
  www/index.html <--┘
  www/services.html
  www/matrix.html
  www/insights.html
```

Each generated HTML still relies on the same DLL-powered data: templates resolve `@style_*` tokens, `tpl_image-card` materializes `background-image:url('%0')`, and the inserted script uses the DLL’s `MyceliaPhysics` JSON to animate the hero cube.
