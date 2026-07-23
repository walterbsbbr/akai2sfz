#pragma once
// Filesystem de disco E-mu ("EMU3"): superbloco, cadeia de clusters
// ("FAT" de 16 bits) e diretorio de 2 niveis (pastas na raiz, arquivos numa
// pool de blocos compartilhada). Comum a EIII/EIIIx/ESI-32/EIV -- o
// conteudo binario de cada arquivo (bank EMU3-flat, bank E4B0/EOS, etc.) e
// tratado por camadas separadas (ver emu_format.hpp).
//
// Ao contrario do Akai, nao ha particoes multiplas dentro da imagem; ao
// contrario do Roland, PODE haver varias pastas de topo (ex.: "Default
// Folder", "New Folder") -- cada uma e uma "Volume" real no sentido do
// column-browser da GUI.

#include "akai2sfz/emu_raw_format.hpp"
#include "akai2sfz/image.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace akai2sfz {

// true se o bloco 0 (lido com block_size=512) tiver a assinatura "EMU3".
bool looks_like_emu(const BlockDevice &dev);

// Uma pasta de topo (entrada de diretorio-raiz).
struct EmuFolder {
  std::string name;
  std::vector<std::uint16_t> content_blocks; // ate 7, blocos da pool de diretorio
};

// Um arquivo dentro de uma pasta (bank, ou outro tipo de arquivo E-mu).
struct EmuFileEntry {
  std::string name;
  std::uint16_t start_cluster = 0;
  std::uint16_t clusters = 0;      // clusters alocados (cadeia)
  std::uint16_t blocks = 0;        // blocos de 512B validos no arquivo
  std::uint16_t bytes_in_last_block = 0; // bytes validos no ultimo bloco (0 = bloco cheio)
  std::uint8_t type = 0;
  std::string props; // 5 bytes crus (ex.: "\0E3B0" em bancos EIII reais)

  // Tamanho exato do arquivo em bytes: `clusters-1` clusters inteiros mais
  // `blocks` blocos validos e `bytes_in_last_block` bytes validos no ultimo
  // bloco do ULTIMO cluster (blocks/bytes descrevem so o cluster final, nao
  // o arquivo inteiro -- confirmado porque em toda entrada observada
  // `blocks` e sempre menor que blocks_per_cluster; um teste inicial com um
  // bank vazio de 1 cluster so nao distinguia as duas hipoteses, e um bank
  // real de varios samples so leu direito apos essa correcao -- ver git log).
  std::uint64_t byte_size(std::size_t blocks_per_cluster) const {
    if (clusters == 0) return 0;
    std::uint64_t full_clusters_bytes = static_cast<std::uint64_t>(clusters - 1) * blocks_per_cluster
        * emu_raw::kBlockSize;
    std::uint64_t last_cluster_bytes = 0;
    if (blocks > 0) {
      last_cluster_bytes = static_cast<std::uint64_t>(blocks - 1) * emu_raw::kBlockSize
          + (bytes_in_last_block == 0 ? emu_raw::kBlockSize : bytes_in_last_block);
    }
    return full_clusters_bytes + last_cluster_bytes;
  }
};

// Disco E-mu aberto (superbloco validado). `dev` deve ter block_size=512.
class EmuDisk {
public:
  explicit EmuDisk(const BlockDevice &dev);

  std::size_t blocks_per_cluster() const { return blocks_per_cluster_; }

  std::vector<EmuFolder> list_folders() const;
  std::vector<EmuFileEntry> list_files(const EmuFolder &folder) const;

  // Le o conteudo completo do arquivo, seguindo a cadeia de clusters a
  // partir de start_cluster ate o fim de cadeia ou byte_size() bytes (o que
  // vier primeiro), e recorta para o tamanho exato.
  std::vector<std::uint8_t> read_file(const EmuFileEntry &entry) const;

private:
  const BlockDevice &dev_;

  std::uint64_t start_root_block_ = 0;
  std::uint64_t root_blocks_ = 0;
  std::uint64_t start_dir_content_block_ = 0;
  std::uint64_t dir_content_blocks_ = 0;
  std::uint64_t start_cluster_list_block_ = 0;
  std::uint64_t cluster_list_blocks_ = 0;
  std::uint64_t start_data_block_ = 0;
  std::size_t blocks_per_cluster_ = 0;

  std::vector<std::uint8_t> cluster_list_bytes_; // cache: cluster_list_blocks_ * 512 bytes

  std::uint16_t cluster_next(std::uint16_t cluster) const;
  std::vector<std::uint8_t> read_dentry_block(std::uint64_t block) const;
};

} // namespace akai2sfz
