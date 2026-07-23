#include "akai2sfz/roland_filesystem.hpp"

#include <cstring>
#include <stdexcept>

namespace akai2sfz {

using namespace roland_raw;

namespace {

std::uint16_t le16(const std::uint8_t *p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t le32(const std::uint8_t *p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
      | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::string ascii_trim(const std::uint8_t *p, std::size_t len) {
  std::string s(reinterpret_cast<const char *>(p), len);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
  return s;
}

const ParamAreaInfo &param_area_for(FileType type) {
  switch (type) {
    case FileType::Volume: return kParamVolume;
    case FileType::Performance: return kParamPerformance;
    case FileType::Patch: return kParamPatch;
    case FileType::Partial: return kParamPartial;
    case FileType::Sample: return kParamSample;
  }
  throw std::runtime_error("tipo de arquivo Roland invalido");
}

const DirAreaInfo &dir_area_for(FileType type) {
  switch (type) {
    case FileType::Volume: return kDirVolume;
    case FileType::Performance: return kDirPerformance;
    case FileType::Patch: return kDirPatch;
    case FileType::Partial: return kDirPartial;
    case FileType::Sample: return kDirSample;
  }
  throw std::runtime_error("tipo de arquivo Roland invalido");
}

} // namespace

bool looks_like_roland(const BlockDevice &dev) {
  if (dev.block_size() != kBlockSize) return false;
  if (dev.block_count() < 1) return false;
  std::vector<std::uint8_t> block(kBlockSize);
  try {
    dev.read_blocks(0, 1, block.data());
  } catch (const std::exception &) {
    return false;
  }
  return std::memcmp(block.data() + kId_Signature, kId_SignatureText, kId_SignatureLen) == 0;
}

bool RolandDirEntry::is_virginal() const {
  return name.empty(); // byte 0 do nome == 0x00 vira string vazia apos ascii_trim
}

bool RolandDirEntry::is_deleted() const {
  return !name.empty() && static_cast<std::uint8_t>(name[0]) == kNameDeleted;
}

RolandDisk::RolandDisk(const BlockDevice &dev) : dev_(dev), id_block_(kBlockSize) {
  if (!looks_like_roland(dev)) {
    throw std::runtime_error("imagem nao tem assinatura de disco Roland ('S770 MR25A')");
  }
  dev_.read_blocks(0, 1, id_block_.data());
}

std::string RolandDisk::drive_name() const {
  return ascii_trim(id_block_.data() + kId_DriveName, 16);
}

std::uint32_t RolandDisk::capacity_blocks() const {
  return le32(id_block_.data() + kId_Capacity);
}

std::uint16_t RolandDisk::file_count(FileType type) const {
  switch (type) {
    case FileType::Volume: return le16(id_block_.data() + kId_FileNumVolume);
    case FileType::Performance: return le16(id_block_.data() + kId_FileNumPerformance);
    case FileType::Patch: return le16(id_block_.data() + kId_FileNumPatch);
    case FileType::Partial: return le16(id_block_.data() + kId_FileNumPartial);
    case FileType::Sample: return le16(id_block_.data() + kId_FileNumSample);
  }
  return 0;
}

std::vector<std::uint8_t> RolandDisk::read_bytes(std::uint64_t byte_offset, std::size_t len) const {
  std::uint64_t block_start = byte_offset / kBlockSize;
  std::uint64_t block_end = (byte_offset + len + kBlockSize - 1) / kBlockSize; // exclusivo
  std::size_t nblocks = static_cast<std::size_t>(block_end - block_start);

  std::vector<std::uint8_t> buf(nblocks * kBlockSize);
  dev_.read_blocks(block_start, nblocks, buf.data());

  std::size_t skip = static_cast<std::size_t>(byte_offset - block_start * kBlockSize);
  return std::vector<std::uint8_t>(buf.begin() + static_cast<long>(skip),
                                    buf.begin() + static_cast<long>(skip + len));
}

RolandDirEntry RolandDisk::read_dir_entry(FileType type, std::size_t index) const {
  const DirAreaInfo &area = dir_area_for(type);
  if (index >= area.entry_count) {
    throw std::runtime_error("indice de diretorio Roland fora do limite");
  }
  std::uint64_t offset = area.start_block * kBlockSize + index * kDirEntrySize;
  auto raw = read_bytes(offset, kDirEntrySize);

  RolandDirEntry e;
  e.index = index;
  e.type = type;
  e.name = ascii_trim(raw.data() + kDirEntry_Name, kDirEntry_NameLen);
  e.fat_entry = le16(raw.data() + kDirEntry_FatEntry);
  e.capacity = le16(raw.data() + kDirEntry_Capacity);
  return e;
}

std::vector<RolandDirEntry> RolandDisk::list_active(FileType type) const {
  const DirAreaInfo &area = dir_area_for(type);
  std::vector<RolandDirEntry> result;
  for (std::size_t i = 0; i < area.entry_count; ++i) {
    RolandDirEntry e = read_dir_entry(type, i);
    if (e.is_active()) result.push_back(std::move(e));
  }
  return result;
}

std::vector<std::uint8_t> RolandDisk::read_param(FileType type, std::size_t index) const {
  const ParamAreaInfo &area = param_area_for(type);
  if (index >= area.entry_count) {
    throw std::runtime_error("indice de parametro Roland fora do limite");
  }
  std::uint64_t offset = area.start_block * kBlockSize + index * area.entry_size;
  return read_bytes(offset, area.entry_size);
}

std::uint16_t RolandDisk::fat_at(std::uint32_t cluster) const {
  auto raw = read_bytes(kFatStartBlock * kBlockSize + cluster * 2, 2);
  return le16(raw.data());
}

std::vector<std::uint8_t> RolandDisk::read_sample_wave(const RolandDirEntry &entry) const {
  if (entry.type != FileType::Sample) {
    throw std::runtime_error("read_sample_wave: entrada nao e do tipo Sample");
  }

  std::vector<std::uint8_t> out;
  out.reserve(static_cast<std::size_t>(entry.capacity) * kClusterBlocks * kBlockSize);

  std::uint32_t cluster = entry.fat_entry;
  std::uint32_t clusters_read = 0;

  while (clusters_read < entry.capacity) {
    std::uint64_t offset = kWaveDataStartBlock * kBlockSize
        + static_cast<std::uint64_t>(cluster) * kClusterBlocks * kBlockSize;
    auto chunk = read_bytes(offset, kClusterBlocks * kBlockSize);
    out.insert(out.end(), chunk.begin(), chunk.end());
    ++clusters_read;

    std::uint16_t next = fat_at(cluster);
    if (next >= kFatEndMin || next == kFatFree || next == kFatBad) break;
    cluster = next;
  }

  return out;
}

} // namespace akai2sfz
