#include "akai2sfz/emu_filesystem.hpp"

#include <cstring>
#include <stdexcept>

namespace akai2sfz {

using namespace emu_raw;

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

} // namespace

bool looks_like_emu(const BlockDevice &dev) {
  if (dev.block_size() != kBlockSize) return false;
  if (dev.block_count() < 1) return false;
  std::vector<std::uint8_t> block(kBlockSize);
  try {
    dev.read_blocks(0, 1, block.data());
  } catch (const std::exception &) {
    return false;
  }
  return std::memcmp(block.data() + kSb_Signature, kSbSignatureText, 4) == 0;
}

EmuDisk::EmuDisk(const BlockDevice &dev) : dev_(dev) {
  if (!looks_like_emu(dev)) {
    throw std::runtime_error("imagem nao tem assinatura de disco E-mu ('EMU3')");
  }
  std::vector<std::uint8_t> sb(kBlockSize);
  dev_.read_blocks(0, 1, sb.data());

  start_root_block_ = le32(sb.data() + kSb_StartRootBlock);
  root_blocks_ = le32(sb.data() + kSb_RootBlocks);
  start_dir_content_block_ = le32(sb.data() + kSb_StartDirContentBlock);
  dir_content_blocks_ = le32(sb.data() + kSb_DirContentBlocks);
  start_cluster_list_block_ = le32(sb.data() + kSb_StartClusterListBlock);
  cluster_list_blocks_ = le32(sb.data() + kSb_ClusterListBlocks);
  start_data_block_ = le32(sb.data() + kSb_StartDataBlock);

  std::uint8_t shift_extra = sb[kSb_ClusterSizeShiftByte];
  std::size_t cluster_size = std::size_t{1} << (15 + shift_extra);
  blocks_per_cluster_ = cluster_size / kBlockSize;

  cluster_list_bytes_.resize(cluster_list_blocks_ * kBlockSize);
  if (cluster_list_blocks_ > 0) {
    dev_.read_blocks(start_cluster_list_block_, static_cast<std::size_t>(cluster_list_blocks_),
                      cluster_list_bytes_.data());
  }
}

std::uint16_t EmuDisk::cluster_next(std::uint16_t cluster) const {
  std::size_t offset = static_cast<std::size_t>(cluster) * 2;
  if (offset + 2 > cluster_list_bytes_.size()) {
    throw std::runtime_error("indice de cluster E-mu fora do limite da lista de clusters");
  }
  return le16(cluster_list_bytes_.data() + offset);
}

std::vector<std::uint8_t> EmuDisk::read_dentry_block(std::uint64_t block) const {
  std::vector<std::uint8_t> buf(kBlockSize);
  dev_.read_blocks(block, 1, buf.data());
  return buf;
}

std::vector<EmuFolder> EmuDisk::list_folders() const {
  std::vector<EmuFolder> result;

  for (std::uint64_t rb = 0; rb < root_blocks_; ++rb) {
    auto buf = read_dentry_block(start_root_block_ + rb);
    for (std::size_t i = 0; i < kEntriesPerBlock; ++i) {
      const std::uint8_t *e = buf.data() + i * kDentrySize;
      std::uint8_t id = e[kDentry_Id];
      if (id != kDType1 && id != kDType2) continue; // slot livre/nao-pasta

      EmuFolder folder;
      folder.name = ascii_trim(e + kDentry_Name, kDentry_NameLen);

      const std::uint8_t *block_list = e + kDentry_Union + kDattrs_BlockList;
      for (std::size_t b = 0; b < kBlocksPerDir; ++b) {
        std::uint16_t v = le16(block_list + b * 2);
        if (v == 0 || v == 0xFFFF) continue; // livre
        folder.content_blocks.push_back(v);
      }

      if (folder.content_blocks.empty()) continue; // pasta "virgem" (ex.: "New Folder" sem uso)
      result.push_back(std::move(folder));
    }
  }

  return result;
}

std::vector<EmuFileEntry> EmuDisk::list_files(const EmuFolder &folder) const {
  std::vector<EmuFileEntry> result;

  for (std::uint16_t block : folder.content_blocks) {
    auto buf = read_dentry_block(block);
    for (std::size_t i = 0; i < kEntriesPerBlock; ++i) {
      const std::uint8_t *e = buf.data() + i * kDentrySize;
      const std::uint8_t *fa = e + kDentry_Union;

      std::uint8_t type = fa[kFattrs_Type];
      std::uint16_t clusters = le16(fa + kFattrs_Clusters);
      if (clusters == 0) continue; // slot livre
      if (type != kFType_Std && type != kFType_Upd && type != kFType_Sys) continue;

      EmuFileEntry entry;
      entry.name = ascii_trim(e + kDentry_Name, kDentry_NameLen);
      entry.start_cluster = le16(fa + kFattrs_StartCluster);
      entry.clusters = clusters;
      entry.blocks = le16(fa + kFattrs_Blocks);
      entry.bytes_in_last_block = le16(fa + kFattrs_Bytes);
      entry.type = type;
      entry.props.assign(reinterpret_cast<const char *>(fa + kFattrs_Props), kFattrsPropsLen);
      result.push_back(std::move(entry));
    }
  }

  return result;
}

std::vector<std::uint8_t> EmuDisk::read_file(const EmuFileEntry &entry) const {
  std::uint64_t total_bytes = entry.byte_size(blocks_per_cluster_);
  std::vector<std::uint8_t> out;
  out.reserve(total_bytes);

  std::uint16_t cluster = entry.start_cluster;
  std::uint16_t clusters_read = 0;
  std::vector<std::uint8_t> cluster_buf(blocks_per_cluster_ * kBlockSize);

  while (clusters_read < entry.clusters && out.size() < total_bytes) {
    std::uint64_t phys_block = start_data_block_
        + static_cast<std::uint64_t>(cluster - 1) * blocks_per_cluster_;
    dev_.read_blocks(phys_block, blocks_per_cluster_, cluster_buf.data());

    std::size_t take = std::min<std::size_t>(cluster_buf.size(), total_bytes - out.size());
    out.insert(out.end(), cluster_buf.begin(), cluster_buf.begin() + static_cast<long>(take));

    ++clusters_read;
    std::uint16_t next = cluster_next(cluster);
    if (next == kLastFileCluster || next == kFreeCluster) break;
    cluster = next;
  }

  out.resize(total_bytes, 0); // preenche com zero se a cadeia terminou cedo (nao deveria acontecer)
  return out;
}

} // namespace akai2sfz
