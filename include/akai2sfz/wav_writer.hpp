#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace akai2sfz {

// Escreve PCM 16-bit mono como WAV usando libsndfile.
// Lanca std::runtime_error em caso de falha.
void write_wav_mono16(const std::string &path, const std::vector<std::int16_t> &pcm,
                       int sample_rate);

} // namespace akai2sfz
