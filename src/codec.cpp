#include "akai2sfz/codec.hpp"

namespace akai2sfz {

char akai_to_ascii_char(std::uint8_t c) {
  if (c <= 9) return static_cast<char>('0' + c);
  if (c == 10) return ' ';
  if (c >= 11 && c <= 36) return static_cast<char>('A' + (c - 11));
  if (c == 37) return '#';
  if (c == 38) return '+';
  if (c == 39) return '-';
  if (c == 40) return '.';
  return '.'; // valor invalido: mesmo fallback do akaiutil original
}

std::uint8_t ascii_to_akai_char(char a) {
  if (a == '_') a = ' '; // convencao para entrada via teclado (mesma do akaiutil)
  if (a >= '0' && a <= '9') return static_cast<std::uint8_t>(a - '0');
  if (a == ' ') return 10;
  if (a >= 'A' && a <= 'Z') return static_cast<std::uint8_t>(11 + (a - 'A'));
  if (a >= 'a' && a <= 'z') return static_cast<std::uint8_t>(11 + (a - 'a'));
  if (a == '#') return 37;
  if (a == '+') return 38;
  if (a == '-') return 39;
  if (a == '.') return 40;
  return 40; // fallback
}

std::string akai_name_to_ascii(const std::uint8_t *raw, std::size_t len) {
  std::string out;
  out.reserve(len);
  for (std::size_t i = 0; i < len; ++i) {
    out.push_back(akai_to_ascii_char(raw[i]));
  }
  while (!out.empty() && out.back() == ' ') {
    out.pop_back();
  }
  return out;
}

} // namespace akai2sfz
