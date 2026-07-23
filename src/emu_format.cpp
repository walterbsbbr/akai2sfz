#include "akai2sfz/emu_format.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace akai2sfz {

using namespace emu_raw;

namespace {

std::uint16_t le16(const std::uint8_t *p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t le32(const std::uint8_t *p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
      | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::string ascii_trim(const std::uint8_t *p, std::size_t len) {
  std::string s(reinterpret_cast<const char *>(p), len);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
  return s;
}

// Constantes de layout por sub-formato -- espelha emu3_get_preset_addresses/
// emu3_get_preset_address/emu3_get_sample_start_address/
// emu3_get_sample_addresses de emu3bm.c linha por linha (nomes identicos).
struct FormatConsts {
  std::size_t preset_addr_table_start;
  std::uint32_t preset_start;
  std::uint32_t preset_offset; // so != 0 para EMULATOR THREE
  std::size_t sample_addr_table_start;
  int max_presets;
  int max_samples;
};

FormatConsts consts_for(BankFormat format) {
  switch (format) {
    case BankFormat::Emulator3X:
    case BankFormat::EsiV3:
      return {kPresetSizeAddrStart_Emu3X, kPresetStart_Emu3X, 0, kSampleAddrStart_Emu3X,
              kMaxPresets_Emu3X, kMaxSamples_Emu3X};
    case BankFormat::EmulatorThree:
    case BankFormat::Unknown:
    default:
      return {kPresetSizeAddrStart_EmuThree, kPresetStart_EmuThree, kPresetOffset_EmuThree,
              kSampleAddrStart_EmuThree, kMaxPresets_EmuThree, kMaxSamples_EmuThree};
  }
}

std::uint32_t preset_table_value(const std::vector<std::uint8_t> &bank, const FormatConsts &c,
                                  int index) {
  std::size_t off = c.preset_addr_table_start + static_cast<std::size_t>(index) * 4;
  if (off + 4 > bank.size()) throw std::runtime_error("bank E-mu truncado (tabela de presets)");
  return le32(bank.data() + off);
}

std::uint32_t preset_address(const std::vector<std::uint8_t> &bank, const FormatConsts &c,
                              int preset_index) {
  std::uint32_t offset = preset_table_value(bank, c, preset_index);
  return c.preset_start + offset - c.preset_offset;
}

std::uint32_t sample_start_address(const std::vector<std::uint8_t> &bank, const FormatConsts &c) {
  std::uint32_t offset = preset_table_value(bank, c, c.max_presets);
  std::uint32_t start = c.preset_start + 1 - c.preset_offset;
  return start + offset;
}

std::uint32_t sample_table_value(const std::vector<std::uint8_t> &bank, const FormatConsts &c,
                                  int index0based) {
  std::size_t off = c.sample_addr_table_start + static_cast<std::size_t>(index0based) * 4;
  if (off + 4 > bank.size()) throw std::runtime_error("bank E-mu truncado (tabela de samples)");
  return le32(bank.data() + off);
}

std::uint32_t sample_address(const std::vector<std::uint8_t> &bank, const FormatConsts &c,
                              int sample_num_1based) {
  std::uint32_t start = sample_start_address(bank, c);
  std::uint32_t offset = sample_table_value(bank, c, sample_num_1based - 1);
  return start + offset - kSampleOffset;
}

} // namespace

emu_raw::BankFormat detect_emu_bank_format(const std::vector<std::uint8_t> &bank_bytes) {
  if (bank_bytes.size() < kFormatSize) return BankFormat::Unknown;
  const char *fmt = reinterpret_cast<const char *>(bank_bytes.data() + kBank_Format);
  if (std::memcmp(fmt, kFmt_EmulatorThree, kFormatSize) == 0) return BankFormat::EmulatorThree;
  if (std::memcmp(fmt, kFmt_Emulator3X, kFormatSize) == 0) return BankFormat::Emulator3X;
  if (std::memcmp(fmt, kFmt_EsiV3, kFormatSize) == 0) return BankFormat::EsiV3;
  return BankFormat::Unknown;
}

std::string emu_bank_name(const std::vector<std::uint8_t> &bank_bytes) {
  if (bank_bytes.size() < kBank_Name + 16) return "";
  return ascii_trim(bank_bytes.data() + kBank_Name, 16);
}

int emu_bank_preset_count(const std::vector<std::uint8_t> &bank_bytes, BankFormat format) {
  FormatConsts c = consts_for(format);
  int total = 0;
  while (total < c.max_presets) {
    std::uint32_t a0 = preset_table_value(bank_bytes, c, total);
    std::uint32_t a1 = preset_table_value(bank_bytes, c, total + 1);
    if (a0 == a1) break; // slot nao usado -- mesmo criterio de emu3_get_bank_presets
    ++total;
  }
  return total;
}

int emu_bank_sample_count(const std::vector<std::uint8_t> &bank_bytes, BankFormat format) {
  FormatConsts c = consts_for(format);
  int total = 0;
  while (total < c.max_samples) {
    if (sample_table_value(bank_bytes, c, total) == 0) break; // mesmo criterio de emu3_get_bank_samples
    ++total;
  }
  return total;
}

EmuPreset parse_emu_preset(const std::vector<std::uint8_t> &bank_bytes, BankFormat format,
                            int preset_index) {
  FormatConsts c = consts_for(format);
  std::uint32_t addr = preset_address(bank_bytes, c, preset_index);
  if (static_cast<std::uint64_t>(addr) + kPresetSize > bank_bytes.size()) {
    throw std::runtime_error("preset E-mu fora dos limites do bank");
  }
  const std::uint8_t *h = bank_bytes.data() + addr;

  EmuPreset p;
  p.name = ascii_trim(h + kPreset_Name, 16);
  p.pitch_bend_range = static_cast<std::int8_t>(h[kPreset_PitchBendRange]);
  p.vel_pri_low = h[kPreset_VelPriLow];
  p.vel_pri_high = h[kPreset_VelPriHigh];
  p.vel_sec_low = h[kPreset_VelSecLow];
  p.vel_sec_high = h[kPreset_VelSecHigh];
  std::uint8_t note_zones_count = h[kPreset_NoteZonesCount];
  for (std::size_t i = 0; i < kNumNotes; ++i) {
    p.note_zone_mapping[i] = h[kPreset_NoteZoneMappings + i];
  }

  std::uint64_t nz_addr = static_cast<std::uint64_t>(addr) + kPresetSize;
  std::uint64_t nz_bytes = static_cast<std::uint64_t>(note_zones_count) * kNoteZoneSize;
  if (nz_addr + nz_bytes > bank_bytes.size()) {
    throw std::runtime_error("note-zones do preset E-mu fora dos limites do bank");
  }
  p.note_zones.resize(note_zones_count);
  for (std::size_t i = 0; i < note_zones_count; ++i) {
    const std::uint8_t *nz = bank_bytes.data() + nz_addr + i * kNoteZoneSize;
    p.note_zones[i].opt_lsb = nz[kNoteZone_OptLsb];
    p.note_zones[i].opt_msb = nz[kNoteZone_OptMsb];
    p.note_zones[i].pri_zone = nz[kNoteZone_PriZone];
    p.note_zones[i].sec_zone = nz[kNoteZone_SecZone];
  }

  int max_zone = -1;
  for (const auto &nz : p.note_zones) {
    if (nz.pri_zone != kZoneUnused) max_zone = std::max(max_zone, static_cast<int>(nz.pri_zone));
    if (nz.sec_zone != kZoneUnused) max_zone = std::max(max_zone, static_cast<int>(nz.sec_zone));
  }

  if (max_zone >= 0) {
    std::uint64_t zone_base = nz_addr + nz_bytes;
    std::uint64_t zones_bytes = static_cast<std::uint64_t>(max_zone + 1) * kZoneSize;
    if (zone_base + zones_bytes > bank_bytes.size()) {
      throw std::runtime_error("zonas do preset E-mu fora dos limites do bank");
    }
    p.zones.resize(max_zone + 1);
    for (int zi = 0; zi <= max_zone; ++zi) {
      const std::uint8_t *z = bank_bytes.data() + zone_base + static_cast<std::size_t>(zi) * kZoneSize;
      EmuZone &zone = p.zones[static_cast<std::size_t>(zi)];
      zone.original_key = z[kZone_OriginalKey];
      zone.sample_id = static_cast<std::uint16_t>(z[kZone_SampleIdLsb] | (z[kZone_SampleIdMsb] << 8));
      zone.vca_envelope = {z[kZone_VcaEnvelope], z[kZone_VcaEnvelope + 1], z[kZone_VcaEnvelope + 2],
                            z[kZone_VcaEnvelope + 3], z[kZone_VcaEnvelope + 4]};
      zone.vcf_cutoff = z[kZone_VcfCutoff];
      zone.note_tuning = static_cast<std::int8_t>(z[kZone_NoteTuning]);
      zone.vca_pan = static_cast<std::int8_t>(z[kZone_VcaPan]);
      zone.vca_level = z[kZone_VcaLevel];
      zone.vel_to_vca_level = static_cast<std::int8_t>(z[kZone_VelToVcaLevel]);
      zone.vel_to_pan = static_cast<std::int8_t>(z[kZone_VelToPan]);
    }
  }

  return p;
}

EmuSample parse_emu_sample(const std::vector<std::uint8_t> &bank_bytes, BankFormat format,
                            int sample_num) {
  FormatConsts c = consts_for(format);
  std::uint32_t addr = sample_address(bank_bytes, c, sample_num);
  if (static_cast<std::uint64_t>(addr) + kSampleHeaderSize > bank_bytes.size()) {
    throw std::runtime_error("sample E-mu fora dos limites do bank");
  }
  const std::uint8_t *h = bank_bytes.data() + addr;

  EmuSample s;
  s.name = ascii_trim(h + kSample_Name, 16);

  std::uint32_t start_l = le32(h + kSample_StartL);
  std::uint32_t start_r = le32(h + kSample_StartR);
  std::uint32_t end_l = le32(h + kSample_EndL);
  std::uint32_t end_r = le32(h + kSample_EndR);
  std::uint32_t loop_start_l = le32(h + kSample_LoopStartL);
  std::uint32_t loop_end_l = le32(h + kSample_LoopEndL);
  s.rate = static_cast<int>(le32(h + kSample_SampleRate));
  std::uint16_t options = le16(h + kSample_Options);
  s.stereo = (options & kOpt_Stereo) == kOpt_Stereo;
  s.loop_enabled = (options & (kOpt_Loop | kOpt_LoopRelease)) != 0;

  auto read_channel = [&](std::uint32_t start, std::uint32_t end, std::vector<std::int16_t> &out) {
    if (end < start) return;
    std::uint64_t frames = (static_cast<std::uint64_t>(end) - start) / 2 + 1;
    std::uint64_t byte_end = static_cast<std::uint64_t>(addr) + end + 2;
    if (byte_end > bank_bytes.size()) {
      throw std::runtime_error("PCM do sample E-mu fora dos limites do bank");
    }
    out.resize(frames);
    const std::uint8_t *pcm = bank_bytes.data() + addr + start;
    for (std::uint64_t i = 0; i < frames; ++i) {
      out[i] = static_cast<std::int16_t>(le16(pcm + i * 2));
    }
  };

  read_channel(start_l, end_l, s.pcm_left);
  if (s.stereo) read_channel(start_r, end_r, s.pcm_right);

  s.loop_start_frame = (loop_start_l >= start_l) ? (loop_start_l - start_l) / 2 : 0;
  s.loop_end_frame = (loop_end_l >= start_l) ? (loop_end_l - start_l) / 2 : 0;

  return s;
}

} // namespace akai2sfz
