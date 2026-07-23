#include "akai2sfz/converter.hpp"
#include "akai2sfz/akai_format.hpp"
#include "akai2sfz/sfz_writer.hpp"
#include "akai2sfz/wav_writer.hpp"

#include <sys/stat.h>

#include <map>
#include <set>

namespace akai2sfz {

std::string sanitize_filename(const std::string &name) {
  std::string out;
  for (char c : name) {
    if (static_cast<unsigned char>(c) < 32) continue;
    const std::string invalid = "<>:\"/\\|?*";
    out.push_back(invalid.find(c) != std::string::npos ? '_' : c);
  }
  // colapsa espacos/underscores repetidos em um unico '_'
  std::string collapsed;
  bool last_was_sep = false;
  for (char c : out) {
    bool is_sep = (c == ' ' || c == '_');
    if (is_sep) {
      if (!last_was_sep) collapsed.push_back('_');
      last_was_sep = true;
    } else {
      collapsed.push_back(c);
      last_was_sep = false;
    }
  }
  while (!collapsed.empty() && (collapsed.front() == '.' || collapsed.front() == '_')) {
    collapsed.erase(collapsed.begin());
  }
  while (!collapsed.empty() && (collapsed.back() == '.' || collapsed.back() == '_')) {
    collapsed.pop_back();
  }
  return collapsed.empty() ? "unnamed" : collapsed;
}

namespace {

bool find_volume(const OpenPartition &part, const std::string &name, std::size_t *out_index) {
  for (std::size_t vi = 0; vi < part.volume_count(); ++vi) {
    raw::VolType t = part.volume_type(vi);
    if (t == raw::VolType::Inactive) continue;
    if (part.volume_name(vi) == name) {
      *out_index = vi;
      return true;
    }
  }
  return false;
}

const FileEntry *find_file(const std::vector<FileEntry> &files, const std::string &name,
                            const std::string &ext) {
  for (const auto &f : files) {
    if (f.name == name && f.extension == ext) return &f;
  }
  return nullptr;
}

void mkdir_p(const std::string &path) {
  ::mkdir(path.c_str(), 0755); // ignora erro se ja existir
}

std::string loop_mode_to_sfz(AkaiLoopMode mode) {
  switch (mode) {
    // "in release" continua tocando o loop mesmo depois do note-off -- o
    // mais proximo em SFZ e loop_continuous (nao ha equivalente exato).
    case AkaiLoopMode::InRelease: return "loop_continuous";
    // caso mais comum: loop enquanto a nota esta sustentada, para no release.
    case AkaiLoopMode::UntilRelease: return "loop_sustain";
    case AkaiLoopMode::None:
    case AkaiLoopMode::PlayToEnd:
    default: return "no_loop";
  }
}

// Extrai e converte para WAV cada sample distinto referenciado pelo program,
// devolvendo um mapa nome-do-sample -> (nome do arquivo wav, sample parseado).
template <typename SampleT, typename ParseFn>
std::map<std::string, std::pair<std::string, SampleT>> extract_and_convert_samples(
    const OpenPartition &part, const std::vector<FileEntry> &files,
    const std::set<std::string> &names, const std::string &ext, const std::string &output_dir,
    ParseFn parse, std::vector<std::string> &wav_paths, std::vector<std::string> &warnings) {
  std::map<std::string, std::pair<std::string, SampleT>> cache;
  for (const auto &sname : names) {
    const FileEntry *entry = find_file(files, sname, ext);
    if (!entry) {
      warnings.push_back("sample nao encontrado: " + sname);
      continue;
    }
    auto bytes = extract_file(part, *entry);
    SampleT sample = parse(bytes);

    std::string wav_name = sanitize_filename(sample.name) + ".wav";
    std::string wav_path = output_dir + "/" + wav_name;
    write_wav_mono16(wav_path, sample.pcm, sample.rate);
    wav_paths.push_back(wav_path);

    cache.emplace(sname, std::make_pair(wav_name, std::move(sample)));
  }
  return cache;
}

template <typename SampleT>
void apply_loop(SfzRegion &r, const SampleT &sample) {
  if (sample.has_loop() && sample.loop_len > 0) {
    r.loop_mode = loop_mode_to_sfz(sample.loop_mode_raw);
    r.loop_start_frame = sample.loop_start;
    r.loop_end_frame = sample.loop_start + sample.loop_len;
  }
}

} // namespace

ConvertResult convert_program(const OpenPartition &part, const std::string &volume_name,
                               const std::string &program_name, const std::string &output_dir) {
  ConvertResult result;
  result.program_name = program_name;

  std::size_t vi = 0;
  if (!find_volume(part, volume_name, &vi)) {
    result.error = "volume nao encontrado: " + volume_name;
    return result;
  }

  auto files = list_files(part, vi);

  const FileEntry *s3000_entry = find_file(files, program_name, "a3p");
  const FileEntry *s1000_entry = s3000_entry ? nullptr : find_file(files, program_name, "a1p");

  if (!s3000_entry && !s1000_entry) {
    result.error = "program nao encontrado: " + volume_name + "/" + program_name;
    return result;
  }

  mkdir_p(output_dir);

  SfzProgramInfo info;
  std::vector<SfzRegion> regions;

  if (s3000_entry) {
    auto prog_bytes = extract_file(part, *s3000_entry);
    S3000Program program = parse_s3000_program(prog_bytes);

    info.name = program.name;
    info.format_label = "S3000";
    info.midi_prog = program.midi_prog;
    info.midi_chan = program.midi_chan;
    info.low_key = program.low_key;
    info.high_key = program.high_key;

    std::set<std::string> unique_samples;
    for (const auto &kg : program.keygroups) {
      for (const auto &zone : kg.zones) unique_samples.insert(zone.sample_name);
    }
    auto sample_cache = extract_and_convert_samples<S3000Sample>(
        part, files, unique_samples, "a3s", output_dir, parse_s3000_sample, result.wav_paths,
        result.warnings);

    for (std::size_t i = 0; i < program.keygroups.size(); ++i) {
      const auto &kg = program.keygroups[i];
      for (std::size_t z = 0; z < kg.zones.size(); ++z) {
        const auto &zone = kg.zones[z];

        SfzRegion r;
        r.comment = "keygroup " + std::to_string(i + 1) + " zona " + std::to_string(z + 1) + ": "
            + zone.sample_name;

        auto it = sample_cache.find(zone.sample_name);
        if (it == sample_cache.end()) {
          regions.push_back(std::move(r)); // sample_wav_filename vazio -> pulado no writer
          continue;
        }
        const auto &[wav_name, sample] = it->second;

        r.sample_wav_filename = wav_name;
        r.lokey = kg.low_key;
        r.hikey = kg.high_key;
        r.pitch_keycenter = sample.key;
        r.lovel = zone.low_vel;
        r.hivel = zone.high_vel;
        r.tune_semitones = sample.tune + kg.tune;
        r.transpose = static_cast<int>(kg.pitch) - 60;
        r.volume_db = zone.vol_offset;
        r.pan = static_cast<int>(zone.pan_offset) * 2; // -50..50 -> -100..100
        apply_loop(r, sample);

        regions.push_back(std::move(r));
      }
    }

  } else {
    auto prog_bytes = extract_file(part, *s1000_entry);
    S1000Program program = parse_s1000_program(prog_bytes);

    info.name = program.name;
    info.format_label = "S1000";
    info.midi_prog = program.midi_prog;
    info.midi_chan = program.midi_chan;
    info.low_key = program.low_key;
    info.high_key = program.high_key;

    std::set<std::string> unique_samples;
    for (const auto &kg : program.keygroups) {
      for (const auto &zone : kg.zones) unique_samples.insert(zone.sample_name);
    }
    auto sample_cache = extract_and_convert_samples<S1000Sample>(
        part, files, unique_samples, "a1s", output_dir, parse_s1000_sample, result.wav_paths,
        result.warnings);

    for (std::size_t i = 0; i < program.keygroups.size(); ++i) {
      const auto &kg = program.keygroups[i];
      for (std::size_t z = 0; z < kg.zones.size(); ++z) {
        const auto &zone = kg.zones[z];

        SfzRegion r;
        r.comment = "keygroup " + std::to_string(i + 1) + " zona " + std::to_string(z + 1) + ": "
            + zone.sample_name;

        auto it = sample_cache.find(zone.sample_name);
        if (it == sample_cache.end()) {
          regions.push_back(std::move(r));
          continue;
        }
        const auto &[wav_name, sample] = it->second;

        r.sample_wav_filename = wav_name;
        r.lokey = kg.low_key;
        r.hikey = kg.high_key;
        // "key tracking" FIXED (comum em kits de bateria) significa que a
        // amostra deve soar sempre na mesma altura -- ignora a tecla raiz
        // do sample e usa a propria tecla do keygroup, derrotando qualquer
        // transposicao pelo SFZ. Ver akai_format.hpp (S1000Zone::fixed_pitch).
        r.pitch_keycenter = zone.fixed_pitch ? kg.low_key : sample.key;
        r.lovel = zone.low_vel;
        r.hivel = zone.high_vel;
        r.tune_semitones = sample.tune + kg.tune;
        // S1000 nao documenta um campo de transpose por keygroup equivalente
        // ao "pitch" do S3000 -- fica 0.
        r.transpose = 0;
        r.volume_db = zone.loudness;
        r.pan = static_cast<int>(zone.pan) * 2; // -50..50 -> -100..100
        apply_loop(r, sample);

        regions.push_back(std::move(r));
      }
    }
  }

  std::string sfz_path = output_dir + "/" + sanitize_filename(info.name) + ".sfz";
  write_sfz(info, regions, sfz_path);

  result.success = true;
  result.sfz_path = sfz_path;
  return result;
}

} // namespace akai2sfz
