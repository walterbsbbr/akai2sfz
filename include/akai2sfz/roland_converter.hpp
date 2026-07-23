#pragma once
// Orquestracao Roland: acha um Patch pelo nome, resolve Partials/Samples,
// converte para WAV e escreve o SFZ -- reaproveita ConvertResult/SfzRegion/
// write_sfz do lado Akai (ja formato-agnosticos).

#include "akai2sfz/converter.hpp"
#include "akai2sfz/roland_filesystem.hpp"

#include <string>

namespace akai2sfz {

// Converte o Patch de nome `patch_name` (como aparece no diretorio Roland)
// para SFZ + WAV em `output_dir` (criado se nao existir).
ConvertResult convert_roland_patch(const RolandDisk &disk, const std::string &patch_name,
                                    const std::string &output_dir);

} // namespace akai2sfz
