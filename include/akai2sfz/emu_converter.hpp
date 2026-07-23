#pragma once
// Orquestracao E-mu: acha um bank numa pasta, resolve um preset pelo nome,
// extrai/converte os samples referenciados e escreve o SFZ -- reaproveita
// ConvertResult/SfzRegion/write_sfz do lado Akai (ja formato-agnosticos),
// mesmo padrao do roland_converter.cpp.

#include "akai2sfz/converter.hpp"
#include "akai2sfz/emu_filesystem.hpp"

#include <string>

namespace akai2sfz {

// Converte o preset `preset_name` dentro do bank `bank_name` (arquivo) da
// pasta `folder_name` para SFZ + WAV em `output_dir` (criado se nao existir).
ConvertResult convert_emu_preset(const EmuDisk &disk, const std::string &folder_name,
                                  const std::string &bank_name, const std::string &preset_name,
                                  const std::string &output_dir);

} // namespace akai2sfz
