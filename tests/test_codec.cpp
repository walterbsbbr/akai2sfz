#include "akai2sfz/codec.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

using namespace akai2sfz;

int main() {
  // Tabela: 0-9 -> '0'-'9', 10 -> ' ', 11-36 -> 'A'-'Z', 37 '#', 38 '+', 39 '-', 40 '.'
  assert(akai_to_ascii_char(0) == '0');
  assert(akai_to_ascii_char(9) == '9');
  assert(akai_to_ascii_char(10) == ' ');
  assert(akai_to_ascii_char(11) == 'A');
  assert(akai_to_ascii_char(36) == 'Z');
  assert(akai_to_ascii_char(37) == '#');
  assert(akai_to_ascii_char(38) == '+');
  assert(akai_to_ascii_char(39) == '-');
  assert(akai_to_ascii_char(40) == '.');

  for (int c = 0; c <= 40; ++c) {
    char a = akai_to_ascii_char(static_cast<std::uint8_t>(c));
    std::uint8_t back = ascii_to_akai_char(a);
    assert(back == static_cast<std::uint8_t>(c));
  }

  // "PIANO       " (12 bytes, akai-encoded) -> "PIANO" (sem espacos a direita)
  std::uint8_t raw[12];
  const char *src = "PIANO       "; // 12 chars
  for (int i = 0; i < 12; ++i) raw[i] = ascii_to_akai_char(src[i]);
  std::string decoded = akai_name_to_ascii(raw, 12);
  assert(decoded == "PIANO");

  std::cout << "test_codec: OK\n";
  return 0;
}
