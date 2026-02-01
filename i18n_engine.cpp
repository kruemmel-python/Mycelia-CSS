#include "i18n_engine.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cctype>
#include <functional>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <cerrno>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
constexpr char BINARY_MAGIC[4] = { 'I', '1', '8', 'N' };
constexpr uint8_t BINARY_VERSION_V1 = 1;
constexpr uint8_t BINARY_VERSION_CURRENT = 2;
constexpr uint8_t BINARY_VERSION = BINARY_VERSION_CURRENT;
constexpr size_t BINARY_HEADER_SIZE_V1 = 20;
constexpr size_t BINARY_HEADER_SIZE_V2 = 24;
constexpr size_t BINARY_HEADER_SIZE = BINARY_HEADER_SIZE_V2;
constexpr size_t METADATA_HEADER_SIZE = 6; // locale_len, fallback_len, note_len

uint16_t read_le_u16(const uint8_t* data) {
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

uint32_t read_le_u32(const uint8_t* data) {
  return (uint32_t)data[0] |
         ((uint32_t)data[1] << 8) |
         ((uint32_t)data[2] << 16) |
         ((uint32_t)data[3] << 24);
}

void append_le_u16(std::vector<uint8_t>& dst, uint16_t value) {
  dst.push_back((uint8_t)(value & 0xFF));
  dst.push_back((uint8_t)((value >> 8) & 0xFF));
}

void append_le_u32(std::vector<uint8_t>& dst, uint32_t value) {
  dst.push_back((uint8_t)(value & 0xFF));
  dst.push_back((uint8_t)((value >> 8) & 0xFF));
  dst.push_back((uint8_t)((value >> 16) & 0xFF));
  dst.push_back((uint8_t)((value >> 24) & 0xFF));
}

uint32_t fnv1a32_append(uint32_t hash, const uint8_t* data, size_t len) {
  uint32_t h = hash;
  for (size_t i = 0; i < len; ++i) {
    h ^= data[i];
    h *= 16777619u;
  }
  return h;
}

struct FileMapping {
  void* data = nullptr;
  size_t size = 0;
#ifdef _WIN32
  HANDLE file_handle = INVALID_HANDLE_VALUE;
  HANDLE mapping_handle = nullptr;
#else
  int fd = -1;
#endif

  ~FileMapping() { unmap(); }

  bool map(const std::filesystem::path& file_path, std::string& err) {
    unmap();
#ifdef _WIN32
    const std::wstring wide_path = file_path.wstring();
    file_handle = CreateFileW(wide_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_handle == INVALID_HANDLE_VALUE) {
      err = "Datei konnte nicht geöffnet werden.";
      return false;
    }
    LARGE_INTEGER size_li;
    if (!GetFileSizeEx(file_handle, &size_li)) {
      err = "Dateigröße konnte nicht ermittelt werden.";
      unmap();
      return false;
    }
    if (size_li.QuadPart <= 0) {
      err = "Datei ist leer.";
      unmap();
      return false;
    }
    mapping_handle = CreateFileMappingW(file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping_handle) {
      err = "Datei-Mapping fehlgeschlagen.";
      unmap();
      return false;
    }
    data = MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, 0);
    if (!data) {
      err = "MapViewOfFile fehlgeschlagen.";
      unmap();
      return false;
    }
    size = (size_t)size_li.QuadPart;
#else
    const std::string native_path = file_path.string();
    fd = open(native_path.c_str(), O_RDONLY);
    if (fd < 0) {
      err = "Datei konnte nicht geöffnet werden.";
      return false;
    }
    struct stat st {};
    if (fstat(fd, &st) != 0) {
      err = "Dateigröße konnte nicht ermittelt werden.";
      unmap();
      return false;
    }
    if (st.st_size <= 0) {
      err = "Datei ist leer.";
      unmap();
      return false;
    }
    size = (size_t)st.st_size;
    data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
      data = nullptr;
      err = "Datei-Mapping fehlgeschlagen.";
      unmap();
      return false;
    }
    close(fd);
    fd = -1;
#endif
    return true;
  }

  void unmap() {
#ifdef _WIN32
    if (data) {
      UnmapViewOfFile(data);
      data = nullptr;
    }
    if (mapping_handle) {
      CloseHandle(mapping_handle);
      mapping_handle = nullptr;
    }
    if (file_handle != INVALID_HANDLE_VALUE) {
      CloseHandle(file_handle);
      file_handle = INVALID_HANDLE_VALUE;
    }
#else
    if (data && size > 0) {
      munmap(data, size);
      data = nullptr;
    }
    if (fd >= 0) {
      close(fd);
      fd = -1;
    }
#endif
    size = 0;
  }
};
} // namespace

void set_engine_error(I18nEngine* eng, const std::string& msg) {
  if (eng) eng->set_last_error(msg);
}

void clear_engine_error(I18nEngine* eng) {
  if (eng) eng->clear_last_error();
}

bool I18nEngine::is_ws(unsigned char c) noexcept { return std::isspace(c) != 0; }
bool I18nEngine::is_digit(unsigned char c) noexcept { return std::isdigit(c) != 0; }
bool I18nEngine::is_xdigit(unsigned char c) noexcept { return std::isxdigit(c) != 0; }
bool I18nEngine::is_digit_uc(char c) noexcept { return std::isdigit((unsigned char)c) != 0; }
bool I18nEngine::is_xdigit_uc(char c) noexcept { return std::isxdigit((unsigned char)c) != 0; }

void I18nEngine::trim_inplace(std::string& s) {
  auto isspace_uc = [](unsigned char c) { return std::isspace(c) != 0; };

  auto it1 = std::find_if_not(s.begin(), s.end(),
                              [&](char ch){ return isspace_uc((unsigned char)ch); });
  auto it2 = std::find_if_not(s.rbegin(), s.rend(),
                              [&](char ch){ return isspace_uc((unsigned char)ch); }).base();

  if (it1 >= it2) { s.clear(); return; }
  s.assign(it1, it2);
}

bool I18nEngine::is_hex_token(const std::string& s) {
  if (s.size() < 6 || s.size() > 32) return false;
  for (unsigned char c : s) if (!is_xdigit(c)) return false;
  return true;
}

void I18nEngine::strip_utf8_bom(std::string& s) {
  if (s.size() >= 3 &&
      (unsigned char)s[0] == 0xEF &&
      (unsigned char)s[1] == 0xBB &&
      (unsigned char)s[2] == 0xBF) {
    s.erase(0, 3);
  }
}

std::string I18nEngine::to_lower_ascii(std::string s) {
  for (char& c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}

std::string I18nEngine::unescape_txt_min(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      char c = s[i + 1];
      switch (c) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case '\\': out += '\\'; break;
        case ':': out += ':'; break;
        default: out += c; break;
      }
      ++i;
    } else {
      out += s[i];
    }
  }
  return out;
}

