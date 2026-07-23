#include "akai2sfz/wav_writer.hpp"

#include <sndfile.h>

#include <stdexcept>

namespace akai2sfz {

void write_wav_mono16(const std::string &path, const std::vector<std::int16_t> &pcm,
                       int sample_rate) {
  SF_INFO info{};
  info.samplerate = sample_rate;
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

  SNDFILE *f = sf_open(path.c_str(), SFM_WRITE, &info);
  if (!f) {
    throw std::runtime_error("nao foi possivel criar WAV: " + path + " (" + sf_strerror(nullptr) + ")");
  }

  sf_count_t written = sf_write_short(f, pcm.data(), static_cast<sf_count_t>(pcm.size()));
  sf_close(f);

  if (written != static_cast<sf_count_t>(pcm.size())) {
    throw std::runtime_error("escrita incompleta em: " + path);
  }
}

} // namespace akai2sfz
