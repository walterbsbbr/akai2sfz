#include "akai2sfz/emu_converter.hpp"
#include "akai2sfz/emu_format.hpp"
#include "akai2sfz/sfz_writer.hpp"
#include "akai2sfz/wav_writer.hpp"

#include <sys/stat.h>

#include <algorithm>
#include <map>
#include <set>

namespace akai2sfz {

using namespace emu_raw;

namespace {

void mkdir_p(const std::string &path) {
  ::mkdir(path.c_str(), 0755);
}

bool find_folder(const EmuDisk &disk, const std::string &name, EmuFolder *out) {
  for (auto &f : disk.list_folders()) {
    if (f.name == name) {
      *out = std::move(f);
      return true;
    }
  }
  return false;
}

bool find_bank_file(const EmuDisk &disk, const EmuFolder &folder, const std::string &name,
                     EmuFileEntry *out) {
  for (auto &f : disk.list_files(folder)) {
    if (f.name == name) {
      *out = std::move(f);
      return true;
    }
  }
  return false;
}

int clamp_pan(double v) {
  return static_cast<int>(std::max(-100.0, std::min(100.0, v)));
}

struct LoadedSample {
  EmuSample data;
  std::string wav_left;  // sempre preenchido
  std::string wav_right; // vazio se mono
};

} // namespace

ConvertResult convert_emu_preset(const EmuDisk &disk, const std::string &folder_name,
                                  const std::string &bank_name, const std::string &preset_name,
                                  const std::string &output_dir) {
  ConvertResult result;
  result.program_name = preset_name;

  EmuFolder folder;
  if (!find_folder(disk, folder_name, &folder)) {
    result.error = "pasta nao encontrada: " + folder_name;
    return result;
  }
  EmuFileEntry bank_entry;
  if (!find_bank_file(disk, folder, bank_name, &bank_entry)) {
    result.error = "bank nao encontrado: " + folder_name + "/" + bank_name;
    return result;
  }

  auto bank_bytes = disk.read_file(bank_entry);
  BankFormat format = detect_emu_bank_format(bank_bytes);
  if (format == BankFormat::Unknown) {
    result.error = "formato de bank E-mu desconhecido (nao e EMULATOR THREE/3X/ESI SI-32 v3): "
        + folder_name + "/" + bank_name;
    return result;
  }

  int npresets = emu_bank_preset_count(bank_bytes, format);
  int preset_index = -1;
  EmuPreset preset;
  for (int i = 0; i < npresets; ++i) {
    EmuPreset p = parse_emu_preset(bank_bytes, format, i);
    if (p.name == preset_name) {
      preset = std::move(p);
      preset_index = i;
      break;
    }
  }
  if (preset_index < 0) {
    result.error = "preset nao encontrado: " + folder_name + "/" + bank_name + "/" + preset_name;
    return result;
  }

  mkdir_p(output_dir);

  SfzProgramInfo info;
  info.name = preset.name;
  info.format_label = "E-mu EMU3";
  info.low_key = kMidiNoteOffset;
  info.high_key = kMidiNoteOffset + static_cast<int>(kNumNotes) - 1;

  // Coleta os samples distintos referenciados pelas zonas efetivamente usadas.
  std::set<int> zones_used;
  for (const auto &nz : preset.note_zones) {
    if (nz.pri_zone != kZoneUnused && nz.pri_zone < preset.zones.size()) zones_used.insert(nz.pri_zone);
    if (nz.sec_zone != kZoneUnused && nz.sec_zone < preset.zones.size()) zones_used.insert(nz.sec_zone);
  }
  std::set<int> unique_samples;
  for (int zi : zones_used) {
    int sid = preset.zones[static_cast<std::size_t>(zi)].sample_id;
    if (sid > 0) unique_samples.insert(sid);
  }

  std::map<int, LoadedSample> samples;
  for (int sid : unique_samples) {
    EmuSample s;
    try {
      s = parse_emu_sample(bank_bytes, format, sid);
    } catch (const std::exception &e) {
      result.warnings.push_back("sample #" + std::to_string(sid) + " invalido: " + e.what());
      continue;
    }

    LoadedSample ls;
    std::string base = sanitize_filename(s.name.empty() ? ("sample" + std::to_string(sid)) : s.name);

    if (s.stereo && !s.pcm_right.empty()) {
      ls.wav_left = base + "_L.wav";
      ls.wav_right = base + "_R.wav";
      write_wav_mono16(output_dir + "/" + ls.wav_left, s.pcm_left, s.rate);
      write_wav_mono16(output_dir + "/" + ls.wav_right, s.pcm_right, s.rate);
      result.wav_paths.push_back(output_dir + "/" + ls.wav_left);
      result.wav_paths.push_back(output_dir + "/" + ls.wav_right);
    } else {
      ls.wav_left = base + ".wav";
      write_wav_mono16(output_dir + "/" + ls.wav_left, s.pcm_left, s.rate);
      result.wav_paths.push_back(output_dir + "/" + ls.wav_left);
    }

    ls.data = std::move(s);
    samples.emplace(sid, std::move(ls));
  }

  // Comprime teclas consecutivas com o mesmo note_zone numa faixa lokey-hikey
  // (mesma tecnica do lado Roland: partial_at_key -> compressao de faixas).
  std::vector<SfzRegion> regions;
  std::size_t key = 0;
  while (key < preset.note_zone_mapping.size()) {
    std::uint8_t nzi = preset.note_zone_mapping[key];
    std::size_t range_start = key;
    while (key < preset.note_zone_mapping.size() && preset.note_zone_mapping[key] == nzi) ++key;
    std::size_t range_end = key - 1;
    if (nzi >= preset.note_zones.size()) continue; // tecla sem som

    int lokey = static_cast<int>(range_start) + kMidiNoteOffset;
    int hikey = static_cast<int>(range_end) + kMidiNoteOffset;
    const EmuNoteZone &note_zone = preset.note_zones[nzi];

    for (std::uint8_t zone_idx : {note_zone.pri_zone, note_zone.sec_zone}) {
      if (zone_idx == kZoneUnused || zone_idx >= preset.zones.size()) continue;
      const EmuZone &zone = preset.zones[zone_idx];

      SfzRegion r;
      r.comment = "teclas " + std::to_string(lokey) + "-" + std::to_string(hikey) + ", zona "
          + std::to_string(static_cast<int>(zone_idx));
      r.lokey = lokey;
      r.hikey = hikey;
      // vel_pri/vel_sec do preset nao mostraram, em nenhum bank real testado,
      // uma divisao de velocidade coerente quando as duas camadas estao
      // ativas (valores tipicamente [0,0]/[0,255], ou irregulares nos poucos
      // casos com sec_zone real) -- ate validar contra audio real, as duas
      // camadas sao tratadas como sobrepostas (faixa completa 0-127) em vez
      // de crossfade por velocidade. Ver README.
      r.lovel = 0;
      r.hivel = 127;
      r.pitch_keycenter = static_cast<int>(zone.original_key) + kMidiNoteOffset;
      r.tune_semitones = zone.note_tuning * 1.5625 / 100.0;

      auto it = samples.find(zone.sample_id);
      if (it == samples.end()) {
        regions.push_back(r); // sample_wav_filename vazio -> pulado no writer com aviso
        continue;
      }
      const LoadedSample &ls = it->second;
      if (ls.data.loop_enabled) {
        r.loop_mode = "loop_continuous";
        r.loop_start_frame = ls.data.loop_start_frame;
        r.loop_end_frame = ls.data.loop_end_frame;
      }

      if (!ls.wav_right.empty()) {
        SfzRegion rl = r, rr = r;
        rl.sample_wav_filename = ls.wav_left;
        rl.pan = -100;
        rr.sample_wav_filename = ls.wav_right;
        rr.pan = 100;
        regions.push_back(std::move(rl));
        regions.push_back(std::move(rr));
      } else {
        r.sample_wav_filename = ls.wav_left;
        r.pan = clamp_pan(static_cast<double>(zone.vca_pan) * 100.0 / 64.0);
        regions.push_back(std::move(r));
      }
    }
  }

  std::string sfz_path = output_dir + "/" + sanitize_filename(preset.name) + ".sfz";
  write_sfz(info, regions, sfz_path);

  result.success = true;
  result.sfz_path = sfz_path;
  return result;
}

} // namespace akai2sfz
