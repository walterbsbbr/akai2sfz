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

void write_sfz(const SfzProgramInfo &info, const std::vector<SfzRegion> &regions,
                const std::string &output_path) {
  std::ofstream f(output_path);
  if (!f) {
    throw std::runtime_error("nao foi possivel criar: " + output_path);
  }

  f << "// SFZ gerado a partir de program " << info.format_label << " pelo akai2sfz\n";
  f << "// Program original: " << info.name << "\n";
  f << "// MIDI program: " << info.midi_prog << ", canal: " << info.midi_chan << "\n\n";

  f << "<global>\n";
  f << "// faixa de teclas global: " << info.low_key << "-" << info.high_key << "\n\n";

  if (regions.empty()) {
    std::cerr << "aviso: program '" << info.name << "' nao tem regions\n";
    return;
  }

  int written = 0;
  for (const auto &r : regions) {
    if (r.sample_wav_filename.empty()) {
      std::cerr << "aviso: " << r.comment << " -- sample nao encontrado, region pulada\n";
      continue;
    }

    if (!r.comment.empty()) {
      f << "// " << r.comment << "\n";
    }
    f << "// teclas: " << midi_note_name(r.lokey) << " a " << midi_note_name(r.hikey) << "\n";
    if (r.lovel != r.hivel) {
      f << "// velocity: " << r.lovel << "-" << r.hivel << "\n";
    }

    f << "<region>\n";
    f << "sample=" << r.sample_wav_filename << "\n";
    f << "lokey=" << r.lokey << "\n";
    f << "hikey=" << r.hikey << "\n";
    f << "pitch_keycenter=" << r.pitch_keycenter << "\n";
    f << "lovel=" << r.lovel << "\n";
    f << "hivel=" << r.hivel << "\n";

    if (r.tune_semitones != 0.0) {
      f << "tune=" << static_cast<int>(std::lround(r.tune_semitones * 100.0)) << "\n";
    }
    if (r.transpose != 0) {
      f << "transpose=" << r.transpose << "\n";
    }
    if (r.volume_db != 0) {
      f << "volume=" << r.volume_db << "\n";
    }
    if (r.pan != 0) {
      f << "pan=" << r.pan << "\n";
    }

    f << "loop_mode=" << r.loop_mode << "\n";
    if (r.loop_mode != "no_loop") {
      f << "loop_start=" << r.loop_start_frame << "\n";
      f << "loop_end=" << r.loop_end_frame << "\n";
    }

    f << "\n";
    ++written;
  }

  std::cerr << written << "/" << regions.size() << " regions escritas em " << output_path << "\n";
}

} // namespace akai2sfz
