#pragma once
// Orquestracao Kurzweil: acha um arquivo .KRZ em qualquer lugar da arvore
// FAT16 (busca recursiva, sem distincao maiuscula/minuscula), resolve um
// Program pelo nome, percorre Layer->Keymap->Sample e escreve SFZ + WAV --
// reaproveita ConvertResult/SfzRegion/write_sfz/write_wav_mono16, mesmas
// funcoes usadas pelos lados Akai/Roland/E-mu.

#include "akai2sfz/converter.hpp"
#include "akai2sfz/kurzweil_filesystem.hpp"

#include <string>

namespace akai2sfz {

// Converte o Program `program_name` dentro do arquivo `krz_file_name` (ex.:
// "DRUMS1.KRZ", buscado recursivamente na arvore do disco) para SFZ + WAV
// em `output_dir` (criado se nao existir).
ConvertResult convert_krz_program(const KurzweilDisk &disk, const std::string &krz_file_name,
                                   const std::string &program_name, const std::string &output_dir);

} // namespace akai2sfz
