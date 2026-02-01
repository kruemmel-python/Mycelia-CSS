import ctypes
import os
from pathlib import Path

DLL_NAME = "i18n_engine.dll"
CATALOG_PATH = "tailwind_style_catalog.i18n"
OUTPUT_PATH = Path("www") / "index.html"


EXTRA_STYLE = """
<style>
body {
  margin: 0;
  min-height: 100vh;
  background: radial-gradient(circle at top, rgba(59,130,246,0.35), rgba(15,23,42,0.95)) #020617;
  font-family: 'Manrope', 'Inter', system-ui, sans-serif;
  color: #e2e8ff;
}
main {
  width: 100%;
}
.style_grid-3 article {
  min-height: 190px;
  padding-bottom: 1rem;
}
#hero-ice {
  min-width: 260px;
  min-height: 160px;
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #0f172a;
  letter-spacing: 0.35em;
  text-transform: uppercase;
  transition: transform 400ms ease, box-shadow 400ms ease;
}
#hero-ice::after {
  content: '';
  position: absolute;
  inset: 0;
  border-radius: inherit;
  opacity: 0.3;
  background: linear-gradient(135deg, rgba(59,130,246,0.8), rgba(14,165,233,0.4));
  filter: blur(18px);
  z-index: -1;
}
#hero-ice:hover {
  transform: translateY(-6px) scale(1.01);
  box-shadow: 0 30px 60px rgba(14,165,233,0.8);
}
.style_hero-banner {
  flex-wrap: wrap;
}
@media (max-width: 960px) {
  .style_grid-3 {
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  }
  .style_hero-banner {
    flex-direction: column;
  }
}
</style>
"""

HOME_CARDS = [
    ("tpl_image-card", ["assets/home-grid.svg", "Sichere Kanäle", "<p>Verschlüsselung über den gesamten Stack.</p>"]),
    ("tpl_image-card", ["assets/home-global.svg", "Global Reach", "<p>100+ Länder vertrauen der Matrix.</p>"]),
    ("tpl_image-card", ["assets/home-health.svg", "SubQG Health", "<p>Reibung &amp; Masse in perfektem Balance.</p>"]),
]

SERVICES_CARDS = [
    ("tpl_image-card", ["assets/services-ops.svg", "Managed Matrix", "<p>Dedizierte Operatoren routen Daten in Echtzeit.</p>"]),
    ("tpl_image-card", ["assets/services-api.svg", "API Fabric", "<p>Starke Contracts mit deterministischen Antwortzeiten.</p>"]),
    ("tpl_image-card", ["assets/services-quantum.svg", "Quantum Observability", "<p>Messungen, die bei hoher Masse Performanz halten.</p>"]),
]

MATRIX_CARDS = [
    ("tpl_image-card", ["assets/matrix-grid.svg", "Dynamic Grid", "<p>Echtzeit-Skalen über hunderte Cubes auf der GPU.</p>"]),
    ("tpl_image-card", ["assets/matrix-physics.svg", "Live Physics", "<p>Massen, Reibung und Spacing als JSON.</p>"]),
    ("tpl_image-card", ["assets/matrix-ai.svg", "AI Orchestrator", "<p>Signal-Baum passt sich an neue Tailwind-Styles an.</p>"]),
]

INSIGHTS_CARDS = [
    ("tpl_image-card", ["assets/insights-telemetry.svg", "Telemetry", "<p>SubQG-Telemetrien zeigen Health bis in die Tiefe.</p>"]),
    ("tpl_image-card", ["assets/insights-forecast.svg", "Forecast", "<p>Voraussagen der Grid-Stabilität basierend auf Physik.</p>"]),
    ("tpl_image-card", ["assets/insights-subqg.svg", "SubQG Intelligence", "<p>Decision-Matrix lernt aus jeder Button-Interaktion.</p>"]),
]

PAGE_DEFINITIONS = [
    ("index.html", "All Comprehensive Solution In One Platform", HOME_CARDS),
    ("services.html", "Matrix Services", SERVICES_CARDS),
    ("matrix.html", "Live Cube Matrix", MATRIX_CARDS),
    ("insights.html", "SubQG Insights", INSIGHTS_CARDS),
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
        self.lib.i18n_get_physics_json.argtypes = (
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_char_p),
            ctypes.c_int,
            ctypes.c_char_p,
            ctypes.c_int,
        )
        self.lib.i18n_get_physics_json.restype = ctypes.c_int
        self.lib.i18n_render_to_html.restype = ctypes.c_int
        self.lib.i18n_last_error_copy.argtypes = (ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int)
        self.lib.i18n_last_error_copy.restype = ctypes.c_int

        self.handle = self.lib.i18n_new()
        if not self.handle:
            raise RuntimeError("Failed to initialize I18n engine.")

    def __del__(self):
        if getattr(self, "handle", None):
            self.lib.i18n_free(self.handle)

    def _last_error(self) -> str:
        buf = ctypes.create_string_buffer(1024)
        length = self.lib.i18n_last_error_copy(self.handle, buf, len(buf))
        if length <= 0:
            return ""
        return buf.value.decode("utf-8", errors="ignore")

    def load_catalog(self, path: str) -> None:
        path_bytes = path.encode("utf-8")
        if self.lib.i18n_load_txt_file(self.handle, path_bytes, 1) != 0:
            with open(path, "rb") as fh:
                raw = fh.read()
            if self.lib.i18n_load_txt(self.handle, raw, 1) != 0:
                raise RuntimeError(f"Could not load catalog: {self._last_error()}")

    def render_to_html(self, token: str, args: list[str]) -> str:
        arr = (ctypes.c_char_p * len(args))()
        for i, value in enumerate(args):
            arr[i] = value.encode("utf-8")

        token_bytes = token.encode("utf-8")
        length = self.lib.i18n_render_to_html(self.handle, token_bytes, arr if args else None, len(args), None, 0)
        if length < 0:
            raise RuntimeError(f"Render failed: {self._last_error()}")

        buf = ctypes.create_string_buffer(length + 1)
        if self.lib.i18n_render_to_html(
            self.handle, token_bytes, arr if args else None, len(args), buf, length + 1
        ) < 0:
            raise RuntimeError(f"Render failed: {self._last_error()}")

        return buf.value.decode("utf-8")

    def get_physics_json(self, token: str, args: list[str]) -> str:
        arr = (ctypes.c_char_p * len(args))()
        for i, value in enumerate(args):
            arr[i] = value.encode("utf-8")

        token_bytes = token.encode("utf-8")
        length = self.lib.i18n_get_physics_json(
            self.handle, token_bytes, arr if args else None, len(args), None, 0
        )
        if length < 0:
            raise RuntimeError(f"Could not build physics json: {self._last_error()}")

        buf = ctypes.create_string_buffer(length + 1)
        if self.lib.i18n_get_physics_json(
            self.handle, token_bytes, arr if args else None, len(args), buf, length + 1
        ) < 0:
            raise RuntimeError(f"Could not build physics json: {self._last_error()}")

        return buf.value.decode("utf-8")


