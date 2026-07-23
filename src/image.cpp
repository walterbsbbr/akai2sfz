#include "akai2sfz/image.hpp"
#include "akai2sfz/raw_format.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <cstring>
#include <stdexcept>
#include <vector>

namespace akai2sfz {

using namespace raw;

namespace {
SectorLayout flat_layout(std::size_t block_size) {
  SectorLayout l;
  l.base_byte_offset = 0;
  l.physical_sector_size = block_size;
  l.user_data_offset = 0;
  l.user_data_size = block_size;
  return l;
}

void pread_full(int fd, std::uint8_t *out, std::size_t bytes, std::uint64_t offset,
                 std::uint64_t file_size) {
  if (offset + bytes > file_size) {
    throw std::runtime_error("leitura fora dos limites da imagem");
  }
  std::size_t done = 0;
  while (done < bytes) {
    ssize_t n = ::pread(fd, out + done, bytes - done, static_cast<off_t>(offset + done));
    if (n < 0) {
      throw std::runtime_error("erro de leitura na imagem");
    }
    if (n == 0) {
      throw std::runtime_error("fim de arquivo inesperado na imagem");
    }
    done += static_cast<std::size_t>(n);
  }
}
} // namespace

BlockDevice::BlockDevice(const std::string &path, std::size_t block_size)
    : BlockDevice(path, block_size, flat_layout(block_size)) {}

BlockDevice::BlockDevice(const std::string &path, std::size_t block_size, SectorLayout layout)
    : fd_(-1), block_size_(block_size), file_size_(0), layout_(layout) {
  fd_ = ::open(path.c_str(), O_RDONLY);
  if (fd_ < 0) {
    throw std::runtime_error("nao foi possivel abrir a imagem: " + path);
  }
  struct stat st{};
  if (::fstat(fd_, &st) != 0) {
    ::close(fd_);
    throw std::runtime_error("nao foi possivel obter o tamanho de: " + path);
  }
  file_size_ = static_cast<std::uint64_t>(st.st_size);
}

BlockDevice::~BlockDevice() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

std::uint64_t BlockDevice::block_count() const {
  if (file_size_ <= layout_.base_byte_offset) return 0;
  std::uint64_t data_bytes = file_size_ - layout_.base_byte_offset;
  std::uint64_t logical_sectors = data_bytes / layout_.physical_sector_size;
  std::uint64_t sectors_per_block = block_size_ / layout_.user_data_size;
  return logical_sectors / sectors_per_block;
}

void BlockDevice::read_blocks(std::uint64_t start, std::size_t count, std::uint8_t *out) const {
  if (is_flat()) {
    // caminho original do M0: um unico pread contiguo.
    const std::size_t bytes = count * block_size_;
    const std::uint64_t offset = layout_.base_byte_offset + start * block_size_;
    pread_full(fd_, out, bytes, offset, file_size_);
    return;
  }

  // setor fisico != dados uteis (ex.: raw MODE1/2352): cada bloco Akai
  // (block_size_ bytes) e composto de varios setores logicos de
  // user_data_size bytes, cada um lido separadamente do meio do setor fisico.
  const std::size_t sectors_per_block = block_size_ / layout_.user_data_size;
  std::uint64_t logical_sector = start * sectors_per_block;

  for (std::size_t i = 0; i < count * sectors_per_block; ++i, ++logical_sector) {
    std::uint64_t offset = layout_.base_byte_offset + logical_sector * layout_.physical_sector_size
        + layout_.user_data_offset;
    pread_full(fd_, out + i * layout_.user_data_size, layout_.user_data_size, offset, file_size_);
  }
}

namespace {

std::uint16_t le16(const std::uint8_t *p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

// Valida o header de particao (akai_parthead_s) que comeca em `buf`
// (kPartheadSize bytes). Espelha akai_check_partheadmagic():
//   magic[i] == (i * 3333) & 0xFFFF, para i em [0, 98)
//   chksum == bsize + soma(magic[i])  (32bit, little-endian)
bool validate_parthead(const std::uint8_t *buf, std::uint32_t bsize) {
  std::uint32_t cs = bsize;
  const std::uint8_t *magic = buf + kParthead_Magic;
  for (std::size_t i = 0; i < kParthead_MagicCount; ++i) {
    std::uint16_t m = le16(magic + i * 2);
    std::uint16_t expect = static_cast<std::uint16_t>((i * kParthead_MagicVal) & 0xFFFF);
    if (m != expect) {
      return false;
    }
    cs += m;
  }
  const std::uint8_t *ck = buf + kParthead_Chksum;
  std::uint32_t chksum = static_cast<std::uint32_t>(ck[0])
      | (static_cast<std::uint32_t>(ck[1]) << 8)
      | (static_cast<std::uint32_t>(ck[2]) << 16)
      | (static_cast<std::uint32_t>(ck[3]) << 24);
  return chksum == cs;
}

} // namespace

std::vector<Partition> scan_partitions(const BlockDevice &dev) {
  std::vector<Partition> result;
  std::vector<std::uint8_t> buf(kPartheadSize);

  std::uint64_t block = 0;
  const std::uint64_t total_blocks = dev.block_count();

  while (block + kPartheadBlocks <= total_blocks) {
    dev.read_blocks(block, kPartheadBlocks, buf.data());

    std::uint16_t size_field = le16(buf.data() + kParthead_Size);
    if (size_field == 0 || size_field == kPartEndMark) {
      break; // sem mais particoes
    }
    if (!validate_parthead(buf.data(), size_field)) {
      break; // nao e um header de particao sampler valido: para a varredura
    }

    Partition part;
    part.start_block = block;
    part.size_blocks = size_field;
    result.push_back(part);

    block += size_field;
  }

  return result;
}

std::string partition_label(std::size_t index_0based) {
  if (index_0based < 26) {
    return std::string(1, static_cast<char>('A' + index_0based));
  }
  return "P" + std::to_string(index_0based + 1);
}

} // namespace akai2sfz
