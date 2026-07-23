#pragma once
// Acesso a blocos de uma imagem de disco/CD Akai, e varredura/validacao de
// particoes sampler (S1000/S3000/CD3000) dentro dela.
//
// M0 so suporta imagem plana (bytes crus, endereca por bloco a partir do
// offset 0 do arquivo). Containers reais (MDF/NRG/BIN+CUE, com seus proprios
// headers e possivel modo de setor 2352 bytes) ficam para M1 -- ver README.

#include <cstdint>
#include <string>
#include <vector>

namespace akai2sfz {

// Fluxo de bytes endereçavel por bloco de tamanho fixo, apoiado num arquivo comum.
class BlockDevice {
public:
  // Abre `path` somente leitura. Lanca std::runtime_error se nao conseguir abrir.
  explicit BlockDevice(const std::string &path, std::size_t block_size = 0x2000);
  ~BlockDevice();

  BlockDevice(const BlockDevice &) = delete;
  BlockDevice &operator=(const BlockDevice &) = delete;

  std::size_t block_size() const { return block_size_; }

  // Numero total de blocos completos disponiveis no arquivo.
  std::uint64_t block_count() const;

  // Le `count` blocos a partir do bloco `start` (offset absoluto no arquivo,
  // nao relativo a particao) para `out`, que deve ter espaco para
  // count * block_size() bytes. Lanca std::runtime_error em erro/EOF.
  void read_blocks(std::uint64_t start, std::size_t count, std::uint8_t *out) const;

private:
  int fd_;
  std::size_t block_size_;
  std::uint64_t file_size_;
};

// Uma particao sampler (S1000/S3000/CD3000) valida dentro da imagem.
struct Partition {
  std::uint64_t start_block; // bloco inicial na imagem (absoluto, em unidades de kHdBlockSize)
  std::uint32_t size_blocks; // tamanho declarado no header da particao
};

// Varre a imagem sequencialmente a partir do bloco 0, seguindo o mesmo
// algoritmo do akaitools original (Perl AkaiDisk::new / C akai_scan_disk):
// cada particao comeca onde a anterior termina, e o proprio header da
// particao informa seu tamanho em blocos. Cada candidata e validada pelo
// magic+checksum de akai_parthead_s antes de ser aceita; a varredura para
// no primeiro bloco invalido, no marcador de fim (kPartEndMark) ou no fim
// do arquivo.
std::vector<Partition> scan_partitions(const BlockDevice &dev);

// Letra de particao no estilo akaiutil/Akai (part[pi].letter = 'A' + pi em
// akaiutil.cc): particao 0 (0-based) = 'A', 1 = 'B', etc. Alem de 26
// particoes (nunca deve acontecer numa imagem de CD real) cai para "P<n>".
std::string partition_label(std::size_t index_0based);

} // namespace akai2sfz