bool I18nEngine::parse_line(const std::string& line_in,
                            std::string& out_token,
                            std::string& out_label,
                            std::string& out_text,
                            std::string& out_err) {
  out_err.clear();
  out_token.clear();
  out_label.clear();
  out_text.clear();

  std::string line = line_in;
  trim_inplace(line);
  if (line.empty()) return false;
  if (line[0] == '#') return false;

  const auto colon = line.find(':');
  if (colon == std::string::npos) {
    out_err = "Kein ':' gefunden.";
    return false;
  }

  std::string head = line.substr(0, colon);
  std::string text = line.substr(colon + 1);
  trim_inplace(head);
  while (!text.empty() && is_ws((unsigned char)text.front())) text.erase(text.begin());

  std::string token;
  std::string label;

  const auto paren_open = head.find('(');
  if (paren_open == std::string::npos) {
    token = head;
  } else {
    token = head.substr(0, paren_open);
    trim_inplace(token);

    const auto paren_close = head.find(')', paren_open + 1);
    if (paren_close == std::string::npos) {
      out_err = "Label '(' ohne schließende ')'.";
      return false;
    }

    label = head.substr(paren_open + 1, paren_close - (paren_open + 1));
    trim_inplace(label);
  }

  std::string base_token;
  std::string variant_token;

  if (token.find('{') != std::string::npos) {
    if (!parse_variant_suffix(token, base_token, variant_token)) {
      out_err = "Token-Variante ist ungültig.";
      return false;
    }
    token = base_token + '{' + variant_token + '}';
  } else {
    base_token = to_lower_ascii(token);
    token = base_token;
  }

  if (!is_hex_token(base_token) && !is_style_token(base_token)) {
    out_err = "Token ist kein gültiger Hex-String (6–32 Zeichen).";
    return false;
  }

  out_token = std::move(token);
  out_label = std::move(label);
  out_text  = unescape_txt_min(text);
  return true;
}

bool I18nEngine::starts_with(const std::string& s, const char* pref) {
  return s.rfind(pref, 0) == 0;
}

bool I18nEngine::parse_meta_line(const std::string& line, std::string& key, std::string& value) {
  key.clear();
  value.clear();

  std::string s = line;
  trim_inplace(s);
  if (!starts_with(s, "@meta")) return false;

  s.erase(0, 5);
  trim_inplace(s);
  if (s.empty()) return false;

  const auto eq = s.find('=');
  if (eq == std::string::npos) return false;

  key = s.substr(0, eq);
  value = s.substr(eq + 1);
  trim_inplace(key);
  trim_inplace(value);
  key = to_lower_ascii(key);

  return !key.empty() && !value.empty();
}

I18nEngine::PluralRule I18nEngine::parse_plural_rule_name(std::string v, bool& ok) {
  ok = true;
  v = to_lower_ascii(std::move(v));
  if (v == "default") return PluralRule::DEFAULT;
  if (v == "slavic")  return PluralRule::SLAVIC;
  if (v == "arabic")  return PluralRule::ARABIC;
  ok = false;
  return PluralRule::DEFAULT;
}

bool I18nEngine::try_parse_inline_token(const std::string& s, size_t at_pos,
                                        std::string& out_token, size_t& out_advance) {
  out_token.clear();
  out_advance = 1;

  if (at_pos >= s.size() || s[at_pos] != '@') return false;
  if (at_pos + 1 >= s.size()) return false;

  // Escape: @@ -> literal '@'
  if (s[at_pos + 1] == '@') {
    out_advance = 2;
    return false;
  }

  const size_t BASE_POS = at_pos + 1;
  static constexpr char STYLE_PREFIX[] = "style_";
  static constexpr size_t STYLE_PREFIX_LEN = sizeof(STYLE_PREFIX) - 1;

  auto parse_style_inline = [&]() -> bool {
    if (BASE_POS + STYLE_PREFIX_LEN >= s.size()) return false;
    for (size_t i = 0; i < STYLE_PREFIX_LEN; ++i) {
      if (std::tolower((unsigned char)s[BASE_POS + i]) != STYLE_PREFIX[i]) return false;
    }
    size_t pos = BASE_POS + STYLE_PREFIX_LEN;
    while (pos < s.size() && (std::isalnum((unsigned char)s[pos]) || s[pos] == '_' || s[pos] == '-')) {
      if (pos - BASE_POS > 64) return false;
      ++pos;
    }

    std::string token = s.substr(BASE_POS, pos - BASE_POS);

    if (pos < s.size() && s[pos] == '{') {
      size_t brace_start = pos + 1;
      std::string variant;
      while (brace_start < s.size() && s[brace_start] != '}') {
        variant += std::tolower((unsigned char)s[brace_start]);
        ++brace_start;
        if (variant.size() > 16) return false;
      }
      if (brace_start >= s.size() || s[brace_start] != '}' || variant.empty()) return false;
      if (!is_variant_valid(variant)) return false;
      token += '{';
      token += variant;
      token += '}';
      pos = brace_start + 1;
    }

    if (token.size() <= STYLE_PREFIX_LEN) return false;
    out_token = token;
    out_advance = pos - at_pos;
    return true;
  };

  if (parse_style_inline()) {
    return true;
  }

  // Token: @ + 6..32 hex
  size_t j = at_pos + 1;
  size_t n = 0;
  while (j < s.size() && n < 32 && is_xdigit_uc(s[j])) { ++j; ++n; }
  if (n < 6) return false;

  out_token = s.substr(at_pos + 1, n);
  for (char& c : out_token) c = (char)std::tolower((unsigned char)c);

  size_t advance = 1 + n;
  if (j < s.size() && s[j] == '{') {
    size_t k = j + 1;
    std::string variant;
    while (k < s.size() && s[k] != '}') {
      variant += s[k];
      ++k;
      if (variant.size() > 16) break;
    }
    if (k >= s.size() || s[k] != '}' || variant.empty()) return false;
    for (char& c : variant) c = (char)std::tolower((unsigned char)c);
    if (!is_variant_valid(variant)) return false;
    out_token += '{';
    out_token += variant;
    out_token += '}';
    advance = (k + 1) - at_pos;
  }

  out_advance = advance;
  return true;
}

void I18nEngine::scan_inline_refs(const std::string& text, std::vector<std::string>& out_refs) {
  out_refs.clear();
  for (size_t i = 0; i < text.size();) {
    if (text[i] != '@') { ++i; continue; }

    // @@ = escape
    if (i + 1 < text.size() && text[i + 1] == '@') { i += 2; continue; }

    std::string tok;
    size_t adv = 1;
    if (try_parse_inline_token(text, i, tok, adv)) {
      out_refs.push_back(tok);
      i += adv;
      continue;
    }

    ++i; // einzelnes '@'
  }

  std::sort(out_refs.begin(), out_refs.end());
  out_refs.erase(std::unique(out_refs.begin(), out_refs.end()), out_refs.end());
}

