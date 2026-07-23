#include "akai2sfz/krz_converter.hpp"
#include "akai2sfz/krz_format.hpp"
#include "akai2sfz/krz_raw_format.hpp"
#include "akai2sfz/sfz_writer.hpp"
#include "akai2sfz/wav_writer.hpp"

#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <map>

namespace akai2sfz {

using namespace krz_raw;

namespace {

void mkdir_p(const std::string &path) {
  ::mkdir(path.c_str(), 0755);
}

std::string to_upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
  return s;
}

// Busca recursiva por nome (sem distincao maiuscula/minuscula) na arvore do
// disco -- discos Kurzweil normalmente sao planos (sem subdiretorios), mas
// a busca cobre o caso geral mesmo assim.
bool find_krz_file(const KurzweilDisk &disk, const std::vector<KurzweilDirEntry> &entries,
                    const std::string &want_upper, KurzweilDirEntry *out) {
  for (const auto &e : entries) {
    if (e.is_directory) {
      if (find_krz_file(disk, disk.list_directory(e), want_upper, out)) return true;
    } else if (to_upper(e.name) == want_upper) {
      *out = e;
      return true;
    }
  }
  return false;
}

const KrzObjectRef *find_object(const std::vector<KrzObjectRef> &objs, ObjectType type, int id) {
  for (const auto &o : objs) {
    if (o.type_raw == static_cast<int>(type) && o.id == id) return &o;
  }
  return nullptr;
}

const KrzObjectRef *find_program(const std::vector<KrzObjectRef> &objs, const std::string &name) {
  for (const auto &o : objs) {
    if (o.type_raw == static_cast<int>(ObjectType::Program) && o.name == name) return &o;
  }
  return nullptr;
}

struct LoadedSample {
  KrzSample data;
  std::vector<std::string> wav_names; // 1 (mono) ou 2 (estereo: [0]=L, [1]=R)
};

int clamp_key(int v) {
  return std::max(0, std::min(127, v));
}

} // namespace

