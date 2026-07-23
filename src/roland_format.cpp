#include "akai2sfz/roland_format.hpp"

#include <stdexcept>

namespace akai2sfz {

using namespace roland_raw;

namespace {

std::uint16_t le16(const std::uint8_t *p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::string ascii_trim(const std::uint8_t *p, std::size_t len) {
  std::string s(reinterpret_cast<const char *>(p), len);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
  return s;
}

// Ponto de sample: 4 bytes = fine(1) + endereco 24-bit LE (LSB, mid, MSB).
// A parte "fine" (sub-palavra) e descartada aqui -- SFZ trabalha em frames
// inteiros. NAO validado se "endereco" e em words ou bytes; ver README.
std::uint32_t decode_point(const std::uint8_t *p) {
  return static_cast<std::uint32_t>(p[1]) | (static_cast<std::uint32_t>(p[2]) << 8)
      | (static_cast<std::uint32_t>(p[3]) << 16);
}

} // namespace

RolandPatch parse_roland_patch(const std::vector<std::uint8_t> &raw) {
  if (raw.size() < kParamPatch.entry_size) {
    throw std::runtime_error("parametro de patch Roland menor que o esperado");
  }
  const std::uint8_t *h = raw.data();

  RolandPatch p;
  p.name = ascii_trim(h + kPatchParam_Name, kDirEntry_NameLen);

  // "Partial List" e indexado DIRETO pela tecla (nao ha indirecao via
  // "Partial Sel" -- hipotese inicial baseada no padrao de outras listas do
  // formato, refutada contra dados reais: kPatchParam_PartialSel acaba
  // sendo 0xFF/0 sem relacao consistente com o indice usado de fato).
  // "Nao usado" e 0xFFFF, nao 0 -- confirmado contra KIK:Gretsch Kik5 em
  // Roland S770 Drum Samples.iso (87 teclas em 0xFFFF, 1 em 335).
  const std::uint8_t *list = h + kPatchParam_PartialList;
  for (std::size_t key = 0; key < kPatchParam_PartialSelCount; ++key) {
    p.partial_at_key[key] = le16(list + key * 2); // 0xFFFF = tecla sem som
  }
  return p;
}

RolandPartial parse_roland_partial(const std::vector<std::uint8_t> &raw) {
  if (raw.size() < kParamPartial.entry_size) {
    throw std::runtime_error("parametro de partial Roland menor que o esperado");
  }
  const std::uint8_t *h = raw.data();

  RolandPartial partial;
  partial.name = ascii_trim(h + kPartialParam_Name, kDirEntry_NameLen);

  for (std::size_t i = 0; i < kPartialParam_SampleCount; ++i) {
    const std::uint8_t *s = h + kPartialParam_SampleBase + i * kPartialParam_SampleStride;
    std::uint16_t sample_index = le16(s + kPSample_Sel);
    // 0xFFFF = slot vazio; indices sao 0-based direto na Sample directory
    // (mesma convencao confirmada para Partial List -- ver parse_roland_patch).
    if (sample_index == 0xFFFF) continue;

    RolandPartialSlot slot;
    slot.sample_index = sample_index;
    slot.level = static_cast<std::int8_t>(s[kPSample_Level]);
    slot.pan = static_cast<std::int8_t>(s[kPSample_Pan]);
    slot.coarse_tune = static_cast<std::int8_t>(s[kPSample_CoarseTune]);
    slot.fine_tune = static_cast<std::int8_t>(s[kPSample_FineTune]);
    slot.vel_lower = s[kPSample_VelLower];
    slot.vel_upper = s[kPSample_VelUpper];
    slot.lower_fade_width = s[kPSample_LowerFadeWidth];
    slot.upper_fade_width = s[kPSample_UpperFadeWidth];
    partial.sample_slots.push_back(slot);
  }
  return partial;
}

RolandSample parse_roland_sample(const std::vector<std::uint8_t> &param_raw,
                                  const std::vector<std::uint8_t> &wave) {
  if (param_raw.size() < kParamSample.entry_size) {
    throw std::runtime_error("parametro de sample Roland menor que o esperado");
  }
  const std::uint8_t *h = param_raw.data();

  RolandSample s;
  s.name = ascii_trim(h + kSampleParam_Name, kDirEntry_NameLen);
  s.start_point = decode_point(h + kSampleParam_StartPoint);
  s.s_loop_start = decode_point(h + kSampleParam_SLoopStart);
  s.s_loop_end = decode_point(h + kSampleParam_SLoopEnd);

  std::uint8_t loop_mode_raw = h[kSampleParam_LoopMode];
  s.loop_mode = static_cast<RolandLoopMode>(loop_mode_raw <= 3 ? loop_mode_raw : 2);
  s.s_loop_enable = h[kSampleParam_SLoopEnable] != 0;
  s.original_key = h[kSampleParam_OriginalKey];

  // Frequencia/modo de amostragem: o manual nao da o mapeamento exato de
  // bits para o S-750/770 (so formatos anteriores, S-50/330/550, que usam
  // uma tabela diferente e nao necessariamente aplicavel aqui). Ate validar
  // contra audio real, assume 44100 Hz sempre -- ver README.
  s.rate = 44100;

  s.pcm.resize(wave.size() / 2);
  for (std::size_t i = 0; i < s.pcm.size(); ++i) {
    s.pcm[i] = static_cast<std::int16_t>(le16(wave.data() + i * 2));
  }

  return s;
}

} // namespace akai2sfz
