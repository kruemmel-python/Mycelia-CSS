#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <thread>
#include <sys/stat.h>

#include "i18n_api.h"
#include "i18n_engine.h"

struct ColorEntry {
  std::string hex;
  int r = 0;
  int g = 0;
  int b = 0;
};

static int hex_digit(char c) {
  const char normalized = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (normalized >= '0' && normalized <= '9') return normalized - '0';
  if (normalized >= 'a' && normalized <= 'f') return 10 + (normalized - 'a');
  return -1;
}

static std::vector<ColorEntry> collect_hex_colors(const std::string& text) {
  std::vector<ColorEntry> colors;
  std::unordered_set<std::string> seen;
  for (size_t i = 0; i + 7 <= text.size(); ++i) {
    if (text[i] != '#') continue;
    bool valid = true;
    std::array<int, 6> digits{};
    for (size_t j = 0; j < 6; ++j) {
      int value = hex_digit(text[i + 1 + j]);
      if (value < 0) {
        valid = false;
        break;
      }
      digits[j] = value;
    }
    if (!valid) continue;
    std::string hex = text.substr(i, 7);
    if (!seen.insert(hex).second) continue;
    ColorEntry entry;
    entry.hex = hex;
    entry.r = digits[0] * 16 + digits[1];
    entry.g = digits[2] * 16 + digits[3];
    entry.b = digits[4] * 16 + digits[5];
    colors.push_back(entry);
  }
  return colors;
}

static std::string translate_style(void* engine,
                                   const char* token,
                                   const std::vector<const char*>& args) {
  const char** arg_data =
      args.empty() ? nullptr : const_cast<const char**>(args.data());
  int arg_len = static_cast<int>(args.size());
  int required = i18n_translate(engine, token, arg_data, arg_len, nullptr, 0);
  if (required < 0) return {};
  std::string result;
  result.resize(required);
  if (i18n_translate(engine, token, arg_data, arg_len, result.data(), required + 1) < 0) {
    return {};
  }
  return result;
}

static bool load_catalog_force_text(void* engine, const char* path) {
  if (i18n_load_txt_file(engine, path, 1) == 0) return true;

  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  if (contents.empty()) return false;
  return i18n_load_txt(engine, contents.c_str(), 1) == 0;
}

static long long get_last_write_time(const char* filename) {
  struct stat result;
  if (stat(filename, &result) == 0) return static_cast<long long>(result.st_mtime);
  return 0;
}

static void live_reload_loop(void* engine, const char* filename) {
  long long last_time = get_last_write_time(filename);
  std::cout << ">>> Live-Reload aktiv. Warte auf Änderungen an " << filename << "..." << std::endl;

  while (true) {
    long long current_time = get_last_write_time(filename);
    if (current_time > last_time) {
      last_time = current_time;
      std::cout << "\n[!] Änderung erkannt! Lade Katalog neu..." << std::endl;

      if (!load_catalog_force_text(engine, filename)) {
        const char* err = i18n_last_error(engine);
        std::cerr << ">>> Live-Reload fehlgeschlagen: " << (err ? err : "Unknown error") << std::endl;
      } else {
        std::cout << ">>> Katalog neu geladen." << std::endl;
      }

      I18nNativeStyle ice{};
      if (i18n_get_native_style(engine, "style_cube-ice", nullptr, 0, &ice) == 0) {
        std::cout << ">>> NEUE WERTE (Ice): Mass: " << ice.mass << " | Friction: " << ice.friction << std::endl;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

static void print_color_palette(const std::vector<ColorEntry>& colors) {
  if (colors.empty()) {
    std::cout << "    (No explicit hex colors found)\n";
    return;
  }

  for (const auto& entry : colors) {
    std::cout << "    ";
    std::cout << "\x1b[48;2;" << entry.r << ";" << entry.g << ";" << entry.b << "m"
              << "   " << "\x1b[0m" << ' ';
    std::cout << entry.hex;
    std::cout << " (RGB " << entry.r << ',' << entry.g << ',' << entry.b << ")\n";
  }
}

int main() {
  void* engine = i18n_create();
  if (!engine) {
    std::cerr << "Fehler: Engine konnte nicht erstellt werden." << std::endl;
    return 1;
  }

  if (!load_catalog_force_text(engine, "tailwind_style_catalog.i18n")) {
    std::cerr << "Fehler: Katalog konnte nicht geladen werden.";
    const char* err = i18n_last_error(engine);
    if (err && *err) std::cerr << " (" << err << ")";
    std::cerr << std::endl;
    i18n_destroy(engine);
    return 1;
  }

  const char* tokens[] = { "style_cube-heavy", "style_cube-ice" };
  std::vector<const char*> color_args = { "#0f172a", "#e2e8f0", "#38bdf8", "#fb7185" };

  std::cout << "--- Mycelia Matrix: Physical Style Resolution ---" << std::endl;
  std::cout << std::left << std::setw(22) << "Token"
            << std::setw(10) << "Mass"
            << std::setw(12) << "Friction"
            << std::setw(10) << "Spacing" << std::endl;
  std::cout << std::string(56, '-') << std::endl;

  for (const char* token : tokens) {
    I18nNativeStyle phys{};
    if (i18n_get_native_style(engine,
                              token,
                              color_args.empty() ? nullptr : const_cast<const char**>(color_args.data()),
                              static_cast<int>(color_args.size()),
                              &phys) != 0) {
      std::cerr << "Fehler: Style " << token << " nicht gefunden!" << std::endl;
      continue;
    }

    std::string style = translate_style(engine, token, color_args);
    auto colors = collect_hex_colors(style);

    std::cout << std::left << std::setw(22) << token
              << std::setw(10) << phys.mass
              << std::setw(12) << phys.friction
              << std::setw(10) << phys.spacing << std::endl;
    std::cout << "    Resulting style string: " << (style.empty() ? "(empty)" : style) << std::endl;
    print_color_palette(colors);
    std::cout << std::endl;
  }

  std::cout << "\nDrücke Strg+C, um den Live-Reload zu beenden." << std::endl;
  live_reload_loop(engine, "tailwind_style_catalog.i18n");
  i18n_destroy(engine);
  return 0;
}