ConvertResult convert_krz_program(const KurzweilDisk &disk, const std::string &krz_file_name,
                                   const std::string &program_name, const std::string &output_dir) {
  ConvertResult result;
  result.program_name = program_name;

  KurzweilDirEntry file_entry;
  if (!find_krz_file(disk, disk.list_root(), to_upper(krz_file_name), &file_entry)) {
    result.error = "arquivo .krz nao encontrado: " + krz_file_name;
    return result;
  }

  auto krz_bytes = disk.read_file(file_entry);
  std::uint32_t osize;
  try {
    osize = krz_read_osize(krz_bytes);
  } catch (const std::exception &e) {
    result.error = std::string("arquivo .krz invalido (") + krz_file_name + "): " + e.what();
    return result;
  }

  auto objects = list_krz_objects(krz_bytes);
  const KrzObjectRef *program_ref = find_program(objects, program_name);
  if (!program_ref) {
    result.error = "program nao encontrado: " + krz_file_name + "/" + program_name;
    return result;
  }

  KrzProgram program = parse_krz_program(krz_bytes, *program_ref);

  mkdir_p(output_dir);

  SfzProgramInfo info;
  info.name = program.name;
  info.format_label = program.mode == 3 ? "Kurzweil K2500"
      : program.mode == 4              ? "Kurzweil K2600"
                                        : "Kurzweil K2000";
  info.low_key = 0;
  info.high_key = 127;

  // Coleta os samples distintos referenciados por todas as camadas, pra
  // extrair/converter cada um so uma vez.
  std::map<int, LoadedSample> samples;
  auto ensure_sample_loaded = [&](int sample_id) -> LoadedSample * {
    auto it = samples.find(sample_id);
    if (it != samples.end()) return &it->second;

    const KrzObjectRef *sref = find_object(objects, ObjectType::Sample, sample_id);
    if (!sref) return nullptr;

    LoadedSample ls;
    try {
      ls.data = parse_krz_sample(krz_bytes, *sref);
    } catch (const std::exception &e) {
      result.warnings.push_back("sample id=" + std::to_string(sample_id)
                                 + " invalido: " + e.what());
      return nullptr;
    }

    std::string base = sanitize_filename(ls.data.name.empty()
                                              ? ("sample" + std::to_string(sample_id))
                                              : ls.data.name);
    if (ls.data.stereo && ls.data.headers.size() >= 2) {
      ls.wav_names = {base + "_L.wav", base + "_R.wav"};
    } else {
      ls.wav_names = {base + ".wav"};
    }
    for (std::size_t h = 0; h < ls.wav_names.size() && h < ls.data.headers.size(); ++h) {
      auto pcm = krz_extract_pcm(krz_bytes, osize, ls.data.headers[h]);
      int rate = ls.data.headers[h].sample_period_ns > 0
          ? static_cast<int>(1000000000.0 / ls.data.headers[h].sample_period_ns + 0.5)
          : 44100;
      std::string wav_path = output_dir + "/" + ls.wav_names[h];
      write_wav_mono16(wav_path, pcm, rate);
      result.wav_paths.push_back(wav_path);
    }

    auto [inserted_it, ok] = samples.emplace(sample_id, std::move(ls));
    (void)ok;
    return &inserted_it->second;
  };

  std::vector<SfzRegion> regions;

  for (std::size_t li = 0; li < program.layers.size(); ++li) {
    const KrzLayer &layer = program.layers[li];
    if (layer.keymap_id < 0) {
      result.warnings.push_back("layer " + std::to_string(li) + " sem keymap (CAL nao decodificado)");
      continue;
    }
    const KrzObjectRef *kref = find_object(objects, ObjectType::Keymap, layer.keymap_id);
    if (!kref) {
      result.warnings.push_back("keymap id=" + std::to_string(layer.keymap_id) + " nao encontrado (layer "
                                 + std::to_string(li) + ")");
      continue;
    }
    KrzKeymap keymap = parse_krz_keymap(krz_bytes, *kref);

    int lokey = clamp_key(layer.lokey);
    int hikey = clamp_key(layer.hikey);

    // comprime teclas consecutivas com o mesmo (sample_id, subsample) numa
    // faixa lokey-hikey -- mesma tecnica ja usada pros lados Roland/E-mu.
    int key = lokey;
    while (key <= hikey) {
      if (key >= static_cast<int>(keymap.entries.size())) break;
      const KrzKeymapEntry &first = keymap.entries[static_cast<std::size_t>(key)];
      int sample_id = first.sample_id >= 0 ? first.sample_id : keymap.default_sample_id;
      int subsample = first.subsample_number;

      int range_start = key;
      while (key + 1 <= hikey && key + 1 < static_cast<int>(keymap.entries.size())) {
        const KrzKeymapEntry &next = keymap.entries[static_cast<std::size_t>(key + 1)];
        int next_sample_id = next.sample_id >= 0 ? next.sample_id : keymap.default_sample_id;
        if (next_sample_id != sample_id || next.subsample_number != subsample) break;
        ++key;
      }
      int range_end = key;
      ++key;

      if (sample_id < 0) continue; // tecla sem sample valido nesse keymap

      LoadedSample *ls = ensure_sample_loaded(sample_id);
      if (!ls) {
        result.warnings.push_back("sample id=" + std::to_string(sample_id) + " referenciado (teclas "
                                   + std::to_string(range_start) + "-" + std::to_string(range_end)
                                   + ") nao encontrado");
        continue;
      }

      std::size_t header_index = subsample > 0
          ? static_cast<std::size_t>(subsample - 1)
          : 0;
      if (header_index >= ls->data.headers.size()) header_index = 0;
      if (ls->data.headers.empty()) continue;

      auto make_region = [&](std::size_t hidx, const std::string &wav_name, int pan) {
        const KrzSoundfilehead &sfh = ls->data.headers[hidx];
        SfzRegion r;
        r.comment = "layer " + std::to_string(li) + ", teclas " + std::to_string(range_start) + "-"
            + std::to_string(range_end) + ", sample '" + ls->data.name + "'";
        r.lokey = range_start;
        r.hikey = range_end;
        r.pitch_keycenter = (range_start == range_end) ? range_start : sfh.root_key;
        r.lovel = 0;
        r.hivel = 127;
        r.pan = pan;
        r.sample_wav_filename = wav_name;
        // loop_start==sample_end e um loop de comprimento zero -- visto em
        // amostras de percussao reais (ex. "808 Kick") marcadas com o bit
        // de loop ligado mas sem regiao de loop de fato; tratado como
        // no_loop (ver README).
        if (sfh.looped && sfh.sample_loop_start_words < sfh.sample_end_words) {
          r.loop_mode = "loop_continuous";
          r.loop_start_frame = sfh.sample_loop_start_words - sfh.sample_start_words;
          r.loop_end_frame = sfh.sample_end_words - sfh.sample_start_words;
        }
        regions.push_back(std::move(r));
      };

      if (ls->wav_names.size() >= 2 && ls->data.stereo) {
        make_region(0, ls->wav_names[0], -100);
        std::size_t right_idx = std::min<std::size_t>(1, ls->data.headers.size() - 1);
        make_region(right_idx, ls->wav_names[1], 100);
      } else {
        make_region(header_index, ls->wav_names[0], 0);
      }
    }
  }

  std::string sfz_path = output_dir + "/" + sanitize_filename(program.name) + ".sfz";
  write_sfz(info, regions, sfz_path);

  result.success = true;
  result.sfz_path = sfz_path;
  return result;
}

} // namespace akai2sfz
