#include "akai2sfz/akai_format.hpp"
#include "akai2sfz/codec.hpp"

#include <cstring>
#include <stdexcept>

namespace akai2sfz {

namespace {

constexpr std::size_t kSampleHeaderSize = 192;
constexpr std::size_t kProgramCommonSize = 0xC0; // 192
// 192 bytes, medido contra GRV070-1.a3p real: (1152 - 192) / 5 keygroups = 192
// exato. O protótipo Python usava 450 ("approximate") e o akai2sfz chegou a
// tentar 480 antes disso -- ambos liam o keygroup seguinte fora do lugar.
constexpr std::size_t kKeygroupSize = 0xC0;

// Zonas de velocidade dentro de um keygroup: offsets 0x22, 0x3A, 0x52, 0x6A
// (24 bytes entre uma e outra), confirmado contra o mesmo arquivo real --
// pares estereo Akai usam zona 0 = canal L, zona 1 = canal R, ambas com a
// mesma faixa de velocidade (0-127), zonas 2-3 ficam vazias quando nao usadas.
constexpr std::size_t kZoneBaseOffset = 0x22;
constexpr std::size_t kZoneStride = 24;
constexpr std::size_t kZoneCount = 4;
constexpr std::size_t kZone_Name = 0;        // 12 bytes, relativo ao inicio da zona
constexpr std::size_t kZone_LowVel = 12;
constexpr std::size_t kZone_HighVel = 13;
constexpr std::size_t kZone_VolOffset = 16;
constexpr std::size_t kZone_PanOffset = 18;

std::uint16_t le16(const std::uint8_t *p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::int16_t sle16(const std::uint8_t *p) {
  return static_cast<std::int16_t>(le16(p));
}

std::uint32_t le32(const std::uint8_t *p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
      | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

} // namespace

S3000Sample parse_s3000_sample(const std::vector<std::uint8_t> &raw) {
  if (raw.size() < kSampleHeaderSize) {
    throw std::runtime_error("arquivo .a3s menor que o header esperado (192 bytes)");
  }
  const std::uint8_t *h = raw.data();

  S3000Sample s;
  s.key = h[0x02];
  s.name = akai_name_to_ascii(h + 0x03, 12);
  s.num_active_loops = h[0x10];
  s.first_active_loop = h[0x11];
  s.loop_mode_raw = static_cast<AkaiLoopMode>(h[0x13] <= 3 ? h[0x13] : 2);
  s.tune = sle16(h + 0x14) / 256.0;
  s.size_words = le32(h + 0x1A);
  s.start = le32(h + 0x1E);
  s.end = le32(h + 0x22);
  s.loop_start = le32(h + 0x26);
  s.loop_len = le32(h + 0x2C);
  s.loop_time_ms = le16(h + 0x30);
  s.rate = le16(h + 0x8A);

  const std::uint8_t *pcm_bytes = raw.data() + kSampleHeaderSize;
  std::size_t pcm_bytes_len = raw.size() - kSampleHeaderSize;
  std::size_t nsamples = pcm_bytes_len / 2;
  s.pcm.resize(nsamples);
  for (std::size_t i = 0; i < nsamples; ++i) {
    s.pcm[i] = static_cast<std::int16_t>(le16(pcm_bytes + i * 2));
  }

  return s;
}

bool S3000Sample::has_loop() const {
  return num_active_loops > 0 && first_active_loop > 0
      && loop_mode_raw != AkaiLoopMode::None && loop_mode_raw != AkaiLoopMode::PlayToEnd;
}

S3000Program parse_s3000_program(const std::vector<std::uint8_t> &raw) {
  if (raw.size() < kProgramCommonSize) {
    throw std::runtime_error("arquivo .a3p menor que a secao comum esperada (192 bytes)");
  }
  const std::uint8_t *h = raw.data();

  S3000Program p;
  p.name = akai_name_to_ascii(h + 0x03, 12);
  p.midi_prog = h[0x0F];
  p.midi_chan = h[0x10];
  p.low_key = h[0x13];
  p.high_key = h[0x14];
  std::uint8_t num_keygroups = h[0x2A];

  std::size_t offset = kProgramCommonSize;
  for (std::uint8_t i = 0; i < num_keygroups; ++i) {
    if (offset + kKeygroupSize > raw.size()) {
      break; // arquivo truncado ou menos keygroups do que o declarado
    }
    const std::uint8_t *kg = raw.data() + offset;

    S3000Keygroup g;
    g.low_key = kg[0x03];
    g.high_key = kg[0x04];
    g.tune = sle16(kg + 0x05) / 256.0;
    g.pitch = kg[0x84];

    for (std::size_t z = 0; z < kZoneCount; ++z) {
      const std::uint8_t *zp = kg + kZoneBaseOffset + z * kZoneStride;
      std::string sample_name = akai_name_to_ascii(zp + kZone_Name, 12);
      if (sample_name.empty()) continue; // zona nao usada

      S3000Zone zone;
      zone.sample_name = std::move(sample_name);
      zone.low_vel = zp[kZone_LowVel];
      zone.high_vel = zp[kZone_HighVel];
      zone.vol_offset = static_cast<std::int8_t>(zp[kZone_VolOffset]);
      zone.pan_offset = static_cast<std::int8_t>(zp[kZone_PanOffset]);
      g.zones.push_back(std::move(zone));
    }

    if (!g.zones.empty()) {
      p.keygroups.push_back(std::move(g));
    }

    offset += kKeygroupSize;
  }

  return p;
}

// --- S1000 ---
// Mesmos offsets do S3000 para tudo que os dois formatos tem em comum (ver
// comentario no .hpp); so o tamanho do registro (150 em vez de 192) e a
// codificacao de tune mudam.
namespace {
constexpr std::size_t kS1000SampleHeaderSize = 150;
constexpr std::size_t kS1000RecordSize = 150; // header de sample, common de program, e keygroup

constexpr std::size_t kS1000Zone_Loudness = 16;
constexpr std::size_t kS1000Zone_Pan = 18;
constexpr std::size_t kS1000_VelZonesUsed = 0x1F;
// "sample 1-4 key tracking" no doc Kellett: 1 byte por zona, offset 0x84+z.
constexpr std::size_t kS1000_KeyTrackingBase = 0x84;

// Cents e semitons vem em bytes signed separados (doc Kellett), nao no
// fixed-point de 16 bits do S3000. Interpretados como valores literais
// (nao ha uma segunda fonte para confirmar se ha reescala envolvida).
double decode_s1000_tune(std::int8_t cents, std::int8_t semitones) {
  return static_cast<double>(semitones) + static_cast<double>(cents) / 100.0;
}
} // namespace

S1000Sample parse_s1000_sample(const std::vector<std::uint8_t> &raw) {
  if (raw.size() < kS1000SampleHeaderSize) {
    throw std::runtime_error("arquivo .a1s menor que o header esperado (150 bytes)");
  }
  const std::uint8_t *h = raw.data();

  S1000Sample s;
  s.key = h[0x02];
  s.name = akai_name_to_ascii(h + 0x03, 12);
  s.num_active_loops = h[0x10];
  s.loop_mode_raw = static_cast<AkaiLoopMode>(h[0x13] <= 3 ? h[0x13] : 2);
  s.tune = decode_s1000_tune(static_cast<std::int8_t>(h[0x14]), static_cast<std::int8_t>(h[0x15]));
  s.size_words = le32(h + 0x1A);
  s.start = le32(h + 0x1E);
  s.end = le32(h + 0x22);
  s.loop_start = le32(h + 0x26);
  s.loop_len = le32(h + 0x2C);
  s.loop_time_ms = le16(h + 0x30);
  s.rate = le16(h + 0x8A);

  const std::uint8_t *pcm_bytes = raw.data() + kS1000SampleHeaderSize;
  std::size_t pcm_bytes_len = raw.size() - kS1000SampleHeaderSize;
  std::size_t nsamples = pcm_bytes_len / 2;
  s.pcm.resize(nsamples);
  for (std::size_t i = 0; i < nsamples; ++i) {
    s.pcm[i] = static_cast<std::int16_t>(le16(pcm_bytes + i * 2));
  }

  return s;
}

bool S1000Sample::has_loop() const {
  return num_active_loops > 0 && loop_mode_raw != AkaiLoopMode::None
      && loop_mode_raw != AkaiLoopMode::PlayToEnd;
}

S1000Program parse_s1000_program(const std::vector<std::uint8_t> &raw) {
  if (raw.size() < kS1000RecordSize) {
    throw std::runtime_error("arquivo .a1p menor que a secao comum esperada (150 bytes)");
  }
  const std::uint8_t *h = raw.data();

  S1000Program p;
  p.name = akai_name_to_ascii(h + 0x03, 12);
  p.midi_prog = h[0x0F];
  p.midi_chan = h[0x10];
  p.low_key = h[0x13];
  p.high_key = h[0x14];
  std::uint8_t num_keygroups = h[0x2A];

  std::size_t offset = kS1000RecordSize;
  for (std::uint8_t i = 0; i < num_keygroups; ++i) {
    if (offset + kS1000RecordSize > raw.size()) {
      break; // arquivo truncado ou menos keygroups do que o declarado
    }
    const std::uint8_t *kg = raw.data() + offset;

    S1000Keygroup g;
    g.low_key = kg[0x03];
    g.high_key = kg[0x04];
    g.tune = decode_s1000_tune(static_cast<std::int8_t>(kg[0x05]), static_cast<std::int8_t>(kg[0x06]));

    std::uint8_t zones_used = kg[kS1000_VelZonesUsed];
    if (zones_used > kZoneCount) zones_used = kZoneCount; // defensivo

    for (std::size_t z = 0; z < kZoneCount; ++z) {
      const std::uint8_t *zp = kg + kZoneBaseOffset + z * kZoneStride;
      std::string sample_name = akai_name_to_ascii(zp + kZone_Name, 12);
      // usa "vel zones used" quando plausivel, mas cai para nome-nao-vazio
      // se o campo parecer invalido -- mesma defesa que o parser S3000 usa.
      bool use_zone = (zones_used > 0) ? (z < zones_used) : !sample_name.empty();
      if (!use_zone || sample_name.empty()) continue;

      S1000Zone zone;
      zone.sample_name = std::move(sample_name);
      zone.low_vel = zp[kZone_LowVel];
      zone.high_vel = zp[kZone_HighVel];
      zone.loudness = static_cast<std::int8_t>(zp[kS1000Zone_Loudness]);
      zone.pan = static_cast<std::int8_t>(zp[kS1000Zone_Pan]);
      // key tracking do sample z (0x84 + z): 0=TRACK, 1=FIXED (doc Kellett).
      zone.fixed_pitch = kg[kS1000_KeyTrackingBase + z] != 0;
      g.zones.push_back(std::move(zone));
    }

    if (!g.zones.empty()) {
      p.keygroups.push_back(std::move(g));
    }

    offset += kS1000RecordSize;
  }

  return p;
}

} // namespace akai2sfz
