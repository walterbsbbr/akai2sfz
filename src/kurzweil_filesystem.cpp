#include "akai2sfz/kurzweil_filesystem.hpp"
#include "akai2sfz/kurzweil_raw_format.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace akai2sfz {

using namespace kurzweil_raw;
namespace fs = std::filesystem;

namespace {

constexpr std::size_t kCdUserDataSize = 0x800; // 2048, setor de CD "cooked"

std::uint16_t le16(const std::uint8_t *p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t le32(const std::uint8_t *p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
      | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

std::string file_extension_lower(const std::string &path) {
  fs::path p(path);
  std::string ext = p.extension().string();
  if (!ext.empty() && ext.front() == '.') ext.erase(ext.begin());
  return to_lower(ext);
}

fs::path resolve_case_insensitive(const fs::path &dir, const std::string &name) {
  fs::path direct = dir / name;
  if (fs::exists(direct)) return direct;
  std::string wanted_lower = to_lower(name);
  if (fs::exists(dir) && fs::is_directory(dir)) {
    for (const auto &entry : fs::directory_iterator(dir)) {
      if (to_lower(entry.path().filename().string()) == wanted_lower) return entry.path();
    }
  }
  throw std::runtime_error("arquivo referenciado nao encontrado: " + name);
}

std::vector<std::string> split_ws(const std::string &line) {
  std::istringstream iss(line);
  std::vector<std::string> out;
  std::string tok;
  while (iss >> tok) out.push_back(tok);
  return out;
}

std::uint32_t msf_to_frames(int mm, int ss, int ff) {
  return static_cast<std::uint32_t>((mm * 60 + ss) * 75 + ff);
}

// Le uma .cue simples (1 FILE, 1a TRACK/INDEX 01) e devolve o SectorLayout
// correspondente -- mesma leitura de campos do lado Akai (cd_container.cpp),
// mas sem a dependencia de scan_partitions()/block_size 8192.
std::unique_ptr<BlockDevice> open_via_cue(const std::string &cue_path) {
  std::ifstream f(cue_path);
  if (!f) throw std::runtime_error("nao foi possivel abrir a cue sheet: " + cue_path);

  std::string bin_name;
  int mode = 1;
  std::size_t sector_size = 2352;
  bool have_track = false;
  int index01_mm = 0, index01_ss = 0, index01_ff = 0;
  bool have_index01 = false;

  std::string line;
  while (std::getline(f, line)) {
    auto parts = split_ws(line);
    if (parts.empty()) continue;

    if (bin_name.empty() && parts[0] == "FILE") {
      auto first_quote = line.find('"');
      auto last_quote = line.rfind('"');
      if (first_quote != std::string::npos && last_quote > first_quote) {
        bin_name = line.substr(first_quote + 1, last_quote - first_quote - 1);
      }
      continue;
    }
    if (!have_track && parts[0] == "TRACK" && parts.size() >= 3) {
      auto slash = parts[2].find('/');
      if (slash != std::string::npos) {
        std::string mode_str = parts[2].substr(0, slash);
        std::string size_str = parts[2].substr(slash + 1);
        if (mode_str.size() >= 5 && mode_str.substr(0, 4) == "MODE") mode = mode_str[4] - '0';
        try {
          sector_size = static_cast<std::size_t>(std::stoul(size_str));
        } catch (...) {
        }
        have_track = true;
      }
      continue;
    }
    if (have_track && !have_index01 && parts[0] == "INDEX" && parts.size() >= 3
        && parts[1] == "01") {
      int mm = 0, ss = 0, ff = 0;
      if (std::sscanf(parts[2].c_str(), "%d:%d:%d", &mm, &ss, &ff) == 3) {
        index01_mm = mm;
        index01_ss = ss;
        index01_ff = ff;
        have_index01 = true;
        break;
      }
    }
  }

  if (bin_name.empty() || !have_track) {
    throw std::runtime_error("cue sheet sem FILE/TRACK reconhecivel: " + cue_path);
  }

  fs::path bin_path = resolve_case_insensitive(fs::path(cue_path).parent_path(), bin_name);

  std::uint64_t base_offset =
      have_index01 ? static_cast<std::uint64_t>(msf_to_frames(index01_mm, index01_ss, index01_ff))
              * sector_size
                   : 0;

  SectorLayout layout;
  layout.base_byte_offset = base_offset;
  layout.physical_sector_size = sector_size;
  layout.user_data_size = kCdUserDataSize;
  if (sector_size == kCdUserDataSize) {
    layout.user_data_offset = 0; // ja "cooked"
  } else if (sector_size == 2352 && mode == 2) {
    layout.user_data_offset = 24; // XA Mode 2 Form 1
  } else {
    layout.user_data_offset = 16; // MODE1/2352
  }

  return std::make_unique<BlockDevice>(bin_path.string(), kCdUserDataSize, layout);
}

// Os nomes de arquivo observados em CDs Kurzweil reais sao preenchidos com
// espaco a ESQUERDA (ex.: "  DRUMS1KRZ", nao "DRUMS1  KRZ" como o FAT 8.3
// classico costuma fazer) -- por isso o trim precisa cobrir os dois lados,
// nao so o fim como um FAT generico assumiria. Confirmado contra
// TZMSKRZ.bin/.cue real: sem esse trim duplo, nomes com menos de 8
// caracteres (ex. "DRUMS1") nunca batiam na busca.
std::string ascii_trim(const std::uint8_t *p, std::size_t len) {
  std::string s(reinterpret_cast<const char *>(p), len);
  while (!s.empty() && s.back() == ' ') s.pop_back();
  std::size_t start = 0;
  while (start < s.size() && s[start] == ' ') ++start;
  return s.substr(start);
}

} // namespace

std::unique_ptr<BlockDevice> open_kurzweil_cd_image(const std::string &path) {
  std::string ext = file_extension_lower(path);

  if (ext == "cue") return open_via_cue(path);

  if (ext == "bin") {
    fs::path cue_guess = fs::path(path).replace_extension(".cue");
    try {
      fs::path resolved =
          resolve_case_insensitive(cue_guess.parent_path(), cue_guess.filename().string());
      return open_via_cue(resolved.string());
    } catch (const std::exception &) {
      // sem .cue ao lado: assume layout plano (nao validado -- ver README)
    }
  }

  // .iso ou desconhecido: layout plano.
  return std::make_unique<BlockDevice>(path, kCdUserDataSize);
}

namespace {

// Acha o setor (unidades FAT de 512 B) onde comeca de fato o boot sector:
// se o bloco 0 (lido em unidades de kCdUserDataSize) tiver assinatura MBR
// (0x55 0xAA em 0x1FE), usa o LBA da 1a entrada de particao; senao, 0.
bool read_boot_sector_start(const BlockDevice &dev, std::uint64_t *out_sector0) {
  if (dev.block_count() < 1) return false;
  std::vector<std::uint8_t> block0(kCdUserDataSize);
  try {
    dev.read_blocks(0, 1, block0.data());
  } catch (const std::exception &) {
    return false;
  }
  if (block0[kMbr_Signature] == 0x55 && block0[kMbr_Signature + 1] == 0xAA) {
    std::uint32_t lba = le32(block0.data() + kMbr_PartitionTable + 8);
    *out_sector0 = lba;
  } else {
    *out_sector0 = 0;
  }
  return true;
}

} // namespace

bool looks_like_kurzweil(const BlockDevice &dev) {
  std::uint64_t start_sector = 0;
  if (!read_boot_sector_start(dev, &start_sector)) return false;

  // le so os primeiros kSectorSize bytes do boot sector, endereçados por
  // byte dentro do bloco de kCdUserDataSize (BlockDevice deve ter sido
  // aberto com esse block_size -- ver open_kurzweil_cd_image()).
  std::uint64_t byte_offset = start_sector * kSectorSize;
  std::uint64_t block_index = byte_offset / kCdUserDataSize;
  std::size_t skip = static_cast<std::size_t>(byte_offset - block_index * kCdUserDataSize);
  if (skip + kSectorSize > kCdUserDataSize) return false; // nao deveria acontecer na pratica

  std::vector<std::uint8_t> block(kCdUserDataSize);
  try {
    dev.read_blocks(block_index, 1, block.data());
  } catch (const std::exception &) {
    return false;
  }
  const std::uint8_t *bs = block.data() + skip;

  std::uint16_t bytes_per_sector = le16(bs + kBpb_BytesPerSector);
  std::uint8_t media = bs[kBpb_MediaDescriptor];
  return bytes_per_sector == kSectorSize && media == 0xF8;
}

KurzweilDisk::KurzweilDisk(const BlockDevice &dev) : dev_(dev) {
  std::uint64_t start_sector = 0;
  if (!read_boot_sector_start(dev_, &start_sector)) {
    throw std::runtime_error("imagem Kurzweil vazia ou ilegivel");
  }
  partition_start_sector_ = start_sector;

  auto bs = read_bytes(partition_start_sector_ * kSectorSize, kSectorSize);

  std::uint16_t bytes_per_sector = le16(bs.data() + kBpb_BytesPerSector);
  std::uint8_t media = bs[kBpb_MediaDescriptor];
  if (bytes_per_sector != kSectorSize || media != 0xF8) {
    throw std::runtime_error("boot sector nao parece FAT16 Kurzweil (bytes/setor ou media descriptor)");
  }

  sectors_per_cluster_ = bs[kBpb_SectorsPerCluster];
  std::uint16_t reserved = le16(bs.data() + kBpb_ReservedSectors);
  std::uint8_t num_fats = bs[kBpb_NumFats];
  root_entries_ = le16(bs.data() + kBpb_RootEntries);
  std::uint16_t sectors_per_fat = le16(bs.data() + kBpb_SectorsPerFat);

  if (sectors_per_cluster_ == 0) {
    throw std::runtime_error("FAT16 Kurzweil: sectors_per_cluster invalido");
  }

  fat_start_ = partition_start_sector_ + reserved;
  root_start_ = fat_start_ + static_cast<std::uint64_t>(num_fats) * sectors_per_fat;
  root_dir_sectors_ = (root_entries_ * kDirEntrySize) / kSectorSize;
  cluster_base_ = root_start_ + root_dir_sectors_ - 2 * sectors_per_cluster_;

  auto fat_bytes = read_bytes(fat_start_ * kSectorSize,
                               static_cast<std::size_t>(sectors_per_fat) * kSectorSize);
  fat_.resize(fat_bytes.size() / 2);
  for (std::size_t i = 0; i < fat_.size(); ++i) {
    fat_[i] = le16(fat_bytes.data() + i * 2);
  }
}

std::vector<std::uint8_t> KurzweilDisk::read_bytes(std::uint64_t byte_offset,
                                                    std::size_t len) const {
  std::uint64_t block_start = byte_offset / kCdUserDataSize;
  std::uint64_t block_end = (byte_offset + len + kCdUserDataSize - 1) / kCdUserDataSize;
  std::size_t nblocks = static_cast<std::size_t>(block_end - block_start);

  std::vector<std::uint8_t> buf(nblocks * kCdUserDataSize);
  dev_.read_blocks(block_start, nblocks, buf.data());

  std::size_t skip = static_cast<std::size_t>(byte_offset - block_start * kCdUserDataSize);
  return std::vector<std::uint8_t>(buf.begin() + static_cast<long>(skip),
                                    buf.begin() + static_cast<long>(skip + len));
}

std::uint16_t KurzweilDisk::fat_at(std::uint16_t cluster) const {
  if (cluster >= fat_.size()) {
    throw std::runtime_error("indice de cluster FAT16 fora do limite");
  }
  return fat_[cluster];
}

std::vector<std::uint8_t> KurzweilDisk::read_cluster_chain(std::uint16_t start_cluster,
                                                            std::uint64_t max_bytes) const {
  std::vector<std::uint8_t> out;
  out.reserve(max_bytes ? max_bytes : sectors_per_cluster_ * kSectorSize);

  std::uint16_t cluster = start_cluster;
  const std::size_t cluster_bytes = sectors_per_cluster_ * kSectorSize;

  while (cluster >= 2 && cluster < kFatEndMin) {
    auto chunk = read_bytes(cluster_to_sector(cluster) * kSectorSize, cluster_bytes);
    out.insert(out.end(), chunk.begin(), chunk.end());
    if (max_bytes != 0 && out.size() >= max_bytes) break;
    cluster = fat_at(cluster);
  }

  if (max_bytes != 0) out.resize(max_bytes, 0);
  return out;
}

std::vector<KurzweilDirEntry> KurzweilDisk::parse_dir_entries(
    const std::vector<std::uint8_t> &buf) const {
  std::vector<KurzweilDirEntry> result;
  std::size_t count = buf.size() / kDirEntrySize;

  for (std::size_t i = 0; i < count; ++i) {
    const std::uint8_t *e = buf.data() + i * kDirEntrySize;
    if (e[kDirEntry_Name] == kNameFree) break; // fim do diretorio
    if (e[kDirEntry_Name] == kNameDeleted) continue;
    if (e[kDirEntry_Name] == kNameDot) continue; // "." / ".."
    std::uint8_t attr = e[kDirEntry_Attr];
    if (attr & kAttrVolumeLabel) continue;

    KurzweilDirEntry entry;
    std::string name = ascii_trim(e + kDirEntry_Name, 8);
    std::string ext = ascii_trim(e + kDirEntry_Ext, 3);
    entry.name = ext.empty() ? name : (name + "." + ext);
    entry.is_directory = (attr & kAttrDirectory) != 0;
    entry.start_cluster = le16(e + kDirEntry_StartCluster);
    entry.size = le32(e + kDirEntry_Size);
    result.push_back(std::move(entry));
  }

  return result;
}

std::vector<KurzweilDirEntry> KurzweilDisk::list_root() const {
  auto buf = read_bytes(root_start_ * kSectorSize, static_cast<std::size_t>(root_dir_sectors_) * kSectorSize);
  return parse_dir_entries(buf);
}

std::vector<KurzweilDirEntry> KurzweilDisk::list_directory(const KurzweilDirEntry &dir) const {
  if (!dir.is_directory) {
    throw std::runtime_error("list_directory: entrada nao e um diretorio");
  }
  auto buf = read_cluster_chain(dir.start_cluster, 0); // 0 = le a cadeia inteira
  return parse_dir_entries(buf);
}

std::vector<std::uint8_t> KurzweilDisk::read_file(const KurzweilDirEntry &entry) const {
  if (entry.is_directory) {
    throw std::runtime_error("read_file: entrada e um diretorio");
  }
  if (entry.size == 0) return {};
  return read_cluster_chain(entry.start_cluster, entry.size);
}

} // namespace akai2sfz