std::string I18nEngine::resolve_arg(const CatalogSnapshot* state,
                                    const std::string& arg,
                                    std::unordered_set<std::string>& seen,
                                    int depth) {
  if (!arg.empty() && arg[0] == '=') {
    return arg.substr(1);
  }

  std::string normalized = to_lower_ascii(arg);
  std::string base = normalized;
  std::string variant;
  std::string lookup = normalized;
  if (parse_variant_suffix(normalized, base, variant)) {
    lookup.clear();
    lookup += base;
    lookup += '{';
    lookup += variant;
    lookup += '}';
  } else {
    base = normalized;
  }

  if (!is_hex_token(base)) return arg;

  auto it = state->catalog.find(lookup);
  if (it == state->catalog.end()) return arg;

  return translate_impl(state, lookup, {}, seen, depth + 1);
}

std::string I18nEngine::translate_impl(const CatalogSnapshot* state,
                                       const std::string& token,
                                       const std::vector<std::string>& args,
                                       std::unordered_set<std::string>& seen,
                                       int depth) {
  if (depth > 32) return "⟦RECURSION_LIMIT⟧";
  if (seen.count(token)) return "⟦CYCLE:" + token + "⟧";
  seen.insert(token);

  if (const auto* style_state = dynamic_cast<const StyleCatalogSnapshot*>(state)) {
    std::string style_out;
    if (try_build_style_string(style_state, token, args, seen, depth, style_out)) {
      seen.erase(token);
      return style_out;
    }
  }

  auto it = state->catalog.find(token);
  if (it == state->catalog.end()) {
    seen.erase(token);
    return "⟦" + token + "⟧";
  }

  const std::string& raw = it->second;
  std::string out;
  out.reserve(raw.size() + 32);

  for (size_t i = 0; i < raw.size();) {
    // --- Inline Token Reference: @deadbeef / @@ ---
    if (raw[i] == '@') {
      std::string ref_tok;
      size_t adv = 1;

      if (try_parse_inline_token(raw, i, ref_tok, adv)) {
        // harte Token-Ref: muss im Catalog sein, sonst sichtbarer Marker
        if (state->catalog.find(ref_tok) == state->catalog.end()) out += "⟦MISSING:@" + ref_tok + "⟧";
        else out += translate_impl(state, ref_tok, args, seen, depth + 1);

        i += adv;
        continue;
      }

      // @@ -> literal '@'
      if (i + 1 < raw.size() && raw[i + 1] == '@') {
        out += '@';
        i += 2;
        continue;
      }

      // einzelnes '@' ohne gültigen Token: literal
      out += '@';
      ++i;
      continue;
    }

    // --- Placeholder %N ---
    if (raw[i] == '%' && i + 1 < raw.size() && is_digit((unsigned char)raw[i + 1])) {
      size_t j = i + 1;
      int idx = 0;
      while (j < raw.size() && is_digit((unsigned char)raw[j])) {
        idx = idx * 10 + (raw[j] - '0');
        ++j;
      }

      if (idx >= 0 && (size_t)idx < args.size()) out += resolve_arg(state, args[(size_t)idx], seen, depth);
      else out += "⟦arg:" + std::to_string(idx) + "⟧";

      i = j;
      continue;
    }
    out += raw[i];
    ++i;
  }

  seen.erase(token);
  return out;
}

std::string I18nEngine::resolve_plain_text(const CatalogSnapshot* state,
                                           const std::string& raw,
                                           const std::vector<std::string>& args,
                                           std::unordered_set<std::string>& seen,
                                           int depth) {
  if (depth > 32) return "⟦RECURSION_LIMIT⟧";
  if (!state) return raw;
  std::string out;
  out.reserve(raw.size());

  for (size_t i = 0; i < raw.size();) {
    if (raw[i] == '@') {
      std::string ref_tok;
      size_t adv = 1;
      if (try_parse_inline_token(raw, i, ref_tok, adv)) {
        if (state->catalog.find(ref_tok) == state->catalog.end()) out += "⟦MISSING:@" + ref_tok + "⟧";
        else out += translate_impl(state, ref_tok, args, seen, depth + 1);
        i += adv;
        continue;
      }
      if (i + 1 < raw.size() && raw[i + 1] == '@') {
        out += '@';
        i += 2;
        continue;
      }
      out += '@';
      ++i;
      continue;
    }

    if (raw[i] == '%' && i + 1 < raw.size() && is_digit((unsigned char)raw[i + 1])) {
      size_t j = i + 1;
      int idx = 0;
      while (j < raw.size() && is_digit((unsigned char)raw[j])) {
        idx = idx * 10 + (raw[j] - '0');
        ++j;
      }
      if (idx >= 0 && (size_t)idx < args.size()) out += resolve_arg(state, args[(size_t)idx], seen, depth + 1);
      else out += "⟦arg:" + std::to_string(idx) + "⟧";
      i = j;
      continue;
    }

    out += raw[i];
    ++i;
  }

  return out;
}

bool I18nEngine::try_build_style_string(const StyleCatalogSnapshot* style_state,
                                        const std::string& token,
                                        const std::vector<std::string>& args,
                                        std::unordered_set<std::string>& seen,
                                        int depth,
                                        std::string& out_style) {
  if (!style_state) return false;
  auto it = style_state->style_registry.find(token);
  if (it == style_state->style_registry.end()) return false;
  if (it->second.empty()) return false;

  std::string builder;
  builder.reserve(it->second.size() * 32);

  for (const auto& prop : it->second) {
    if (prop.prop_name.empty()) {
      std::string resolved = resolve_plain_text(style_state, prop.value, args, seen, depth + 1);
      if (!resolved.empty()) {
        if (!builder.empty() && builder.back() != ' ') builder += ' ';
        builder += resolved;
        builder += ' ';
      }
      continue;
    }
    std::string resolved = resolve_plain_text(style_state, prop.value, args, seen, depth + 1);
    if (!builder.empty() && builder.back() != ' ') builder += ' ';
    builder += prop.prop_name;
    builder += ": ";
    builder += resolved;
    builder += ";";
  }

  while (!builder.empty() && builder.back() == ' ') builder.pop_back();

  if (builder.empty()) return false;
  out_style = std::move(builder);
  return true;
}

std::string I18nEngine::read_file_utf8(const char* path, std::string& err) {
  err.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f) { err = "Datei konnte nicht geöffnet werden."; return {}; }
  std::ostringstream ss;
  ss << f.rdbuf();
  if (!f.good() && !f.eof()) { err = "Fehler beim Lesen der Datei."; return {}; }
  return ss.str();
}

const char* I18nEngine::get_last_error() const noexcept { return last_error.c_str(); }

const std::string& I18nEngine::get_meta_note() const noexcept { return meta_note; }

void I18nEngine::set_last_error(std::string msg) { last_error = msg; }

