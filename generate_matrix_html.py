import ctypes
import os
from pathlib import Path

# Konfiguration
DLL_NAME = "i18n_engine.dll"
CATALOG_PATH = "tailwind_style_catalog.i18n"
OUTPUT_DIR = Path("www_matrix")
OUTPUT_PATH = OUTPUT_DIR / "index.html"

# Das Matrix-Grün Design mit CRT-Effekt und vertikalem Log-Layout
EXTRA_STYLE_MATRIX = """
<style>
@import url('https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;700&display=swap');

body {
    margin: 0;
    min-height: 100vh;
    background: #0a0a0a;
    color: #00ff41;
    font-family: 'JetBrains Mono', monospace;
    overflow-x: hidden;
    position: relative;
}

/* CRT Scanline Effekt */
body::before {
    content: " ";
    display: block;
    position: fixed;
    top: 0; left: 0; bottom: 0; right: 0;
    background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%), 
                linear-gradient(90deg, rgba(255, 0, 0, 0.06), rgba(0, 255, 0, 0.02), rgba(0, 0, 255, 0.06));
    z-index: 2000;
    background-size: 100% 3px, 3px 100%;
    pointer-events: none;
}

/* Vertikales Log-Format Überschreibung */
.style_grid-3 {
    display: flex !important;
    flex-direction: column !important;
    gap: 1rem !important;
    max-width: 900px;
    margin: 0 auto;
}

.style_card {
    border: 1px solid #00ff41 !important;
    background: rgba(0, 255, 65, 0.05) !important;
    box-shadow: 0 0 10px rgba(0, 255, 65, 0.2);
    position: relative;
    overflow: hidden;
}

.style_card::before {
    content: "> LOG_ENTRY_";
    font-size: 0.7rem;
    opacity: 0.5;
    display: block;
    margin-bottom: 0.5rem;
}

.style_card-header {
    color: #00ff41 !important;
    text-transform: uppercase;
    letter-spacing: 0.1em;
}

.style_card-body {
    color: #00ff41 !important;
    opacity: 0.8;
}

#hero-ice {
    border: 2px solid #00ff41;
    background: rgba(0, 255, 65, 0.1);
    box-shadow: 0 0 30px #00ff41;
    color: #00ff41 !important;
    clip-path: polygon(5% 0, 100% 0, 95% 100%, 0 100%);
}

.style_nav-bar {
    background: #000 !important;
    border-bottom: 1px solid #00ff41;
}

.style_primary-button {
    background: #00ff41 !important;
    color: #000 !important;
    border-radius: 0 !important;
    font-weight: bold;
}

.style_secondary-button {
    border: 1px solid #00ff41 !important;
    color: #00ff41 !important;
    border-radius: 0 !important;
}

/* Verstecke Bilder für den reinen Terminal-Look */
.style_card-image {
    display: none !important;
}
</style>
"""

# Karten-Definitionen (identisch mit Original, aber wir nutzen Terminal-Logik)
HOME_CARDS = [
    ("tpl_card", ["#00ff41", "Secure_Channel", "Verschlüsselung aktiv. SubQG Integrität gesichert."]),
    ("tpl_card", ["#00ff41", "Global_Mesh", "Node-Präsenz in 100+ Sektoren verifiziert."]),
    ("tpl_card", ["#00ff41", "Physics_Sync", "Reibungswerte werden direkt aus C++ gestreamt."]),
]

SERVICES_CARDS = [
    ("tpl_card", ["#00ff41", "Managed Matrix", "Dedizierte Operatoren routen Daten in Echtzeit."]),
    ("tpl_card", ["#00ff41", "API Fabric", "Starke Contracts mit deterministischen Antwortzeiten."]),
    ("tpl_card", ["#00ff41", "Quantum Observability", "Messungen halten auch bei hoher Masse Performanz."]),
]

MATRIX_CARDS = [
    ("tpl_card", ["#00ff41", "Dynamic Grid", "Echtzeit-Skalen über hunderte Cubes auf der GPU."]),
    ("tpl_card", ["#00ff41", "Live Physics", "Massen, Reibung und Spacing als JSON erreichbar."]),
    ("tpl_card", ["#00ff41", "AI Orchestrator", "Signal-Bäume passen sich an neue Styles an."]),
]