def split_style(rendered: str) -> tuple[str, str]:
    rendered = rendered.strip()
    if rendered.startswith("<style>"):
        parts = rendered.split("</style>", 1)
        style = parts[0] + "</style>"
        body = parts[1].strip() if len(parts) > 1 else ""
        return style, body
    return "", rendered


def extract_style_blocks(rendered: str) -> tuple[list[str], str]:
    styles = []
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
    styles = []
    bodies = []
    for template, args in card_defs:
        rendered_card = engine.render_to_html(template, args)
        card_styles, card_markup = extract_style_blocks(rendered_card)
        styles.extend(card_styles)
        bodies.append(card_markup)
    return styles, "".join(bodies)


def build_animation_script(physics_json: str) -> str:
    return (
        "  <script>\n"
        f"    const MyceliaPhysics = {physics_json};\n"
        "    const cubeStyle = MyceliaPhysics['style_cube-ice'] || MyceliaPhysics['style_cube-heavy'];\n"
        "    console.log('Matrix geladen. Physik für Ice-Cube:', cubeStyle);\n\n"
        "    function lerp(min, max, t) {\n"
        "      return min + (max - min) * Math.max(0, Math.min(1, t));\n"
        "    }\n\n"
        "    document.addEventListener('DOMContentLoaded', () => {\n"
        "      const heroIce = document.getElementById('hero-ice');\n"
        "      if (!heroIce) return;\n"
        "      const mass = cubeStyle.mass || 1;\n"
        "      const friction = Math.min(Math.max(cubeStyle.friction || 0.3, 0), 1);\n"
        "      const spacing = cubeStyle.spacing || 1;\n"
        "      function animate(time) {\n"
        "        const period = 1200 + (1 - friction) * 900;\n"
        "        const bounce = Math.sin(time / period) * spacing * 22;\n"
        "        const scale = 1 + Math.abs(Math.sin(time / (period * 0.9))) * mass * 0.03;\n"
        "        const glow = lerp(0.35, 0.65, Math.abs(Math.sin(time / period)));\n"
        "        heroIce.style.transform = `translateY(${bounce}px) scale(${scale})`;\n"
        "        heroIce.style.boxShadow = `0 30px 60px rgba(59,130,246,${glow})`;\n"
        "        requestAnimationFrame(animate);\n"
        "      }\n"
        "      requestAnimationFrame(animate);\n"
        "    });\n"
        "  </script>\n"
    )


def build_document_with_physics(style_block: str, body: str, animation_script: str) -> str:
    return (
        "<!doctype html>\n"
        "<html lang=\"de\">\n"
        "<head>\n"
        "  <meta charset=\"utf-8\">\n"
        "  <title>Mycelia Matrix Live</title>\n"
        f"{style_block}\n"
        f"{animation_script}"
        "</head>\n"
        "<body>\n"
        f"  {body}\n"
        "</body>\n"
        "</html>\n"
    )


def build_page(engine: NativeEngine, hero_title: str, card_defs: list[tuple[str, list[str]]], output_path: Path) -> None:
    card_styles, grid_content = render_card_set(engine, card_defs)
    html_payload = engine.render_to_html("tpl_full-page", [hero_title, grid_content])
    template_styles, body_markup = extract_style_blocks(html_payload)
    style_block = "\n".join(template_styles + card_styles) + EXTRA_STYLE
    physics_json = engine.get_physics_json("tpl_full-page", [hero_title, grid_content])
    animation_script = build_animation_script(physics_json)
    document = build_document_with_physics(style_block, body_markup, animation_script)
    output_path.write_text(document, encoding="utf-8")


def main():
    os.makedirs(OUTPUT_PATH.parent, exist_ok=True)

    engine = NativeEngine(os.path.join(os.getcwd(), DLL_NAME))
    engine.load_catalog(CATALOG_PATH)

    for filename, hero_title, cards in PAGE_DEFINITIONS:
        target_path = OUTPUT_PATH.parent / filename
        build_page(engine, hero_title, cards, target_path)
        print(f"Generated {target_path}")


if __name__ == "__main__":
    main()