void I18nEngine::clear_last_error() { last_error.clear(); }

const std::string& I18nEngine::get_meta_locale() const noexcept { return meta_locale; }
const std::string& I18nEngine::get_meta_fallback() const noexcept { return meta_fallback; }
I18nEngine::PublicPluralRule I18nEngine::get_meta_plural_rule() const noexcept { return static_cast<PublicPluralRule>(meta_plural); }

I18nEngine::NativeStyle I18nEngine::get_native_style(const std::string& style_token, const std::vector<std::string>& args) {
  auto snapshot = acquire_snapshot();
  if (!snapshot) return {};
  auto style_snapshot = std::dynamic_pointer_cast<const StyleCatalogSnapshot>(snapshot);
  if (!style_snapshot) return {};
  std::string normalized = to_lower_ascii(style_token);
  auto it = style_snapshot->style_registry.find(normalized);
  if (it == style_snapshot->style_registry.end()) return {};
  return evaluate_native_style(it->second, style_snapshot.get(), args);
}

bool I18nEngine::load_txt_catalog(std::string src, bool strict) {
  clear_last_error();
  if (src.empty()) { set_last_error("src is empty"); return false; }

  std::shared_ptr<CatalogSnapshot> snapshot;
  std::string err;
  if (looks_like_binary_catalog(src)) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(src.data());
    snapshot = build_snapshot_from_binary(data, src.size(), strict, err);
  } else {
    strip_utf8_bom(src);
    snapshot = build_snapshot_from_text(std::move(src), strict, err);
  }

  if (!snapshot) {
    if (err.empty()) err = "Katalog konnte nicht geladen werden.";
    set_last_error(err);
    return false;
  }

  install_snapshot(snapshot);
  return true;
}

std::shared_ptr<I18nEngine::CatalogSnapshot> I18nEngine::build_snapshot_from_text(std::string&& src, bool strict, std::string& err) {
  err.clear();
  auto snapshot = std::make_shared<StyleCatalogSnapshot>();
  size_t start = 0;
  size_t loaded = 0;
  int line_no = 0;
  bool meta_phase = true;
  bool seen_any_entry = false;

  auto next_line = [&](std::string& out_line) -> bool {
    if (start >= src.size()) return false;
    size_t end = src.find('\n', start);
    if (end == std::string::npos) end = src.size();
    out_line = src.substr(start, end - start);
    if (!out_line.empty() && out_line.back() == '\r') out_line.pop_back();
    start = (end < src.size()) ? end + 1 : src.size();
    return true;
  };

  std::string line;
  while (next_line(line)) {
    ++line_no;
    std::string raw = line;
    trim_inplace(raw);
    if (raw.empty()) continue;
    if (raw[0] == '#') continue;

    if (meta_phase) {
      std::string key, value;
      if (parse_meta_line(raw, key, value)) {
        if (seen_any_entry) {
          if (strict) {
            err = "Meta-Zeile nach Einträgen in Zeile " + std::to_string(line_no);
            return {};
          }
          continue;
        }

        if (key == "locale") {
          snapshot->meta_locale = value;
          continue;
        }
        if (key == "fallback") {
          snapshot->meta_fallback = value;
          continue;
        }
        if (key == "note") {
          snapshot->meta_note = value;
          continue;
        }
        if (key == "plural") {
          bool ok = false;
          snapshot->meta_plural = parse_plural_rule_name(value, ok);
          if (!ok && strict) {
            err = "Unbekannte Plural-Rule '" + value + "' in Zeile " + std::to_string(line_no);
            return {};
          }
          continue;
        }
        if (strict) {
          err = "Unbekannter Meta-Key '" + key + "' in Zeile " + std::to_string(line_no);
          return {};
        }
        continue;
      }
      meta_phase = false;
    }

    std::string token, label, text, parse_err;
    const bool ok = parse_line(line, token, label, text, parse_err);
    if (!ok) {
      if (strict && !parse_err.empty()) {
        err = "Parse-Fehler in Zeile " + std::to_string(line_no) + ": " + parse_err;
        return {};
      }
      continue;
    }

    std::string base_token = token;
    std::string variant_token;
    if (parse_variant_suffix(token, base_token, variant_token)) {
      if (!variant_token.empty()) snapshot->plural_variants[base_token].insert(variant_token);
    }

    if (snapshot->catalog.find(token) != snapshot->catalog.end()) {
      err = "Doppelter Token in Zeile " + std::to_string(line_no) + ": " + token;
      return {};
    }

    snapshot->catalog.emplace(token, std::move(text));
    if (!label.empty()) snapshot->labels.emplace(token, std::move(label));
    ++loaded;
    seen_any_entry = true;
  }

  if (loaded == 0) {
    err = "Kein einziger gültiger Eintrag geladen (leerer Katalog?).";
    return {};
  }

  populate_style_registry(snapshot.get());
  return snapshot;
}

