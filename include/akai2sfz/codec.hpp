#pragma once
// Codec de nomes Akai <-> ASCII.
//
// Tabela conferida contra akai2ascii()/ascii2akai() em akaiutil.cc e contra
// Synth::AkaiDisk::akai_to_ascii()/ascii_to_akai() (tr/\x00-\x28/0-9 A-Z#+-./)
// -- as duas fontes coincidem exatamente.

#include <cstdint>
#include <string>

namespace akai2sfz {

// Decodifica um unico caractere Akai (0-40) para ASCII.
char akai_to_ascii_char(std::uint8_t c);

// Codifica um caractere ASCII para o alfabeto Akai (0-40).
std::uint8_t ascii_to_akai_char(char a);

// Decodifica um nome Akai de tamanho fixo (12 bytes) para uma string ASCII,
// removendo espacos a direita.
std::string akai_name_to_ascii(const std::uint8_t *raw, std::size_t len = 12);

} // namespace akai2sfz
