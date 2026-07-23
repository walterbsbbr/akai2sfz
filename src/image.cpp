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

BlockDevice::BlockDevice(const std::string &path, std::size_t block_size)
    : fd_(-1), block_size_(block_size), file_size_(0) {
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
  return file_size_ / block_size_;
}

void BlockDevice::read_blocks(std::uint64_t start, std::size_t count, std::uint8_t *out) const {
  const std::size_t bytes = count * block_size_;
  const std::uint64_t offset = start * block_size_;
  if (offset + bytes > file_size_) {
    throw std::runtime_error("leitura fora dos limites da imagem");
  }
  std::size_t done = 0;
  while (done < bytes) {
    ssize_t n = ::pread(fd_, out + done, bytes - done, static_cast<off_t>(offset + done));
    if (n < 0) {
      throw std::runtime_error("erro de leitura na imagem");
    }
    if (n == 0) {
      throw std::runtime_error("fim de arquivo inesperado na imagem");
    }
    done += static_cast<std::size_t>(n);
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

} // namespace akai2sfz
