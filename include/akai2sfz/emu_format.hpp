#pragma once
// Formato de conteudo "EMU3 flat" (EMULATOR THREE / EMULATOR 3X / EMU
// SI-32 v3): bank binario com enderecamento absoluto de presets/samples.
// Espelha as formulas de `emu3bm` (dagargo/emu3bm, src/emu3bm.c) -- lido
// diretamente do fonte, nao resumido -- e validado campo a campo contra um
// bank real extraido de orbit.iso (ver git log): bank -> preset ->
// note_zone -> zone -> sample, todos os offsets batendo (nomes ASCII,
// sample_rate=48000, contagem de frames simetrica entre canais L/R).
//
// So o parsing "cru" mora aqui (struct-a-struct, sem decisao de SFZ) --
// compressao de faixas de tecla e mapeamento pra SfzRegion ficam no
// converter, mesmo padrao do lado Roland (roland_format.cpp vs
// roland_converter.cpp).

#include "akai2sfz/emu_raw_format.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace akai2sfz {

// Detecta o sub-formato pelos primeiros 16 bytes do bank (kBank_Format).
emu_raw::BankFormat detect_emu_bank_format(const std::vector<std::uint8_t> &bank_bytes);

// Nome do bank (kBank_Name, 16 bytes).
std::string emu_bank_name(const std::vector<std::uint8_t> &bank_bytes);

// Quantos presets/samples estao em uso neste bank (contiguos a partir do
// indice 0 -- mesmo algoritmo de emu3_get_bank_presets/emu3_get_bank_samples).
int emu_bank_preset_count(const std::vector<std::uint8_t> &bank_bytes, emu_raw::BankFormat format);
int emu_bank_sample_count(const std::vector<std::uint8_t> &bank_bytes, emu_raw::BankFormat format);

struct EmuEnvelope {
  std::uint8_t attack = 0;
  std::uint8_t hold = 0;
  std::uint8_t decay = 0;
  std::uint8_t sustain = 0;
  std::uint8_t release = 0;
};

struct EmuNoteZone {
  std::uint8_t opt_lsb = 0;
  std::uint8_t opt_msb = 0;
  std::uint8_t pri_zone = emu_raw::kZoneUnused;
  std::uint8_t sec_zone = emu_raw::kZoneUnused;
};

struct EmuZone {
  std::uint8_t original_key = 0; // nota E-mu (MIDI = +21)
  std::uint16_t sample_id = 0;   // 1-based (0 = nao deveria acontecer em zona referenciada)
  EmuEnvelope vca_envelope;
  std::uint8_t vcf_cutoff = 0;
  std::int8_t note_tuning = 0;   // -64..64 -> -100..100 cents (v*1.5625)
  std::int8_t vca_pan = 0;       // -64..63
  std::uint8_t vca_level = 0;    // 0..0x7f -> 0..100%
  std::int8_t vel_to_vca_level = 0;
  std::int8_t vel_to_pan = 0;
};

struct EmuPreset {
  std::string name;
  std::int8_t pitch_bend_range = 0;
  std::uint8_t vel_pri_low = 0, vel_pri_high = 127;
  std::uint8_t vel_sec_low = 0, vel_sec_high = 127;
  // indice, por tecla fisica (0..87, MIDI = +21), dentro de `note_zones`.
  // Valor >= note_zones.size() = tecla sem som.
  std::array<std::uint8_t, emu_raw::kNumNotes> note_zone_mapping{};
  std::vector<EmuNoteZone> note_zones;
  std::vector<EmuZone> zones;
};

EmuPreset parse_emu_preset(const std::vector<std::uint8_t> &bank_bytes, emu_raw::BankFormat format,
                            int preset_index);

struct EmuSample {
  std::string name;
  int rate = 44100;
  bool stereo = false;
  bool loop_enabled = false;
  std::uint32_t loop_start_frame = 0; // canal esquerdo
  std::uint32_t loop_end_frame = 0;
  std::vector<std::int16_t> pcm_left;
  std::vector<std::int16_t> pcm_right; // vazio se mono
};

// `sample_num` e 1-based, como em emu3_preset_zone::sample_id (mesma
// convencao usada por emu3bm).
EmuSample parse_emu_sample(const std::vector<std::uint8_t> &bank_bytes, emu_raw::BankFormat format,
                            int sample_num);

} // namespace akai2sfz
