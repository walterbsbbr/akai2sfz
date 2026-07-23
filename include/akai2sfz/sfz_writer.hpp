#pragma once
#include "akai2sfz/akai_format.hpp"

#include <map>
#include <string>

namespace akai2sfz {

// Tudo que o writer de SFZ precisa saber sobre um sample Akai ja convertido,
// indexado pelo nome do sample (S3000Keygroup::sample_name).
struct SampleMeta {
  std::string wav_filename;      // relativo ao diretorio do .sfz
  std::uint8_t root_key = 60;    // S3000Sample::key
  double tune_semitones = 0.0;   // S3000Sample::tune
  std::string loop_mode = "no_loop"; // "no_loop" | "loop_continuous" | "loop_sustain"
  std::uint32_t loop_start_frame = 0;
  std::uint32_t loop_end_frame = 0;
};
using SampleMetaMap = std::map<std::string, SampleMeta>;

// Gera um .sfz a partir de um program S3000 ja parseado e dos metadados dos
// samples referenciados (ja convertidos para WAV). Keygroups cujo sample nao
// esta em `samples` sao pulados (com aviso no stderr).
void write_sfz(const S3000Program &program, const SampleMetaMap &samples,
                const std::string &output_path);

} // namespace akai2sfz
