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
  const FileEntry *prog_entry = find_file(files, program_name, "a3p");
  if (!prog_entry) {
    result.error = "program S3000 nao encontrado: " + volume_name + "/" + program_name
        + " (M2 so suporta .a3p; veja README para S1000)";
    return result;
  }

  auto prog_bytes = extract_file(part, *prog_entry);
  S3000Program program = parse_s3000_program(prog_bytes);

  mkdir_p(output_dir);

  // extrai/converte cada sample referenciado uma unica vez, mesmo que varias
  // zonas (de um ou mais keygroups) apontem para o mesmo nome
  std::set<std::string> unique_samples;
  for (const auto &kg : program.keygroups) {
    for (const auto &zone : kg.zones) unique_samples.insert(zone.sample_name);
  }

  SampleMetaMap sample_meta;
  for (const auto &sname : unique_samples) {
    const FileEntry *sample_entry = find_file(files, sname, "a3s");
    if (!sample_entry) {
      result.warnings.push_back("sample nao encontrado: " + sname);
      continue;
    }

    auto sample_bytes = extract_file(part, *sample_entry);
    S3000Sample sample = parse_s3000_sample(sample_bytes);

    std::string wav_name = sanitize_filename(sample.name) + ".wav";
    std::string wav_path = output_dir + "/" + wav_name;
    write_wav_mono16(wav_path, sample.pcm, sample.rate);
    result.wav_paths.push_back(wav_path);

    SampleMeta meta;
    meta.wav_filename = wav_name;
    meta.root_key = sample.key;
    meta.tune_semitones = sample.tune;
    // loop_mode_raw e o byte explicito em 0x13 (doc Kellett), muito mais
    // confiavel que a heuristica anterior baseada em loop_time_ms (0x30),
    // que na verdade e um tempo em milissegundos, nao um modo/contador.
    if (!sample.has_loop() || sample.loop_len == 0) {
      meta.loop_mode = "no_loop";
    } else if (sample.loop_mode_raw == S3000LoopMode::InRelease) {
      // continua tocando o loop mesmo durante o release: o mais proximo em
      // SFZ e loop_continuous (nao ha equivalente exato).
      meta.loop_mode = "loop_continuous";
    } else {
      // UntilRelease (o caso mais comum): loop enquanto a nota esta
      // sustentada, para no release -- exatamente o que loop_sustain faz.
      meta.loop_mode = "loop_sustain";
    }
    if (meta.loop_mode != "no_loop") {
      // loop_start/loop_len ja estao em words (frames), NAO em bytes --
      // confirmado pelo doc Kellett ("Number of sample words" / "Start
      // marker" / "Loop 1 coarse length (words)"). Nao dividir por 2 aqui.
      meta.loop_start_frame = sample.loop_start;
      meta.loop_end_frame = sample.loop_start + sample.loop_len;
    }

    sample_meta[sname] = meta;
  }

  std::string sfz_path = output_dir + "/" + sanitize_filename(program.name) + ".sfz";
  write_sfz(program, sample_meta, sfz_path);

  result.success = true;
  result.sfz_path = sfz_path;
  return result;
}

} // namespace akai2sfz