INSIGHTS_CARDS = [
    ("tpl_card", ["#00ff41", "Telemetry", "SubQG-Telemetrien zeigen Health bis in die Tiefe."]),
    ("tpl_card", ["#00ff41", "Forecast", "Voraussagen der Grid-Stabilität basierend auf Physik."]),
    ("tpl_card", ["#00ff41", "SubQG Intelligence", "Decision-Matrix lernt aus jeder Button-Interaktion."]),
]

PAGE_DEFINITIONS = [
    ("index.html", "SUB-QUANTUM TERMINAL V1.2", HOME_CARDS),
    ("services.html", "MATRIX SERVICES", SERVICES_CARDS),
    ("matrix.html", "LIVE GRID MATRIX", MATRIX_CARDS),
    ("insights.html", "SUBQG INSIGHTS", INSIGHTS_CARDS),
]

class NativeEngine:
    def __init__(self, dll_path: str):
        self.lib = ctypes.CDLL(dll_path)
        self.lib.i18n_new.restype = ctypes.c_void_p
        self.lib.i18n_new.argtypes = ()
        self.lib.i18n_free.argtypes = (ctypes.c_void_p,)
        self.lib.i18n_load_txt_file.argtypes = (ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int)
        self.lib.i18n_load_txt.argtypes = (ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int)
        self.lib.i18n_load_txt.restype = ctypes.c_int
        self.lib.i18n_render_to_html.argtypes = (
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_char_p),
            ctypes.c_int,
            ctypes.c_char_p,
            ctypes.c_int,
        )
        self.lib.i18n_render_to_html.restype = ctypes.c_int
        self.lib.i18n_get_physics_json.argtypes = (
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_char_p),
            ctypes.c_int,
            ctypes.c_char_p,
            ctypes.c_int,
        )
        self.lib.i18n_get_physics_json.restype = ctypes.c_int
        self.lib.i18n_last_error_copy.argtypes = (ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int)
        self.lib.i18n_last_error_copy.restype = ctypes.c_int

        self.handle = self.lib.i18n_new()
        if not self.handle:
            raise RuntimeError("Failed to initialize the i18n engine")

    def __del__(self):
        if getattr(self, "handle", None):
            self.lib.i18n_free(self.handle)

    def _last_error(self) -> str:
        buf = ctypes.create_string_buffer(1024)
        length = self.lib.i18n_last_error_copy(self.handle, buf, len(buf))
        if length <= 0:
            return ""
        return buf.value.decode("utf-8", errors="ignore")

    def load_catalog(self, path: str):
        path_bytes = path.encode("utf-8")
        result = self.lib.i18n_load_txt_file(self.handle, path_bytes, 1)
        if result != 0:
            with open(path, "rb") as fh:
                raw = fh.read()
            if self.lib.i18n_load_txt(self.handle, raw, 1) != 0:
                raise RuntimeError(f"Could not load catalog: {self._last_error()}")

    def render_to_html(self, token: str, args: list[str]) -> str:
        arr = (ctypes.c_char_p * len(args))()
        for i, value in enumerate(args):
            arr[i] = value.encode("utf-8")

        token_bytes = token.encode("utf-8")
        args_ptr = arr if args else None
        length = self.lib.i18n_render_to_html(self.handle, token_bytes, args_ptr, len(args), None, 0)
        if length < 0:
            raise RuntimeError(f"Render failed: {self._last_error()}")

        buf = ctypes.create_string_buffer(length + 1)
        if self.lib.i18n_render_to_html(self.handle, token_bytes, args_ptr, len(args), buf, length + 1) < 0:
            raise RuntimeError(f"Render failed: {self._last_error()}")

        return buf.value.decode("utf-8")

    def get_physics_json(self, token: str, args: list[str]) -> str:
        arr = (ctypes.c_char_p * len(args))()
        for i, value in enumerate(args):
            arr[i] = value.encode("utf-8")

        token_bytes = token.encode("utf-8")
        args_ptr = arr if args else None
        length = self.lib.i18n_get_physics_json(self.handle, token_bytes, args_ptr, len(args), None, 0)
        if length < 0:
            raise RuntimeError(f"Could not build physics json: {self._last_error()}")

        buf = ctypes.create_string_buffer(length + 1)
        if self.lib.i18n_get_physics_json(self.handle, token_bytes, args_ptr, len(args), buf, length + 1) < 0:
            raise RuntimeError(f"Could not build physics json: {self._last_error()}")

        return buf.value.decode("utf-8")

