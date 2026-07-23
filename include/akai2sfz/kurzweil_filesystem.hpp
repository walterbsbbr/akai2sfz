#pragma once
// Filesystem dos CDs Kurzweil K2000/K2500/K2600: FAT16 padrao, sem particoes
// multiplas, sem extensao proprietaria. Ver kurzweil_raw_format.hpp.
//
// `open_kurzweil_cd_image()` e um abridor de container SEPARADO do
// `open_cd_image()` do lado Akai: aquele usa scan_partitions() (magic Akai)
// como oraculo pra escolher entre setor cru/cooked, e um block_size fixo de
// 8192 (kHdBlockSize) incompativel com a granularidade de 512 B do FAT16.
// Aqui o `BlockDevice` e sempre aberto com block_size=2048 (o tamanho
// "cooked" natural de um setor de CD-ROM MODE1) e KurzweilDisk faz sua
// propria leitura endereçada por byte por cima disso -- mesmo padrao de
// `RolandDisk::read_bytes`.

#include "akai2sfz/image.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace akai2sfz {

// Abre uma imagem de CD Kurzweil (.iso plano, ou .cue+.bin MODE1/2352 real)
// como um BlockDevice de block_size=2048. Nao faz auto-deteccao de MDF/NRG
// (fora de escopo aqui -- ver README); .bin sem .cue ao lado cai para
// layout plano.
std::unique_ptr<BlockDevice> open_kurzweil_cd_image(const std::string &path);

// true se o volume (aberto com o BlockDevice acima) tiver um boot sector
// FAT16 valido: bytes/setor==512 e media descriptor==0xF8. Tolera MBR
// opcional (o volume pode comecar direto no boot sector, sem particao).
bool looks_like_kurzweil(const BlockDevice &dev);

struct KurzweilDirEntry {
  std::string name;   // "NOME.EXT" reconstruido (8.3, sem VFAT/LFN)
  bool is_directory = false;
  std::uint16_t start_cluster = 0;
  std::uint32_t size = 0; // bytes; 0 para diretorios
};

// Disco Kurzweil aberto (boot sector validado e geometria FAT16 decodificada).
class KurzweilDisk {
public:
  explicit KurzweilDisk(const BlockDevice &dev);

  std::vector<KurzweilDirEntry> list_root() const;
  // `dir` deve ter is_directory()==true (de list_root() ou list_directory()).
  std::vector<KurzweilDirEntry> list_directory(const KurzweilDirEntry &dir) const;

  // Extrai o conteudo completo do arquivo (cadeia de clusters via FAT,
  // recortado para entry.size bytes exatos).
  std::vector<std::uint8_t> read_file(const KurzweilDirEntry &entry) const;

private:
  const BlockDevice &dev_;

  std::uint64_t partition_start_sector_ = 0; // em setores FAT de 512 B, relativo ao inicio da imagem
  std::uint64_t fat_start_ = 0;
  std::uint64_t root_start_ = 0;
  std::uint64_t root_entries_ = 0;
  std::uint64_t root_dir_sectors_ = 0;
  std::size_t sectors_per_cluster_ = 0;
  std::uint64_t cluster_base_ = 0; // cluster2sector(2) == cluster_base_ + 2*sectors_per_cluster_

  std::vector<std::uint16_t> fat_; // 1a copia da FAT, carregada inteira

  std::vector<std::uint8_t> read_bytes(std::uint64_t byte_offset, std::size_t len) const;
  std::uint64_t cluster_to_sector(std::uint16_t cluster) const {
    return cluster_base_ + static_cast<std::uint64_t>(cluster) * sectors_per_cluster_;
  }
  std::uint16_t fat_at(std::uint16_t cluster) const;

  // Le a cadeia de clusters inteira a partir de `start_cluster` ate o fim de
  // cadeia (diretorios) ou ate `max_bytes` (arquivos, que tem tamanho
  // declarado no dentry).
  std::vector<std::uint8_t> read_cluster_chain(std::uint16_t start_cluster,
                                                std::uint64_t max_bytes) const;
  std::vector<KurzweilDirEntry> parse_dir_entries(const std::vector<std::uint8_t> &buf) const;
};

} // namespace akai2sfz
