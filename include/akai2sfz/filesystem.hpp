#pragma once
// Enumeracao de volumes e arquivos dentro de uma particao sampler, e extracao
// de arquivos via cadeia de FAT.
//
// Algoritmo espelha akai_get_vol()/akai_read_voldir()/akai_read_file() em
// akaiutil.cc: root directory tem ate 100 volumes; volume S1000 ocupa 1 bloco
// (126 entradas), volume S3000/CD3000 ocupa 2 blocos (510 entradas) -- o
// segundo bloco e localizado seguindo a FAT a partir do primeiro, nao por
// contiguidade.

#include "akai2sfz/image.hpp"
#include "akai2sfz/raw_format.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace akai2sfz {

// Particao aberta: header + FAT carregados em memoria, pronta para enumerar
// volumes/arquivos ou extrair dados.
class OpenPartition {
public:
  OpenPartition(const BlockDevice &dev, const Partition &part);

  raw::VolType volume_type(std::size_t index) const;
  std::string volume_name(std::size_t index) const;
  std::size_t volume_count() const { return raw::kParthead_VolCount; }

private:
  friend struct Volume;
  friend std::vector<struct FileEntry> list_files(const OpenPartition &, std::size_t);
  friend std::vector<std::uint8_t> extract_file(const OpenPartition &, const struct FileEntry &);

  const BlockDevice &dev_;
  Partition part_;
  std::vector<std::uint8_t> head_; // kPartheadSize bytes (akai_parthead_s)

  std::uint16_t fat_at(std::uint32_t block) const;
  void read_partition_blocks(std::uint64_t rel_block, std::size_t count, std::uint8_t *out) const;
};

struct FileEntry {
  std::string name;       // nome ASCII, sem espacos a direita
  std::string extension;  // "a1p"/"a1s"/"a3p"/"a3s"/... deduzida do type
  std::uint8_t type = 0;  // codigo de tipo bruto (ver akailist: 112=S1000 PROGRAM, etc.)
  std::uint32_t size = 0; // bytes
  std::uint16_t start = 0; // bloco inicial dentro da particao
};

// Lista os arquivos do volume `index` (0-based) dentro da particao aberta.
// Volumes inativos ou de tipo desconhecido retornam lista vazia.
std::vector<FileEntry> list_files(const OpenPartition &part, std::size_t volume_index);

// Le o conteudo completo de um arquivo seguindo sua cadeia de FAT.
std::vector<std::uint8_t> extract_file(const OpenPartition &part, const FileEntry &file);

// Mapa tipo bruto -> extensao/descricao, igual ao usado por akailist/akai_wrapper.py.
const char *file_type_extension(std::uint8_t type);
const char *file_type_name(std::uint8_t type);

} // namespace akai2sfz
