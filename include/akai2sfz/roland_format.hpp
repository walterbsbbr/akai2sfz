#pragma once
// Parser de conteudo Roland: Patch, Partial e Sample -- monta a partir dos
// blocos de parametro brutos (ja lidos via RolandDisk::read_param) e, para
// Sample, dos bytes de onda (RolandDisk::read_sample_wave).
//
// Hierarquia: Patch -> (88 teclas, cada uma aponta pra um indice local que
// e resolvido via Partial List) -> Partial -> (ate 4 slots de sample,
// velocity layers) -> Sample.

#include "akai2sfz/roland_raw_format.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace akai2sfz {

enum class RolandLoopMode : std::uint8_t {
  Forward = 0,
  Alternate = 1,
  OneShot = 2,
  Reverse = 3,
};

struct RolandSample {
  std::string name;
  std::uint32_t start_point = 0;   // endereco 24-bit (words), parte fina descartada p/ uso em SFZ
  std::uint32_t s_loop_start = 0;
  std::uint32_t s_loop_end = 0;
  bool s_loop_enable = false;
  RolandLoopMode loop_mode = RolandLoopMode::OneShot;
  std::uint8_t original_key = 60;
  std::uint16_t rate = 44100; // derivado de freq_mode
  std::vector<std::int16_t> pcm; // mono, 16-bit

  bool has_loop() const { return s_loop_enable && loop_mode != RolandLoopMode::OneShot; }
};

// Um dos ate 4 slots de velocity de um Partial.
struct RolandPartialSlot {
  std::uint16_t sample_index = 0xFFFF; // 0-based na Sample directory; 0xFFFF = slot vazio
  std::int8_t level = 0;
  std::int8_t pan = 0; // -64..+63 (ver .cpp)
  std::int8_t coarse_tune = 0;
  std::int8_t fine_tune = 0;
  std::uint8_t vel_lower = 0;
  std::uint8_t vel_upper = 127;
  std::uint8_t lower_fade_width = 0;
  std::uint8_t upper_fade_width = 0;
};

struct RolandPartial {
  std::string name;
  // "slots" evitado de proposito -- e uma macro do Qt (signals/slots) e
  // quebra a compilacao em qualquer .cpp que tambem inclua headers do Qt.
  std::vector<RolandPartialSlot> sample_slots; // so entradas com sample_index != 0xFFFF
};

// Um Patch tem uma tabela de 88 teclas (MIDI 21-108, A0-C8), cada uma
// apontando pro indice do Partial (0-based na Partial directory; 0xFFFF =
// tecla sem som). `partial_at_key[i]` corresponde a tecla MIDI `21 + i`.
struct RolandPatch {
  std::string name;
  std::array<std::uint16_t, roland_raw::kPatchParam_PartialSelCount> partial_at_key{};
};

RolandPatch parse_roland_patch(const std::vector<std::uint8_t> &raw);
RolandPartial parse_roland_partial(const std::vector<std::uint8_t> &raw);

// `wave` e o conteudo bruto ja extraido via RolandDisk::read_sample_wave.
RolandSample parse_roland_sample(const std::vector<std::uint8_t> &param_raw,
                                  const std::vector<std::uint8_t> &wave);

} // namespace akai2sfz