std::shared_ptr<I18nEngine::CatalogSnapshot> I18nEngine::build_snapshot_from_binary(const uint8_t* data, size_t size, bool strict, std::string& err) {
  err.clear();
  if (size < BINARY_HEADER_SIZE_V1) {
    err = "Binär-Format: Header zu kurz.";
    return {};
  }

  if (std::memcmp(data, BINARY_MAGIC, 4) != 0) {
    err = "Unbekanntes Binär-Format.";
    return {};
  }

  const uint8_t version = data[4];
  if (version != BINARY_VERSION_V1 && version != BINARY_VERSION) {
    err = "Binär-Format-Version nicht unterstützt.";
    return {};
  }

  uint8_t plural_rule = 0;
  size_t header_size = (version == BINARY_VERSION_V1) ? BINARY_HEADER_SIZE_V1 : BINARY_HEADER_SIZE_V2;
  uint32_t metadata_size = 0;
  if (version >= BINARY_VERSION_CURRENT) {
    plural_rule = data[6];
    metadata_size = read_le_u32(data + 20);
    if (metadata_size > size - header_size) {
      err = "Binär-Format: Metadata block zu groß.";
      return {};
    }
    if (metadata_size > 0 && metadata_size < METADATA_HEADER_SIZE) {
      err = "Binär-Format: Metadata block zu kurz.";
      return {};
    }
  }

  auto snapshot = std::make_shared<StyleCatalogSnapshot>();
  snapshot->meta_plural = PluralRule::DEFAULT;
  if (plural_rule <= static_cast<uint8_t>(PluralRule::ARABIC)) {
    snapshot->meta_plural = static_cast<PluralRule>(plural_rule);
  }

  const uint32_t entry_count = read_le_u32(data + 8);
  const uint32_t string_table_size = read_le_u32(data + 12);
  const uint32_t checksum = read_le_u32(data + 16);

  size_t metadata_block_offset = header_size;
  if (version >= BINARY_VERSION_CURRENT && metadata_size > 0) {
    if (metadata_block_offset + metadata_size > size) {
      err = "Binär-Format: Metadata block überläuft.";
      return {};
    }

    const uint8_t* meta_ptr = data + metadata_block_offset;
    const uint16_t locale_len = read_le_u16(meta_ptr);
    const uint16_t fallback_len = read_le_u16(meta_ptr + 2);
    const uint16_t note_len = read_le_u16(meta_ptr + 4);
    const size_t expected = METADATA_HEADER_SIZE + locale_len + fallback_len + note_len;
    if (expected != metadata_size) {
      err = "Binär-Format: Metadata-Länge inkonsistent.";
      return {};
    }

    size_t cursor = metadata_block_offset + METADATA_HEADER_SIZE;
    if (locale_len > 0) snapshot->meta_locale.assign(reinterpret_cast<const char*>(data + cursor), locale_len);
    cursor += locale_len;
    if (fallback_len > 0) snapshot->meta_fallback.assign(reinterpret_cast<const char*>(data + cursor), fallback_len);
    cursor += fallback_len;
    if (note_len > 0) snapshot->meta_note.assign(reinterpret_cast<const char*>(data + cursor), note_len);
    cursor += note_len;
  }

  size_t entry_table_offset = metadata_block_offset + metadata_size;
  size_t offset = entry_table_offset;
struct EntryInfo {
    std::string base;
    std::string variant;
    uint32_t text_offset;
    uint32_t text_length;
  };

  std::vector<EntryInfo> entries;
  entries.reserve(entry_count);

  for (uint32_t i = 0; i < entry_count; ++i) {
    if (offset >= size) {
      err = "Binär-Format: Eintragstabelle zu kurz.";
      return {};
    }

    const uint8_t token_len = data[offset++];
    if (token_len < 6 || token_len > 32) {
      err = "Binär-Format: Ungültige Token-Länge.";
      return {};
    }

    if (offset + token_len > size) {
      err = "Binär-Format: Token-Länge überschreitet Daten.";
      return {};
    }

    std::string base(reinterpret_cast<const char*>(data + offset), token_len);
    offset += token_len;
    base = to_lower_ascii(base);

    const uint8_t variant_len = data[offset++];
    std::string variant;
    if (variant_len > 0) {
      if (offset + variant_len > size) {
        err = "Binär-Format: Variant-Länge überschreitet Daten.";
        return {};
      }
      variant.assign(reinterpret_cast<const char*>(data + offset), variant_len);
      offset += variant_len;
      variant = to_lower_ascii(variant);
      if (!is_variant_valid(variant)) {
        err = "Binär-Format: Variant enthält ungültige Zeichen.";
        return {};
      }
    }

    if (!is_hex_token(base)) {
      err = "Binär-Format: Token ist kein Hex-String.";
      return {};
    }

    if (offset + 8 > size) {
      err = "Binär-Format: Eintrag zu kurz.";
      return {};
    }

    const uint32_t text_offset = read_le_u32(data + offset);
    offset += 4;
    const uint32_t text_length = read_le_u32(data + offset);
    offset += 4;

    entries.push_back({ base, variant, text_offset, text_length });
  }

  const size_t strings_base = offset;
  if (strings_base + string_table_size > size) {
    err = "Binär-Format: String-Table zu kurz.";
    return {};
  }

  uint32_t computed_checksum = 0;
  if (version == BINARY_VERSION_V1) {
    computed_checksum = fnv1a32(data + strings_base, string_table_size);
  } else {
    computed_checksum = 2166136261u;
    if (metadata_size > 0) computed_checksum = fnv1a32_append(computed_checksum, data + metadata_block_offset, metadata_size);
    computed_checksum = fnv1a32_append(computed_checksum, data + entry_table_offset, strings_base - entry_table_offset);
    computed_checksum = fnv1a32_append(computed_checksum, data + strings_base, string_table_size);
  }

  if (computed_checksum != checksum && strict) {
    err = "Binär-Format: Checksum stimmt nicht.";
    return {};
  }

  for (const auto& entry : entries) {
    if ((uint64_t)entry.text_offset + entry.text_length > string_table_size) {
      err = "Binär-Format: Text-Offset außerhalb der String-Table.";
      return {};
    }

    const char* text_ptr = reinterpret_cast<const char*>(data + strings_base + entry.text_offset);
    std::string value(text_ptr, entry.text_length);

    std::string key = entry.base;
    if (!entry.variant.empty()) {
      key += '{';
      key += entry.variant;
      key += '}';
      snapshot->plural_variants[entry.base].insert(entry.variant);
    }

    if (snapshot->catalog.find(key) != snapshot->catalog.end()) {
      err = "Binär-Format: Doppelte Einträge.";
      return {};
    }

    snapshot->catalog.emplace(std::move(key), std::move(value));
  }

  if (snapshot->catalog.empty()) {
    err = "Binär-Format: Kein Eintrag enthalten.";
    return {};
  }

  populate_style_registry(snapshot.get());
  return snapshot;
}

void I18nEngine::install_snapshot(std::shared_ptr<CatalogSnapshot> snapshot) {
  if (!snapshot) return;
  meta_locale = snapshot->meta_locale;
  meta_fallback = snapshot->meta_fallback;
  meta_note = snapshot->meta_note;
  meta_plural = snapshot->meta_plural;
  std::atomic_store_explicit(&active_snapshot,
                             std::static_pointer_cast<const CatalogSnapshot>(snapshot),
                             std::memory_order_release);
}

std::shared_ptr<const I18nEngine::CatalogSnapshot> I18nEngine::acquire_snapshot() const noexcept {
  return std::atomic_load_explicit(&active_snapshot, std::memory_order_acquire);
}

bool I18nEngine::is_binary_catalog_path(const std::string& path) noexcept {
  const size_t dot_pos = path.find_last_of('.');
  if (dot_pos == std::string::npos) return false;
  std::string ext = path.substr(dot_pos);
  for (char& c : ext) c = (char)std::tolower((unsigned char)c);
  return ext == ".i18n" || ext == ".bin";
}

bool I18nEngine::load_txt_file(const char* path, bool strict) {
  clear_last_error();
  if (!path) { set_last_error("path == nullptr"); return false; }

  std::string err;
  std::shared_ptr<CatalogSnapshot> snapshot;
  const std::string path_str = path;

  if (is_binary_catalog_path(path_str)) {
    FileMapping mapping;
    if (!mapping.map(std::filesystem::path(path_str), err)) {
      set_last_error(err);
      return false;
    }
    snapshot = build_snapshot_from_binary(reinterpret_cast<const uint8_t*>(mapping.data), mapping.size, strict, err);
    if (!snapshot) {
      set_last_error(err);
      return false;
    }
  } else {
    std::string data = read_file_utf8(path, err);
    if (!err.empty()) { set_last_error(err); return false; }
    strip_utf8_bom(data);
    snapshot = build_snapshot_from_text(std::move(data), strict, err);
    if (!snapshot) {
      set_last_error(err);
      return false;
    }
  }

  current_path = path;
  current_strict = strict;
  install_snapshot(snapshot);
  return true;
}