def extract_style_blocks(rendered: str) -> tuple[list[str], str]:
    styles: list[str] = []
    remaining = rendered.strip()
    while remaining.startswith("<style>"):
        end = remaining.find("</style>")
        if end == -1:
            break
        end += len("</style>")
        styles.append(remaining[:end])
        remaining = remaining[end:].strip()
    return styles, remaining


def render_card_set(engine: NativeEngine, card_defs: list[tuple[str, list[str]]]) -> tuple[list[str], str]:
    styles: list[str] = []
    bodies: list[str] = []
    for template, args in card_defs:
        rendered_card = engine.render_to_html(template, args)
        card_styles, card_markup = extract_style_blocks(rendered_card)
        styles.extend(card_styles)
        bodies.append(card_markup)
    return styles, "".join(bodies)


def build_explosive_animation(physics_json: str) -> str:
    return (
        "  <script>\n"
        f"    const MyceliaPhysics = {physics_json};\n"
        "    const cubeStyle = MyceliaPhysics['style_cube-ice'] || {mass: 1, friction: 0.1, restitution: 0.9};\n\n"
        "    document.addEventListener('DOMContentLoaded', () => {\n"
        "      const heroIce = document.getElementById('hero-ice');\n"
        "      if (!heroIce) return;\n\n"
        "      const mass = cubeStyle.mass || 1;\n"
        "      const restitution = 0.95; // Wir erzwingen explosive Restitution\n"
        "      const friction = cubeStyle.friction || 0.1;\n\n"
        "      function animate(time) {\n"
        "        // Explosives Jitter-Verhalten basierend auf Restitution\n"
        "        const jitterX = (Math.random() - 0.5) * restitution * 15;\n"
        "        const jitterY = (Math.random() - 0.5) * restitution * 15;\n"
        "        const scale = 1 + (Math.sin(time / 50) * 0.02 * mass);\n\n"
        "        heroIce.style.transform = `translate(${jitterX}px, ${jitterY}px) scale(${scale})`;\n"
        "        \n"
        "        // Zufälliges Flackern (Glitch-Effekt)\n"
        "        heroIce.style.opacity = Math.random() > 0.98 ? '0.3' : '1';\n\n"
        "        requestAnimationFrame(animate);\n"
        "      }\n"
        "      requestAnimationFrame(animate);\n"
        "    });\n"
        "  </script>\n"
    )

def build_page(engine: NativeEngine, hero_title: str, card_defs: list, output_path: Path):
    card_styles, grid_content = render_card_set(engine, card_defs)
    html_payload = engine.render_to_html("tpl_full-page", [hero_title, grid_content])
    template_styles, body_markup = extract_style_blocks(html_payload)
    style_block = "\n".join(template_styles + card_styles + [EXTRA_STYLE_MATRIX])

    physics_json = engine.get_physics_json("tpl_full-page", [hero_title, grid_content])
    script = build_explosive_animation(physics_json)

    document = f"<!doctype html><html lang='de'><head><meta charset='utf-8'><title>SubQG Matrix Terminal</title>{style_block}{script}</head><body>{body_markup}</body></html>"
    output_path.write_text(document, encoding="utf-8")

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    engine = NativeEngine(os.path.join(os.getcwd(), DLL_NAME))
    engine.load_catalog(CATALOG_PATH)

    for filename, title, cards in PAGE_DEFINITIONS:
        build_page(engine, title, cards, OUTPUT_DIR / filename)
        print(f"Terminal-Seite generiert: {OUTPUT_DIR / filename}")

if __name__ == "__main__":
    main()
