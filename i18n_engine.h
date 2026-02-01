#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <functional>
#include <cstdint>
#include <memory>
#include <atomic>

class I18nEngine {
private:
  enum class PluralRule : uint8_t {
    DEFAULT = 0,
    SLAVIC  = 1,
    ARABIC  = 2
  };

  struct CatalogSnapshot {
    std::unordered_map<std::string, std::string> catalog;
    std::unordered_map<std::string, std::string> labels;
    std::unordered_map<std::string, std::set<std::string>> plural_variants;
    std::string meta_locale;
    std::string meta_fallback;
    std::string meta_note;
    PluralRule meta_plural = PluralRule::DEFAULT;
    virtual ~CatalogSnapshot() = default;
  };

  struct StyleProperty {
    uint32_t prop_hash;
    std::string prop_name;
    std::string value;
  };

  struct NativeStyle {
    float mass = 0.0f;
    float friction = 0.0f;
    float restitution = 0.0f;
    float drag = 0.0f;
    float gravity_scale = 1.0f;
    float spacing = 0.0f;
    bool has_physical = false;
  };

  struct StyleCatalogSnapshot : CatalogSnapshot {
    std::unordered_map<std::string, std::vector<StyleProperty>> style_registry;
  };

  std::shared_ptr<const CatalogSnapshot> active_snapshot;
  std::string last_error;
  std::string current_path;
  bool current_strict = false;
  std::string meta_locale;
  std::string meta_fallback;
  std::string meta_note;
  PluralRule meta_plural = PluralRule::DEFAULT;

  static bool is_ws(unsigned char c) noexcept;
  static bool is_digit(unsigned char c) noexcept;
  static bool is_xdigit(unsigned char c) noexcept;
  static bool is_digit_uc(char c) noexcept;
  static bool is_xdigit_uc(char c) noexcept;
  static void trim_inplace(std::string& s);
  static bool is_hex_token(const std::string& s);
  static void strip_utf8_bom(std::string& s);
  static std::string to_lower_ascii(std::string s);
  static std::string unescape_txt_min(const std::string& s);
  static bool parse_line(const std::string& line_in,
                         std::string& out_token,
                         std::string& out_label,
                         std::string& out_text,
                         std::string& out_err);
  static bool is_style_token(const std::string& token) noexcept;
  static bool parse_style_properties(const std::string& text, std::vector<StyleProperty>& out_props);
  static bool parse_physical_value(const std::string& text, float& out_value);
  static NativeStyle build_native_style(const std::vector<StyleProperty>& props);
  static void populate_style_registry(StyleCatalogSnapshot* snapshot);
  static std::string read_file_utf8(const char* path, std::string& err);
  static bool try_parse_inline_token(const std::string& s, size_t at_pos,
                                     std::string& out_token, size_t& out_advance);
  static void scan_inline_refs(const std::string& text, std::vector<std::string>& out_refs);
  static bool looks_like_binary_catalog(const std::string& data) noexcept;
  static bool parse_variant_suffix(const std::string& token, std::string& out_base, std::string& out_variant);
  static bool is_variant_valid(const std::string& variant) noexcept;
  static uint32_t fnv1a32(const uint8_t* data, size_t len) noexcept;
  static bool parse_meta_line(const std::string& line, std::string& key, std::string& value);
  static PluralRule parse_plural_rule_name(std::string v, bool& ok);
  static const char* pick_variant_name(PluralRule rule, int count) noexcept;
  static bool starts_with(const std::string& s, const char* pref);
  void set_last_error(std::string msg);
  void clear_last_error();
  friend void set_engine_error(I18nEngine* eng, const std::string& msg);
  friend void clear_engine_error(I18nEngine* eng);

  std::string resolve_arg(const CatalogSnapshot* state,
                          const std::string& arg,
                          std::unordered_set<std::string>& seen,
                          int depth);
  std::string resolve_plain_text(const CatalogSnapshot* state,
                                 const std::string& raw,
                                 const std::vector<std::string>& args,
                                 std::unordered_set<std::string>& seen,
                                 int depth);
  std::string translate_impl(const CatalogSnapshot* state,
                             const std::string& token,
                             const std::vector<std::string>& args,
                             std::unordered_set<std::string>& seen,
                             int depth);
  bool try_build_style_string(const StyleCatalogSnapshot* style_state,
                              const std::string& token,
                              const std::vector<std::string>& args,
                              std::unordered_set<std::string>& seen,
                              int depth,
                              std::string& out_style);

  std::shared_ptr<CatalogSnapshot> build_snapshot_from_text(std::string&& src, bool strict, std::string& err);
  std::shared_ptr<CatalogSnapshot> build_snapshot_from_binary(const uint8_t* data, size_t size, bool strict,
                                                              std::string& err);
  void install_snapshot(std::shared_ptr<CatalogSnapshot> snapshot);
  std::shared_ptr<const CatalogSnapshot> acquire_snapshot() const noexcept;
  static bool is_binary_catalog_path(const std::string& path) noexcept;
public:
  enum class PublicPluralRule : uint8_t {
    DEFAULT = 0,
    SLAVIC  = 1,
    ARABIC  = 2
  };

  const char* get_last_error() const noexcept;
  const std::string& get_meta_locale() const noexcept;
  const std::string& get_meta_fallback() const noexcept;
  const std::string& get_meta_note() const noexcept;
  PublicPluralRule get_meta_plural_rule() const noexcept;
  bool load_txt_catalog(std::string src, bool strict);
  bool load_txt_file(const char* path, bool strict);
  bool reload();
  std::string translate(const std::string& token_in, const std::vector<std::string>& args);
  std::string translate_plural(const std::string& token_in, int count, const std::vector<std::string>& args);
  std::string dump_table() const;
  std::string find_any(const std::string& query) const;
  std::string check_catalog_report(int& out_code) const;
  bool export_binary_catalog(const char* path) const;
  NativeStyle get_native_style(const std::string& style_token, const std::vector<std::string>& args);
  NativeStyle evaluate_native_style(const std::vector<StyleProperty>& props,
                                    const StyleCatalogSnapshot* style_state,
                                    const std::vector<std::string>& args);
  void apply_physical_property(NativeStyle& style, const std::string& key, const std::string& raw_value) const;
};
