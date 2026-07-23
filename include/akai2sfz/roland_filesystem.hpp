#pragma once
// Filesystem Roland S-750/S-760/S-770: ID area, FAT, 5 diretorios planos e
// 5 areas de parametro. Ao contrario do Akai, uma imagem Roland tem um
// unico conjunto (sem particoes multiplas).

#include "akai2sfz/image.hpp"
#include "akai2sfz/roland_raw_format.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace akai2sfz {

// true se o bloco 0 (lido com block_size=512) tiver a assinatura Roland
// ('S770 MR25A' em 0x04) -- mesma assinatura para S-750/S-760/S-770.
bool looks_like_roland(const BlockDevice &dev);

struct RolandDirEntry {
  std::size_t index = 0; // 0-based dentro do diretorio do seu tipo
  std::string name;
  roland_raw::FileType type = roland_raw::FileType::Volume;
  std::uint16_t fat_entry = 0;   // so Sample: cluster inicial na FAT
  std::uint16_t capacity = 0;    // so Sample: tamanho em clusters

  bool is_virginal() const;
  bool is_deleted() const;
  bool is_active() const { return !is_virginal() && !is_deleted(); }
};

// Disco Roland aberto (ID area validada). `dev` deve ter block_size=512.
class RolandDisk {
public:
  explicit RolandDisk(const BlockDevice &dev);

  std::string drive_name() const;
  std::uint32_t capacity_blocks() const;
  std::uint16_t file_count(roland_raw::FileType type) const; // do ID area (informativo)

  RolandDirEntry read_dir_entry(roland_raw::FileType type, std::size_t index) const;
  std::vector<RolandDirEntry> list_active(roland_raw::FileType type) const;

  // Bytes crus do bloco de parametro do arquivo #index (0-based) do tipo
  // dado -- tamanho fixo conforme roland_raw::kParam*::entry_size.
  std::vector<std::uint8_t> read_param(roland_raw::FileType type, std::size_t index) const;

  // Le os dados de onda (wave) de um Sample seguindo a cadeia de FAT a
  // partir de entry.fat_entry, ate o fim de cadeia ou entry.capacity
  // clusters (o que vier primeiro).
  std::vector<std::uint8_t> read_sample_wave(const RolandDirEntry &entry) const;

private:
  const BlockDevice &dev_;
  std::vector<std::uint8_t> id_block_;

  std::vector<std::uint8_t> read_bytes(std::uint64_t byte_offset, std::size_t len) const;
  std::uint16_t fat_at(std::uint32_t cluster) const;
};

} // namespace akai2sfz
