import ctypes
import os
import platform
import sys


class I18nEngine:
    class NativeStyle(ctypes.Structure):
        _fields_ = [
            ("mass", ctypes.c_float),
            ("friction", ctypes.c_float),
            ("restitution", ctypes.c_float),
            ("drag", ctypes.c_float),
            ("gravity_scale", ctypes.c_float),
            ("spacing", ctypes.c_float),
            ("has_values", ctypes.c_int),
        ]

    def __init__(self, library_path):
        if not os.path.isfile(library_path):
            raise FileNotFoundError(f"{library_path} not found")
        self.lib = ctypes.CDLL(library_path)
        self.lib.i18n_new.restype = ctypes.c_void_p
        self.lib.i18n_new.argtypes = ()
        self.lib.i18n_free.argtypes = (ctypes.c_void_p,)
        self.lib.i18n_load_txt_file.argtypes = (ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int)
        self.lib.i18n_translate.argtypes = (ctypes.c_void_p,
                                             ctypes.c_char_p,
                                             ctypes.POINTER(ctypes.c_char_p),
                                             ctypes.c_int,
                                             ctypes.c_char_p,
                                             ctypes.c_int)
        self.lib.i18n_get_native_style.argtypes = (ctypes.c_void_p,
                                                    ctypes.c_char_p,
                                                    ctypes.POINTER(ctypes.c_char_p),
                                                    ctypes.c_int,
                                                    ctypes.POINTER(self.NativeStyle))
        self.lib.i18n_get_native_style.restype = ctypes.c_int
        self.lib.i18n_translate.restype = ctypes.c_int
        self.lib.i18n_export_binary.argtypes = (ctypes.c_void_p, ctypes.c_char_p)
        self.lib.i18n_last_error_copy.argtypes = (ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int)
        self.lib.i18n_last_error_copy.restype = ctypes.c_int
        self.lib.i18n_load_txt.argtypes = (ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int)
        self.lib.i18n_load_txt.restype = ctypes.c_int
        self.handle = self.lib.i18n_new()
        if not self.handle:
            raise RuntimeError("Failed to create engine")

    def __del__(self):
        if getattr(self, "handle", None):
            self.lib.i18n_free(self.handle)
            self.handle = None

    def _last_error(self):
        buf = ctypes.create_string_buffer(1024)
        length = self.lib.i18n_last_error_copy(self.handle, buf, len(buf))
        return buf.value.decode("utf-8", errors="ignore") if length > 0 else ""

    def load_file(self, path, strict=True):
        with open(path, "rb") as fh:
            data = fh.read()
        if self.lib.i18n_load_txt(self.handle, data, 1 if strict else 0) != 0:
            raise RuntimeError(f"Could not load catalog: {self._last_error()}")

    def translate(self, token, args=None):
        args = args or []
        arg_array = (ctypes.c_char_p * len(args))()
        for i, value in enumerate(args):
            arg_array[i] = value.encode("utf-8")

        token_bytes = token.encode("utf-8")
        length = self.lib.i18n_translate(self.handle, token_bytes, arg_array if args else None, len(args), None, 0)
        if length < 0:
            raise RuntimeError("translate failed")
        buf = ctypes.create_string_buffer(length + 1)
        self.lib.i18n_translate(self.handle, token_bytes, arg_array if args else None, len(args), buf, length + 1)
        return buf.value.decode("utf-8")

    def export_binary(self, path):
        self.lib.i18n_export_binary(self.handle, os.path.abspath(path).encode("utf-8"))

    def get_native_style(self, token, args=None):
        token_bytes = token.encode("utf-8")
        native = self.NativeStyle()
        args = args or []
        arg_array = (ctypes.c_char_p * len(args))()
        for i, value in enumerate(args):
            arg_array[i] = value.encode("utf-8")
        result = self.lib.i18n_get_native_style(
            self.handle,
            token_bytes,
            arg_array if args else None,
            len(args),
            ctypes.byref(native),
        )
        if result != 0:
            raise RuntimeError("native style lookup failed")
        return native


def main():
    lib_name = "i18n_engine.dll" if platform.system() == "Windows" else "libi18n_engine.so"
    engine = I18nEngine(os.path.join(os.getcwd(), lib_name))

    catalog_path = os.path.join(os.getcwd(), "tailwind_style_catalog.i18n")
    engine.load_file(catalog_path)

    card_style = engine.translate("style_card", ["#3b82f6", "#ffffff"])
    assert "border: 1px solid #3b82f6" in card_style, "Card border color not injected"
    assert "background-color: #ffffff" in card_style, "Card background not injected"
    assert "box-shadow" in card_style, "Card shadow definition missing"

    hover_style = engine.translate("style_card{hover}", ["#3b82f6", "#ffffff"])
    assert "transform: translateY(-4px)" in hover_style, "Hover transformation missing"

    native = engine.get_native_style("style_cube-heavy")
    assert native.has_values != 0, "Native style missing physical values"
    assert abs(native.mass - 4.2) < 0.01, "Cube mass mismatch"
    assert native.friction > 0.9, "Cube friction mismatch"
    assert abs(native.spacing - 1.5) < 0.01, "Cube spacing mismatch"

    bounce_style = engine.translate("style_cube-bounce")
    assert "box-shadow: 0 10px 30px rgba(59,130,246,0.6)" in bounce_style, "Bounce shadow missing"
    assert "--restitution: 0.95" in bounce_style, "Bounce restitution override missing"

    image_card = engine.translate("tpl_image-card", ["assets/placeholder.svg", "Live Card", "<p>Content</p>"])
    assert "background-image" in image_card, "Image card missing background-image"

    engine.export_binary("final_vision_v1.1.bin")

    print("QA PASS: style translation and native physics are valid.")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as err:
        print(f"QA FAILURE: {err}", file=sys.stderr)
        sys.exit(1)
    except Exception as exc:
        print(f"QA ERROR: {exc}", file=sys.stderr)
        sys.exit(2)