bool I18nEngine::reload() {
  if (current_path.empty()) { set_last_error("No file loaded yet"); return false; }
  // Nutzt den gespeicherten Pfad und Strict-Mode
  return load_txt_file(current_path.c_str(), current_strict);
}

std::string I18nEngine::translate(const std::string& token_in, const std::vector<std::string>& args) {
  auto snapshot = acquire_snapshot();
  if (!snapshot) return "⟦NO_CATALOG⟧";
  std::string token = to_lower_ascii(token_in);
  std::unordered_set<std::string> seen;
  return translate_impl(snapshot.get(), token, args, seen, 0);
}

std::string I18nEngine::translate_plural(const std::string& token_in,
                                         int count,
                                         const std::vector<std::string>& args) {
  auto snapshot = acquire_snapshot();
  if (!snapshot) return "⟦NO_CATALOG⟧";
  std::string normalized = to_lower_ascii(token_in);
  std::string base;
  std::string variant;
  std::string lookup;
  if (parse_variant_suffix(normalized, base, variant) && !variant.empty()) {
    lookup = base + "{" + variant + "}";
  } else {
    base = normalized;
    const std::string desired = base + "{" + pick_variant_name(meta_plural, count) + "}";
    if (snapshot->catalog.find(desired) != snapshot->catalog.end()) {
      lookup = desired;
    } else if (snapshot->catalog.find(base + "{other}") != snapshot->catalog.end()) {
      lookup = base + "{other}";
    } else {
      const auto it = snapshot->plural_variants.find(base);
      if (it != snapshot->plural_variants.end() && !it->second.empty()) {
        lookup = base + '{' + *it->second.begin() + '}';
      } else {
        lookup = base;
      }
    }
  }

  std::unordered_set<std::string> seen;
  return translate_impl(snapshot.get(), lookup, args, seen, 0);
}

std::string I18nEngine::dump_table() const {
  auto snapshot = acquire_snapshot();
  if (!snapshot) return "Catalog not loaded\n";
  const auto& catalog = snapshot->catalog;
  const auto& labels = snapshot->labels;

  std::string out;
  out.reserve(catalog.size() * 64);
  out += "Token        | Label                  | Inhalt\n";
  out += "------------------------------------------------------------\n";

  std::vector<std::string> keys;
  keys.reserve(catalog.size());
  for (const auto& kv : catalog) keys.push_back(kv.first);
  std::sort(keys.begin(), keys.end());

  for (const auto& token : keys) {
    const std::string& text = catalog.at(token);
    auto itL = labels.find(token);
    const std::string label = (itL != labels.end()) ? itL->second : "";

    out += token;
    if (token.size() < 12) out.append(12 - token.size(), ' ');
    out += " | ";

    out += label;
    if (label.size() < 22) out.append(22 - label.size(), ' ');
    out += " | ";

    out += text;
    out += "\n";
  }

  return out;
}

std::string I18nEngine::find_any(const std::string& query) const {
  std::string q = query;
  for (char& c : q) c = (char)std::tolower((unsigned char)c);

  std::string out;

  // Determinismus: Sortiere Keys
  auto snapshot = acquire_snapshot();
  if (!snapshot) return "(no catalog loaded)\n";
  const auto& catalog = snapshot->catalog;
  const auto& labels = snapshot->labels;

  std::vector<std::string> keys;
  keys.reserve(catalog.size());
  for (const auto& kv : catalog) keys.push_back(kv.first);
  std::sort(keys.begin(), keys.end());

  for (const auto& token : keys) {
    const std::string& text = catalog.at(token);

    std::string t = text;
    for (char& c : t) c = (char)std::tolower((unsigned char)c);

    std::string lbl;
    auto itL = labels.find(token);
    if (itL != labels.end()) lbl = itL->second;
    std::string l = lbl;
    for (char& c : l) c = (char)std::tolower((unsigned char)c);

    if (t.find(q) != std::string::npos || (!l.empty() && l.find(q) != std::string::npos)) {
      out += token;
      out += "(";
      out += lbl;
      out += "): ";
      out += text;
      out += "\n";
    }
  }

  if (out.empty()) out = "(keine Treffer)\n";
  return out;
}

std::string I18nEngine::check_catalog_report(int& out_code) const {
  out_code = 0;

  auto snapshot = acquire_snapshot();
  if (!snapshot) {
    out_code = 2;
    return "CHECK: FAIL\nGrund: Katalog ist leer oder nicht geladen.\n";
  }

  const auto& catalog = snapshot->catalog;

  if (catalog.empty()) {
    out_code = 2;
    return "CHECK: FAIL\nGrund: Katalog ist leer oder nicht geladen.\n";
  }

  size_t warnings = 0;
  size_t errors = 0;

  std::string report;
  report.reserve(catalog.size() * 96);
  report += "CHECK: REPORT\n";
  report += "------------------------------\n";

  auto scan_placeholders = [](const std::string& s, std::vector<int>& idxs) -> bool {
    idxs.clear();
    for (size_t i = 0; i < s.size();) {
      if (s[i] == '%' && i + 1 < s.size() && std::isdigit((unsigned char)s[i + 1])) {
        size_t j = i + 1;
        int idx = 0;
        while (j < s.size() && std::isdigit((unsigned char)s[j])) {
          if (idx > 1000000) idx = 1000000;
          else idx = idx * 10 + (s[j] - '0');
          ++j;
        }
        idxs.push_back(idx);
        i = j;
        continue;
      }
      ++i;
    }
    if (idxs.empty()) return false;
    std::sort(idxs.begin(), idxs.end());
    idxs.erase(std::unique(idxs.begin(), idxs.end()), idxs.end());
    return true;
  };

  std::vector<int> idxs;
  std::vector<std::string> refs;
  std::unordered_map<std::string, std::vector<std::string>> edges;
  edges.reserve(catalog.size());

  for (const auto& kv : catalog) {
    const std::string& token = kv.first;
    const std::string& text  = kv.second;

    if (scan_placeholders(text, idxs)) {
      bool gap = false;
      int expect = 0;
      for (int got : idxs) { if (got != expect) { gap = true; break; } ++expect; }
      if (gap) {
        ++warnings;
        report += "WARN "; report += token;
        report += ": Placeholder-Lücke. Gefunden: ";
        for (size_t i = 0; i < idxs.size(); ++i) {
          report += "%"; report += std::to_string(idxs[i]);
          if (i + 1 < idxs.size()) report += ", ";
        }
        report += "\n";
      }
    }

    scan_inline_refs(text, refs);
    if (!refs.empty()) edges.emplace(token, refs);

    for (const auto& r : refs) {
      if (catalog.find(r) == catalog.end()) {
        ++errors;
        report += "ERROR "; report += token;
        report += ": Missing inline ref @"; report += r;
        report += "\n";
      }
    }
  }

  enum class Color : unsigned char { White, Gray, Black };
  std::unordered_map<std::string, Color> color;
  color.reserve(catalog.size());
  for (const auto& kv : catalog) color.emplace(kv.first, Color::White);

  std::vector<std::string> stack;
  stack.reserve(64);

  auto dump_cycle = [&](const std::string& start) {
    auto it = std::find(stack.begin(), stack.end(), start);
    report += "ERROR CYCLE: ";
    if (it == stack.end()) { report += start; report += "\n"; return; }
    for (; it != stack.end(); ++it) {
      report += *it;
      report += " -> ";
    }
    report += start;
    report += "\n";
  };

  std::function<void(const std::string&)> dfs = [&](const std::string& u) {
    color[u] = Color::Gray;
    stack.push_back(u);

    auto itE = edges.find(u);
    if (itE != edges.end()) {
      for (const auto& v : itE->second) {
        if (catalog.find(v) == catalog.end()) continue;
        auto cv = color[v];
        if (cv == Color::White) dfs(v);
        else if (cv == Color::Gray) { ++errors; dump_cycle(v); }
      }
    }

    stack.pop_back();
    color[u] = Color::Black;
  };

  for (const auto& kv : catalog) {
    const auto& tok = kv.first;
    if (color[tok] == Color::White) dfs(tok);
  }

  report += "------------------------------\n";
  report += "Tokens: "; report += std::to_string(catalog.size()); report += "\n";
  report += "Warnings: "; report += std::to_string(warnings); report += "\n";
  report += "Errors: "; report += std::to_string(errors); report += "\n";

  if (errors > 0) {
    report += "CHECK: FAIL\n";
    out_code = 3;
  } else if (warnings > 0) {
    report += "CHECK: OK (mit Warnungen)\n";
    out_code = 0;
  } else {
    report += "CHECK: OK\n";
    out_code = 0;
  }

  return report;
}

