#pragma once
// Writer de SFZ generico -- nao depende do formato Akai de origem (S1000 ou
// S3000). O converter.cpp monta um SfzRegion por zona de velocidade, seja
// qual for o formato, e este modulo so sabe escrever texto SFZ.

#include <cstdint>
#include <string>
#include <vector>

namespace akai2sfz {

struct SfzRegion {
  std::string comment;              // ex.: "keygroup 1 zona 1: HIPHOP01  -L"
  std::string sample_wav_filename;  // relativo ao diretorio do .sfz
  int lokey = 0;
  int hikey = 127;
  int pitch_keycenter = 60;
  int lovel = 0;
  int hivel = 127;
  double tune_semitones = 0.0; // combinado: fine-tune do sample + do keygroup
  int transpose = 0;           // semitons inteiros; 0 se nao aplicavel ao formato
  int volume_db = 0;
  int pan = 0; // -100..100
  std::string loop_mode = "no_loop"; // "no_loop" | "loop_continuous" | "loop_sustain"
  std::uint32_t loop_start_frame = 0;
  std::uint32_t loop_end_frame = 0;
};

struct SfzProgramInfo {
  std::string name;
  std::string format_label; // "S1000" ou "S3000", so para o comentario de cabecalho
  int midi_prog = 0;
  int midi_chan = 0;
  int low_key = 0;
  int high_key = 127;
};

// Escreve um .sfz com uma <region> por SfzRegion. Regions sem
// sample_wav_filename (string vazia) sao puladas com aviso no stderr -- usado
// pelo converter para sinalizar "sample referenciado mas nao encontrado".
void write_sfz(const SfzProgramInfo &info, const std::vector<SfzRegion> &regions,
                const std::string &output_path);

} // namespace akai2sfz
