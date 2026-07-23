#pragma once
// Orquestracao de ponta a ponta: acha um program S3000 dentro de um volume ja
// aberto, extrai os samples referenciados, converte para WAV e escreve o SFZ.
// Usado tanto pela CLI quanto pela GUI -- nao deve conter nada especifico de
// interface.

#include "akai2sfz/filesystem.hpp"

#include <string>
#include <vector>

namespace akai2sfz {

struct ConvertResult {
  bool success = false;
  std::string program_name;
  std::string sfz_path;
  std::vector<std::string> wav_paths;
  std::vector<std::string> warnings; // ex.: sample referenciado nao encontrado
  std::string error;                 // vazio se success==true
};

// Converte o program de nome `program_name` (sem extensao) dentro do volume
// `volume_name` para SFZ + WAV em `output_dir` (criado se nao existir).
// So suporta programs S3000 (.a3p) em M2 -- ver README para S1000.
ConvertResult convert_program(const OpenPartition &part, const std::string &volume_name,
                               const std::string &program_name, const std::string &output_dir);

// Sanitiza um nome Akai (pode ter espacos internos) para um nome de arquivo seguro.
std::string sanitize_filename(const std::string &name);

} // namespace akai2sfz