bool I18nEngine::looks_like_binary_catalog(const std::string& data) noexcept {
  if (data.size() < BINARY_HEADER_SIZE_V1) return false;
  if (std::memcmp(data.data(), BINARY_MAGIC, 4) != 0) return false;
  const uint8_t version = static_cast<uint8_t>(data[4]);
  return version == BINARY_VERSION_V1 || version == BINARY_VERSION;
}

bool I18nEngine::parse_variant_suffix(const std::string& token,
                                      std::string& out_base,
                                      std::string& out_variant) {
  const size_t open = token.find('{');
  const size_t close = (open != std::string::npos) ? token.find('}', open + 1) : std::string::npos;
  if (open == std::string::npos || close == std::string::npos || close != token.size() - 1) {
    return false;
  }

  out_base = token.substr(0, open);
  out_variant = token.substr(open + 1, close - open - 1);
  if (out_variant.empty()) return false;

  out_base = to_lower_ascii(out_base);
  out_variant = to_lower_ascii(out_variant);

  return is_variant_valid(out_variant) && !out_base.empty();
}

const char* I18nEngine::pick_variant_name(PluralRule rule, int count) noexcept {
  if (count < 0) return "other";

  switch (rule) {
    case PluralRule::DEFAULT:
      if (count == 0) return "zero";
      if (count == 1) return "one";
      return "other";

    case PluralRule::SLAVIC: {
      const int mod10 = count % 10;
      const int mod100 = count % 100;
      if (mod10 == 1 && mod100 != 11) return "one";
      if (mod10 >= 2 && mod10 <= 4 && !(mod100 >= 12 && mod100 <= 14)) return "few";
      if (mod10 == 0 || (mod10 >= 5 && mod10 <= 9) || (mod100 >= 11 && mod100 <= 14)) return "many";
      return "other";
    }

    case PluralRule::ARABIC: {
      const int mod100 = count % 100;
      if (count == 0) return "zero";
      if (count == 1) return "one";
      if (count == 2) return "two";
      if (mod100 >= 3 && mod100 <= 10) return "few";
      if (mod100 >= 11 && mod100 <= 99) return "many";
      return "other";
    }
  }
  return "other";
}

bool I18nEngine::is_variant_valid(const std::string& variant) noexcept {
  if (variant.empty() || variant.size() > 16) return false;
  for (char c : variant) {
    const unsigned char uc = (unsigned char)c;
    if (!(std::islower(uc) || std::isdigit(uc) || c == '_' || c == '-')) return false;
  }
  return true;
}

bool I18nEngine::is_style_token(const std::string& token) noexcept {
  std::string base = token;
  std::string variant;
  if (!parse_variant_suffix(token, base, variant)) {
    base = token;
  }

  const char* STYLE_PREFIX = "style_";
  static constexpr size_t STYLE_PREFIX_LEN = 6;
  if (base.size() <= STYLE_PREFIX_LEN) return false;
  return starts_with(base, STYLE_PREFIX);
}

bool I18nEngine::parse_style_properties(const std::string& text, std::vector<StyleProperty>& out_props) {
  out_props.clear();
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find(';', pos);
    std::string segment = text.substr(pos, (end == std::string::npos) ? text.size() - pos : end - pos);
    pos = (end == std::string::npos) ? text.size() : end + 1;
    trim_inplace(segment);
    if (segment.empty()) continue;
    const size_t colon = segment.find(':');
    if (colon == std::string::npos) {
      if (segment[0] == '@') {
        StyleProperty prop;
        prop.prop_hash = 0;
        prop.prop_name.clear();
        prop.value = segment;
        out_props.push_back(std::move(prop));
      }
      continue;
    }

    std::string name = segment.substr(0, colon);
    std::string value = segment.substr(colon + 1);
    trim_inplace(name);
    trim_inplace(value);
    if (name.empty() || value.empty()) continue;

    StyleProperty prop;
    prop.prop_name = to_lower_ascii(std::move(name));
    prop.value = std::move(value);
    prop.prop_hash = fnv1a32(reinterpret_cast<const uint8_t*>(prop.prop_name.data()), prop.prop_name.size());
    out_props.push_back(std::move(prop));
  }
  return !out_props.empty();
}

bool I18nEngine::parse_physical_value(const std::string& text, float& out_value) {
  if (text.empty()) return false;
  errno = 0;
  char* end = nullptr;
  const float value = std::strtof(text.c_str(), &end);
  if (end == text.c_str() || errno == ERANGE) return false;
  out_value = value;
  return true;
}

