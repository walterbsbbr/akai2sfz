#include "akai2sfz/filesystem.hpp"
#include "akai2sfz/codec.hpp"

#include <stdexcept>

namespace akai2sfz {

using namespace raw;

namespace {

std::uint16_t le16(const std::uint8_t *p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t le24(const std::uint8_t *p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
      | (static_cast<std::uint32_t>(p[2]) << 16);
}

// Blocos reservados para o header da particao (o header ocupa os 3 primeiros
// blocos da particao); espelha pp->bsyssize para particoes HD em akaiutil.
constexpr std::uint32_t kSystemBlocks = static_cast<std::uint32_t>(kPartheadBlocks);

bool is_valid_data_block(std::uint16_t blk, std::uint32_t part_size_blocks) {
  if (blk == kFatFree || blk == kFatBad || blk == kFatSys
      || blk == kFatDirEnd3000 || blk == kFatFileEnd) {
    return false;
  }
  if (blk >= part_size_blocks || blk < kSystemBlocks) {
    return false;
  }
  return true;
}

struct FileTypeInfo {
  std::uint8_t code;
  const char *ext;
  const char *name;
};

// Espelha o mapeamento usado por akailist / akai_wrapper.py (FILE_TYPES/FILE_EXTENSIONS).
constexpr FileTypeInfo kFileTypes[] = {
    {100, "adr", "DRUM SETTING"},
    {112, "a1p", "S1000 PROGRAM"},
    {113, "aql", "QLIST"},
    {115, "a1s", "S1000 SAMPLE"},
    {120, "afx", "EFFECTS"},
    {237, "aml", "MULTI"},
    {240, "a3p", "S3000 PROGRAM"},
    {243, "a3s", "S3000 SAMPLE"},
};

const FileTypeInfo *find_file_type(std::uint8_t type) {
  for (const auto &t : kFileTypes) {
    if (t.code == type) return &t;
  }
  return nullptr;
}

} // namespace

const char *file_type_extension(std::uint8_t type) {
  if (const auto *t = find_file_type(type)) return t->ext;
  return "bin";
}

const char *file_type_name(std::uint8_t type) {
  if (const auto *t = find_file_type(type)) return t->name;
  return "UNKNOWN";
}

OpenPartition::OpenPartition(const BlockDevice &dev, const Partition &part)
    : dev_(dev), part_(part), head_(kPartheadSize) {
  dev_.read_blocks(part_.start_block, kPartheadBlocks, head_.data());
}

void OpenPartition::read_partition_blocks(std::uint64_t rel_block, std::size_t count,
                                           std::uint8_t *out) const {
  dev_.read_blocks(part_.start_block + rel_block, count, out);
}

std::uint16_t OpenPartition::fat_at(std::uint32_t block) const {
  if (block >= kParthead_FatMaxEntries) {
    throw std::runtime_error("indice de FAT fora do limite");
  }
  return le16(head_.data() + kParthead_Fat + block * 2);
}

raw::VolType OpenPartition::volume_type(std::size_t index) const {
  const std::uint8_t *entry = head_.data() + kParthead_Vol + index * kRootEntrySize;
  return static_cast<raw::VolType>(entry[kRootEntry_Type]);
}

std::string OpenPartition::volume_name(std::size_t index) const {
  const std::uint8_t *entry = head_.data() + kParthead_Vol + index * kRootEntrySize;
  return akai_name_to_ascii(entry + kRootEntry_Name, kNameLen);
}

std::vector<FileEntry> list_files(const OpenPartition &part, std::size_t volume_index) {
  std::vector<FileEntry> result;

  raw::VolType vtype = part.volume_type(volume_index);
  if (vtype != raw::VolType::S1000 && vtype != raw::VolType::S3000
      && vtype != raw::VolType::Cd3000) {
    return result; // volume inativo ou tipo fora de escopo (S900/etc.)
  }

  const std::uint8_t *root_entry =
      part.head_.data() + kParthead_Vol + volume_index * kRootEntrySize;
  std::uint16_t dirblk0 = le16(root_entry + kRootEntry_Start);
  if (!is_valid_data_block(dirblk0, part.part_.size_blocks)) {
    return result;
  }

  bool is_s3000 = (vtype == raw::VolType::S3000 || vtype == raw::VolType::Cd3000);
  std::uint16_t dirblk1 = 0;
  if (is_s3000) {
    dirblk1 = part.fat_at(dirblk0);
    if (!is_valid_data_block(dirblk1, part.part_.size_blocks)) {
      // Mesmo fallback defensivo do akaiutil original: sem bloco 1 valido,
      // trata como volume S1000 (1 bloco, 126 entradas).
      is_s3000 = false;
    }
  }

  std::vector<std::uint8_t> block0(kHdBlockSize);
  part.read_partition_blocks(dirblk0, 1, block0.data());

  std::vector<std::uint8_t> block1;
  if (is_s3000) {
    block1.resize(kHdBlockSize);
    part.read_partition_blocks(dirblk1, 1, block1.data());
  }

  std::size_t fimax = is_s3000 ? kVoldirEntries_S3000Hd : kVoldirEntries_S1000Hd;
  for (std::size_t i = 0; i < fimax; ++i) {
    const std::uint8_t *e;
    if (i < kVoldirEntries1Blk) {
      e = block0.data() + i * kFileEntrySize;
    } else {
      e = block1.data() + (i - kVoldirEntries1Blk) * kFileEntrySize;
    }

    std::uint8_t type = e[kFileEntry_Type];
    if (type == kFileTypeFree) continue;

    FileEntry fe;
    fe.name = akai_name_to_ascii(e + kFileEntry_Name, kNameLen);
    fe.type = type;
    fe.extension = file_type_extension(type);
    fe.size = le24(e + kFileEntry_Size);
    fe.start = le16(e + kFileEntry_Start);
    result.push_back(std::move(fe));
  }

  return result;
}

std::vector<std::uint8_t> extract_file(const OpenPartition &part, const FileEntry &file) {
  std::vector<std::uint8_t> out;
  out.reserve(file.size);

  std::uint16_t blk = file.start;
  std::uint32_t remaining = file.size;
  std::vector<std::uint8_t> chunk(kHdBlockSize);

  while (remaining > 0) {
    std::uint32_t take = remaining < kHdBlockSize ? remaining : static_cast<std::uint32_t>(kHdBlockSize);

    if (!is_valid_data_block(blk, part.part_.size_blocks)) {
      throw std::runtime_error("cadeia de FAT invalida em '" + file.name + "'");
    }

    part.read_partition_blocks(blk, 1, chunk.data());
    out.insert(out.end(), chunk.begin(), chunk.begin() + take);

    blk = part.fat_at(blk);
    remaining -= take;
  }

  return out;
}

} // namespace akai2sfz
