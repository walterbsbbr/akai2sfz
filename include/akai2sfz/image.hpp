#pragma once
// Acesso a blocos de uma imagem de disco/CD Akai, e varredura/validacao de
// particoes sampler (S1000/S3000/CD3000) dentro dela.
//
// M1: alem de imagem plana (ISO simples, comportamento original do M0),
// `open_cd_image()` sabe ler BIN+CUE e NRG reais -- setores brutos de CD
// (2352 bytes: 16 de sync/header + 2048 de dados uteis + 288 de ECC) e o
// pregap de 150 frames que alguns containers embutem no arquivo. Validado
// contra TZIFFXAK.bin+.cue (raw MODE1/2352) e contra um .nrg real (Nero v2
// "NER5", setor cooked de 2048 com pregap de 150 frames) -- ver git log e
// README. MDF usa deteccao automatica (mesma logica de sync-pattern), sem
// amostra real para validar -- ver README.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace akai2sfz {

// Como mapear um bloco logico (2048 bytes de dados uteis) para bytes fisicos
// no arquivo. O caso "plano" (physical_sector_size == user_data_size, offset
// 0) e o comportamento original do M0 -- sem isso, ISO simples continua
// funcionando exatamente como antes.
struct SectorLayout {
  std::uint64_t base_byte_offset = 0;     // onde comeca a track de dados (LBA 0)
  std::size_t physical_sector_size = 0x2000; // tamanho fisico de cada setor no arquivo
  std::size_t user_data_offset = 0;       // offset dos dados uteis dentro do setor fisico
  std::size_t user_data_size = 0x2000;    // tamanho dos dados uteis por setor
};

// Fluxo de bytes endereçavel por bloco de tamanho fixo, apoiado num arquivo comum.
class BlockDevice {
public:
  // Abre `path` somente leitura com layout plano (comportamento do M0).
  // Lanca std::runtime_error se nao conseguir abrir.
  explicit BlockDevice(const std::string &path, std::size_t block_size = 0x2000);

  // Abre `path` com um SectorLayout explicito (usado por open_cd_image()
  // para BIN+CUE/NRG com setores brutos e/ou pregap).
  BlockDevice(const std::string &path, std::size_t block_size, SectorLayout layout);

  ~BlockDevice();

  BlockDevice(const BlockDevice &) = delete;
  BlockDevice &operator=(const BlockDevice &) = delete;

  std::size_t block_size() const { return block_size_; }

  // Numero total de blocos completos disponiveis (considera o SectorLayout).
  std::uint64_t block_count() const;

  // Le `count` blocos a partir do bloco `start` (indice absoluto na imagem,
  // nao relativo a particao) para `out`, que deve ter espaco para
  // count * block_size() bytes. Lanca std::runtime_error em erro/EOF.
  void read_blocks(std::uint64_t start, std::size_t count, std::uint8_t *out) const;

private:
  int fd_;
  std::size_t block_size_;
  std::uint64_t file_size_;
  SectorLayout layout_;

  bool is_flat() const {
    return layout_.physical_sector_size == layout_.user_data_size && layout_.user_data_offset == 0;
  }
};

// Abre uma imagem de CD Akai, detectando o container pela extensao (e,
// quando aplicavel, pelo conteudo real do arquivo) e devolvendo um
// BlockDevice ja configurado com o SectorLayout correto:
//  - .iso (ou desconhecido): layout plano, igual ao M0.
//  - .cue: le a cue sheet de verdade (FILE/TRACK MODEx/yyyy/INDEX 01),
//    resolve o .bin (tolera diferenca de maiuscula/minuscula no nome).
//  - .bin sem .cue ao lado, .mdf: deteccao automatica por sync-pattern de CD
//    (00 FF x10 00 a cada 2352 bytes => raw; ausente => assume 2048 plano).
//  - .nrg: le o footer NER5/chunks (CUEX) para achar o pregap e o LBA 0 da
//    track 1, e usa deteccao por sync-pattern para o tamanho do setor.
// Lanca std::runtime_error se o arquivo nao puder ser aberto ou o container
// nao puder ser entendido.
std::unique_ptr<BlockDevice> open_cd_image(const std::string &path);

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