void I18nEngine::apply_physical_property(NativeStyle& style, const std::string& key, const std::string& raw_value) const {
  if (key.empty()) return;
  std::string name = key;
  if (starts_with(name, "--")) name.erase(0, 2);
  float value = 0.0f;
  if (!parse_physical_value(raw_value, value)) return;

  if (name == "mass") { style.mass = value; style.has_physical = true; }
  else if (name == "friction") { style.friction = value; style.has_physical = true; }
  else if (name == "restitution") { style.restitution = value; style.has_physical = true; }
  else if (name == "drag") { style.drag = value; style.has_physical = true; }
  else if (name == "gravity-scale" || name == "gravity_scale") { style.gravity_scale = value; style.has_physical = true; }
  else if (name == "spacing" || name == "gap") { style.spacing = value; style.has_physical = true; }
}

I18nEngine::NativeStyle I18nEngine::evaluate_native_style(const std::vector<StyleProperty>& props,
                                                           const StyleCatalogSnapshot* style_state,
                                                           const std::vector<std::string>& args) {
  NativeStyle style;
  if (!style_state) return style;
  std::unordered_set<std::string> seen;
  for (const auto& prop : props) {
    if (prop.prop_name.empty()) continue;
    std::string resolved = resolve_plain_text(style_state, prop.value, args, seen, 0);
    apply_physical_property(style, prop.prop_name, resolved);
  }
  return style;
}

void I18nEngine::populate_style_registry(StyleCatalogSnapshot* snapshot) {
  if (!snapshot) return;
  snapshot->style_registry.clear();
  std::vector<StyleProperty> props;
  props.reserve(8);
  for (const auto& kv : snapshot->catalog) {
    if (!is_style_token(kv.first)) continue;
    if (!parse_style_properties(kv.second, props)) continue;
    snapshot->style_registry.emplace(kv.first, props);
  }
}

uint32_t I18nEngine::fnv1a32(const uint8_t* data, size_t len) noexcept {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < len; ++i) {
    hash ^= data[i];
    hash *= 16777619u;
  }
  return hash;
}

bool I18nEngine::export_binary_catalog(const char* path) const {
  if (!path) return false;
  auto snapshot = acquire_snapshot();
  if (!snapshot || snapshot->catalog.empty()) return false;

  const auto& catalog = snapshot->catalog;

  struct ExportEntry {
    std::string base;
    std::string variant;
    std::string text;
    uint32_t text_offset;
    uint32_t text_length;
  };

  std::vector<ExportEntry> entries;
  entries.reserve(catalog.size());

  for (const auto& kv : catalog) {
    std::string base;
    std::string variant;
    if (!parse_variant_suffix(kv.first, base, variant)) {
      base = kv.first;
      variant.clear();
    }

    if (base.empty() || !is_hex_token(base)) return false;

    entries.push_back({ base, variant, kv.second, 0, 0 });
  }

  std::sort(entries.begin(), entries.end(), [](const ExportEntry& a, const ExportEntry& b) {
    if (a.base != b.base) return a.base < b.base;
    return a.variant < b.variant;
  });

  uint32_t current_offset = 0;
  for (auto& entry : entries) {
    entry.text_offset = current_offset;
    entry.text_length = (uint32_t)entry.text.size();
    current_offset += entry.text_length;
  }

  std::vector<uint8_t> entry_table;
  entry_table.reserve(entries.size() * 64);

  for (const auto& entry : entries) {
    entry_table.push_back((uint8_t)entry.base.size());
    entry_table.insert(entry_table.end(), entry.base.begin(), entry.base.end());
    entry_table.push_back((uint8_t)entry.variant.size());
    entry_table.insert(entry_table.end(), entry.variant.begin(), entry.variant.end());
    append_le_u32(entry_table, entry.text_offset);
    append_le_u32(entry_table, entry.text_length);
  }

  std::vector<uint8_t> string_table;
  string_table.reserve(current_offset);
  for (const auto& entry : entries) {
    string_table.insert(string_table.end(), entry.text.begin(), entry.text.end());
  }

  const size_t cap_locale = std::min(meta_locale.size(), (size_t)std::numeric_limits<uint16_t>::max());
  const size_t cap_fallback = std::min(meta_fallback.size(), (size_t)std::numeric_limits<uint16_t>::max());
  const size_t cap_note = std::min(meta_note.size(), (size_t)std::numeric_limits<uint16_t>::max());
  const uint16_t locale_len = (uint16_t)cap_locale;
  const uint16_t fallback_len = (uint16_t)cap_fallback;
  const uint16_t note_len = (uint16_t)cap_note;

  std::vector<uint8_t> metadata_block;
  metadata_block.reserve(METADATA_HEADER_SIZE + cap_locale + cap_fallback + cap_note);
  append_le_u16(metadata_block, locale_len);
  append_le_u16(metadata_block, fallback_len);
  append_le_u16(metadata_block, note_len);
  if (locale_len > 0) metadata_block.insert(metadata_block.end(), meta_locale.begin(), meta_locale.begin() + locale_len);
  if (fallback_len > 0) metadata_block.insert(metadata_block.end(), meta_fallback.begin(), meta_fallback.begin() + fallback_len);
  if (note_len > 0) metadata_block.insert(metadata_block.end(), meta_note.begin(), meta_note.begin() + note_len);
  const uint32_t metadata_size = (uint32_t)metadata_block.size();

  uint32_t checksum = 2166136261u;
  if (metadata_size > 0) checksum = fnv1a32_append(checksum, metadata_block.data(), metadata_block.size());
  checksum = fnv1a32_append(checksum, entry_table.data(), entry_table.size());
  checksum = fnv1a32_append(checksum, string_table.data(), string_table.size());

  uint8_t plural_rule = static_cast<uint8_t>(meta_plural);
  if (plural_rule > static_cast<uint8_t>(PluralRule::ARABIC)) plural_rule = static_cast<uint8_t>(PluralRule::DEFAULT);

  std::vector<uint8_t> header;
  header.reserve(BINARY_HEADER_SIZE);
  header.insert(header.end(), BINARY_MAGIC, BINARY_MAGIC + 4);
  header.push_back(BINARY_VERSION);
  header.push_back(0);
  header.push_back(plural_rule);
  header.push_back(0);
  append_le_u32(header, (uint32_t)entries.size());
  append_le_u32(header, (uint32_t)string_table.size());
  append_le_u32(header, checksum);
  append_le_u32(header, metadata_size);

  std::vector<uint8_t> buffer;
  buffer.reserve(header.size() + metadata_block.size() + entry_table.size() + string_table.size());
  buffer.insert(buffer.end(), header.begin(), header.end());
  if (metadata_size > 0) buffer.insert(buffer.end(), metadata_block.begin(), metadata_block.end());
  buffer.insert(buffer.end(), entry_table.begin(), entry_table.end());
  buffer.insert(buffer.end(), string_table.begin(), string_table.end());

  std::filesystem::path out_path(path);
  if (out_path.has_parent_path()) {
    std::filesystem::create_directories(out_path.parent_path());
  }

  std::ofstream out(out_path, std::ios::binary);
  if (!out) return false;
  out.write(reinterpret_cast<const char*>(buffer.data()), (std::streamsize)buffer.size());
  return out.good();
}
