#include "akai2sfz/sfz_writer.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace akai2sfz {

namespace {

std::string midi_note_name(int note) {
  static const char *names[] = {"C", "C#", "D", "D#", "E", "F",
                                 "F#", "G", "G#", "A", "A#", "B"};
  int octave = (note / 12) - 1;
  return std::string(names[note % 12]) + std::to_string(octave);
}

} // namespace

void write_sfz(const S3000Program &program, const SampleMetaMap &samples,
                const std::string &output_path) {
  std::ofstream f(output_path);
  if (!f) {
    throw std::runtime_error("nao foi possivel criar: " + output_path);
  }

  f << "// SFZ gerado a partir de program S3000 pelo akai2sfz\n";
  f << "// Program original: " << program.name << "\n";
  f << "// MIDI program: " << static_cast<int>(program.midi_prog)
    << ", canal: " << static_cast<int>(program.midi_chan) << "\n\n";

  f << "<global>\n";
  f << "// faixa de teclas global: " << static_cast<int>(program.low_key) << "-"
    << static_cast<int>(program.high_key) << "\n\n";

  if (program.keygroups.empty()) {
    std::cerr << "aviso: program '" << program.name << "' nao tem keygroups\n";
    return;
  }

  int written = 0;
  int total_zones = 0;
  for (std::size_t i = 0; i < program.keygroups.size(); ++i) {
    const auto &kg = program.keygroups[i];
    total_zones += static_cast<int>(kg.zones.size());

    for (std::size_t z = 0; z < kg.zones.size(); ++z) {
      const auto &zone = kg.zones[z];

      auto it = samples.find(zone.sample_name);
      if (it == samples.end()) {
        std::cerr << "aviso: sample '" << zone.sample_name << "' (keygroup " << (i + 1)
                  << ", zona " << (z + 1) << ") nao encontrado, region pulada\n";
        continue;
      }
      const SampleMeta &sm = it->second;

      f << "// keygroup " << (i + 1) << " zona " << (z + 1) << ": " << zone.sample_name << "\n";
      f << "// teclas: " << midi_note_name(kg.low_key) << " a " << midi_note_name(kg.high_key)
        << "\n";
      if (zone.low_vel != zone.high_vel) {
        f << "// velocity: " << static_cast<int>(zone.low_vel) << "-"
          << static_cast<int>(zone.high_vel) << "\n";
      }

      f << "<region>\n";
      f << "sample=" << sm.wav_filename << "\n";
      f << "lokey=" << static_cast<int>(kg.low_key) << "\n";
      f << "hikey=" << static_cast<int>(kg.high_key) << "\n";
      // pitch_keycenter usa a tecla raiz do proprio sample (mais correto do
      // que usar lokey do keygroup, que e a aproximacao que o protótipo
      // Python fazia).
      f << "pitch_keycenter=" << static_cast<int>(sm.root_key) << "\n";

      f << "lovel=" << static_cast<int>(zone.low_vel) << "\n";
      f << "hivel=" << static_cast<int>(zone.high_vel) << "\n";

      // afinacao combinada: fine-tune do sample + tune do keygroup, em cents.
      double total_tune = sm.tune_semitones + kg.tune;
      if (total_tune != 0.0) {
        f << "tune=" << static_cast<int>(std::lround(total_tune * 100.0)) << "\n";
      }

      int transpose = static_cast<int>(kg.pitch) - 60;
      if (transpose != 0) {
        f << "transpose=" << transpose << "\n";
      }
      if (zone.vol_offset != 0) {
        f << "volume=" << static_cast<int>(zone.vol_offset) << "\n";
      }
      if (zone.pan_offset != 0) {
        f << "pan=" << (static_cast<int>(zone.pan_offset) * 2) << "\n"; // -50..50 -> -100..100
      }

      f << "loop_mode=" << sm.loop_mode << "\n";
      if (sm.loop_mode != "no_loop") {
        f << "loop_start=" << sm.loop_start_frame << "\n";
        f << "loop_end=" << sm.loop_end_frame << "\n";
      }

      f << "\n";
      ++written;
    }
  }

  std::cerr << written << "/" << total_zones << " regions escritas em " << output_path << "\n";
}

} // namespace akai2sfz
