#include "akai2sfz/roland_converter.hpp"
#include "akai2sfz/roland_format.hpp"
#include "akai2sfz/sfz_writer.hpp"
#include "akai2sfz/wav_writer.hpp"

#include <sys/stat.h>

#include <map>
#include <set>

namespace akai2sfz {

using namespace roland_raw;

namespace {

void mkdir_p(const std::string &path) {
  ::mkdir(path.c_str(), 0755);
}

bool find_patch_by_name(const RolandDisk &disk, const std::string &name, RolandDirEntry *out) {
  for (std::size_t i = 0; i < kDirPatch.entry_count; ++i) {
    RolandDirEntry e = disk.read_dir_entry(FileType::Patch, i);
    if (e.is_active() && e.name == name) {
      *out = e;
      return true;
    }
  }
  return false;
}

std::string loop_mode_to_sfz(RolandLoopMode mode, bool enabled) {
  if (!enabled) return "no_loop";
  switch (mode) {
    // "Alternate" (ping-pong) nao tem equivalente direto no SFZ padrao;
    // loop_continuous e a aproximacao razoavel mais proxima.
    case RolandLoopMode::Forward:
    case RolandLoopMode::Alternate: return "loop_continuous";
    case RolandLoopMode::Reverse: return "loop_continuous"; // SFZ nao tem reverse-loop nativo
    case RolandLoopMode::OneShot:
    default: return "no_loop";
  }
}

} // namespace

ConvertResult convert_roland_patch(const RolandDisk &disk, const std::string &patch_name,
                                    const std::string &output_dir) {
  ConvertResult result;
  result.program_name = patch_name;

  RolandDirEntry patch_entry;
  if (!find_patch_by_name(disk, patch_name, &patch_entry)) {
    result.error = "patch nao encontrado: " + patch_name;
    return result;
  }

  RolandPatch patch = parse_roland_patch(disk.read_param(FileType::Patch, patch_entry.index));

  mkdir_p(output_dir);

  SfzProgramInfo info;
  info.name = patch.name;
  info.format_label = "Roland S-750/770";
  info.low_key = 21;
  info.high_key = 108;

  // Coleta os partials distintos referenciados pelas 88 teclas.
  std::set<std::uint16_t> unique_partials;
  for (std::uint16_t pidx : patch.partial_at_key) {
    if (pidx != kPartialListUnused) unique_partials.insert(pidx);
  }

  struct LoadedPartial {
    RolandDirEntry entry;
    RolandPartial data;
  };
  std::map<std::uint16_t, LoadedPartial> partials;
  for (std::uint16_t pidx : unique_partials) {
    if (pidx >= kDirPartial.entry_count) {
      result.warnings.push_back("indice de partial fora do limite: " + std::to_string(pidx));
      continue;
    }
    LoadedPartial lp;
    lp.entry = disk.read_dir_entry(FileType::Partial, pidx);
    lp.data = parse_roland_partial(disk.read_param(FileType::Partial, pidx));
    partials.emplace(pidx, std::move(lp));
  }

  // Coleta os samples distintos referenciados por esses partials, extrai e
  // converte cada um para WAV uma unica vez.
  std::set<std::uint16_t> unique_samples;
  for (const auto &[pidx, lp] : partials) {
    for (const auto &slot : lp.data.sample_slots) unique_samples.insert(slot.sample_index);
  }

  struct LoadedSample {
    RolandDirEntry entry;
    RolandSample data;
    std::string wav_filename;
  };
  std::map<std::uint16_t, LoadedSample> samples;
  for (std::uint16_t sidx : unique_samples) {
    if (sidx >= kDirSample.entry_count) {
      result.warnings.push_back("indice de sample fora do limite: " + std::to_string(sidx));
      continue;
    }
    LoadedSample ls;
    ls.entry = disk.read_dir_entry(FileType::Sample, sidx);
    auto wave = disk.read_sample_wave(ls.entry);
    ls.data = parse_roland_sample(disk.read_param(FileType::Sample, sidx), wave);

    ls.wav_filename = sanitize_filename(ls.data.name) + ".wav";
    std::string wav_path = output_dir + "/" + ls.wav_filename;
    write_wav_mono16(wav_path, ls.data.pcm, ls.data.rate);
    result.wav_paths.push_back(wav_path);

    samples.emplace(sidx, std::move(ls));
  }

  // Comprime teclas consecutivas com o mesmo partial numa faixa lokey-hikey,
  // e emite uma region por slot de sample dentro desse partial.
  std::vector<SfzRegion> regions;
  std::size_t key = 0;
  while (key < patch.partial_at_key.size()) {
    std::uint16_t pidx = patch.partial_at_key[key];
    std::size_t range_start = key;
    while (key < patch.partial_at_key.size() && patch.partial_at_key[key] == pidx) ++key;
    std::size_t range_end = key - 1;
    if (pidx == kPartialListUnused) continue;

    int lokey = static_cast<int>(range_start) + 21;
    int hikey = static_cast<int>(range_end) + 21;

    auto pit = partials.find(pidx);
    if (pit == partials.end()) continue; // ja registrado como aviso acima

    for (const auto &slot : pit->second.data.sample_slots) {
      SfzRegion r;
      r.comment = "tecla " + std::to_string(lokey) + "-" + std::to_string(hikey) + ", partial '"
          + pit->second.entry.name + "'";
      r.lokey = lokey;
      r.hikey = hikey;
      r.lovel = slot.vel_lower;
      r.hivel = slot.vel_upper;
      // pan Roland e aproximadamente -64..+63; escala para -100..100 do SFZ
      // (nao validado contra audio real -- ver README).
      r.pan = static_cast<int>(slot.pan) * 100 / 64;
      r.tune_semitones = slot.coarse_tune + slot.fine_tune / 100.0;

      auto sit = samples.find(slot.sample_index);
      if (sit == samples.end()) {
        regions.push_back(r); // sample_wav_filename vazio -> pulado no writer com aviso
        continue;
      }
      const auto &ls = sit->second;
      r.sample_wav_filename = ls.wav_filename;
      // original_key foi validado contra amostras reais (ex.: 'E3 Tap Hrm 1'
      // -> original_key=52=E3) e e confiavel. Mas em patches de percussao,
      // onde o partial e mapeado numa unica tecla, esse campo pode conter
      // um valor sem relacao com a tecla real de disparo (ex.: kick mapeado
      // so em C2 com original_key=G-1) -- provavelmente porque a altura nao
      // importa pra esses sons e o campo fica com lixo/default. Faixa de
      // uma unica tecla usa a propria tecla como centro (sempre correto,
      // sem ambiguidade); faixa mais larga confia no original_key.
      r.pitch_keycenter = (lokey == hikey) ? lokey : ls.data.original_key;
      r.loop_mode = loop_mode_to_sfz(ls.data.loop_mode, ls.data.s_loop_enable);
      if (r.loop_mode != "no_loop") {
        r.loop_start_frame = ls.data.s_loop_start;
        r.loop_end_frame = ls.data.s_loop_end;
      }
      regions.push_back(r);
    }
  }

  std::string sfz_path = output_dir + "/" + sanitize_filename(patch.name) + ".sfz";
  write_sfz(info, regions, sfz_path);

  result.success = true;
  result.sfz_path = sfz_path;
  return result;
}

} // namespace akai2sfz
