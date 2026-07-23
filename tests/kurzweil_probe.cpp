// Ferramenta de validacao manual (nao faz parte do ctest) -- usada durante
// o desenvolvimento do suporte Kurzweil pra confirmar o layout FAT16 e o
// formato .krz contra imagens reais.
#include "akai2sfz/krz_format.hpp"
#include "akai2sfz/krz_raw_format.hpp"
#include "akai2sfz/kurzweil_filesystem.hpp"

#include <iostream>

using namespace akai2sfz;

namespace {

void list_recursive(const KurzweilDisk &disk, const std::vector<KurzweilDirEntry> &entries,
                     const std::string &prefix) {
  for (const auto &e : entries) {
    if (e.is_directory) {
      std::cout << prefix << e.name << "/\n";
      list_recursive(disk, disk.list_directory(e), prefix + e.name + "/");
    } else {
      std::cout << prefix << e.name << "  (" << e.size << " bytes)\n";
    }
  }
}

void probe_krz(const std::vector<std::uint8_t> &data) {
  std::uint32_t osize = krz_read_osize(data);
  std::cout << "\nPRAM ok, osize=" << osize << "\n";

  auto objs = list_krz_objects(data);
  std::size_t nsample = 0, nkeymap = 0, nprogram = 0, nother = 0;
  for (const auto &o : objs) {
    if (o.type_raw == static_cast<int>(krz_raw::ObjectType::Sample)) ++nsample;
    else if (o.type_raw == static_cast<int>(krz_raw::ObjectType::Keymap)) ++nkeymap;
    else if (o.type_raw == static_cast<int>(krz_raw::ObjectType::Program)) ++nprogram;
    else ++nother;
  }
  std::cout << objs.size() << " objeto(s): " << nsample << " sample(s), " << nkeymap
            << " keymap(s), " << nprogram << " program(s), " << nother << " outro(s)\n";

  for (const auto &o : objs) {
    if (o.type_raw != static_cast<int>(krz_raw::ObjectType::Program)) continue;
    KrzProgram pg = parse_krz_program(data, o);
    std::cout << "\nprogram '" << pg.name << "' id=" << o.id << " mode=" << pg.mode
              << " layers=" << pg.layers.size() << "\n";
    for (std::size_t i = 0; i < pg.layers.size() && i < 5; ++i) {
      const auto &l = pg.layers[i];
      std::cout << "  layer[" << i << "] key=" << l.lokey << "-" << l.hikey
                << " stereo=" << l.stereo << " keymap_id=" << l.keymap_id << "\n";
      if (l.keymap_id < 0) continue;
      for (const auto &ko : objs) {
        if (ko.type_raw != static_cast<int>(krz_raw::ObjectType::Keymap) || ko.id != l.keymap_id) {
          continue;
        }
        KrzKeymap km = parse_krz_keymap(data, ko);
        int sid = km.entries[static_cast<std::size_t>(l.lokey)].sample_id;
        if (sid < 0) sid = km.default_sample_id;
        int subsample = km.entries[static_cast<std::size_t>(l.lokey)].subsample_number;
        std::cout << "    -> keymap '" << km.name << "' key[" << l.lokey << "] sample_id=" << sid
                  << " subsample=" << subsample << "\n";
        for (const auto &so : objs) {
          if (so.type_raw != static_cast<int>(krz_raw::ObjectType::Sample) || so.id != sid) continue;
          KrzSample sm = parse_krz_sample(data, so);
          std::cout << "      -> sample '" << sm.name << "' stereo=" << sm.stereo
                    << " headers=" << sm.headers.size() << "\n";
          for (const auto &h : sm.headers) {
            auto pcm = krz_extract_pcm(data, osize, h);
            int rate = h.sample_period_ns > 0
                ? static_cast<int>(1000000000.0 / h.sample_period_ns + 0.5)
                : 0;
            std::cout << "         root_key=" << h.root_key << " looped=" << h.looped
                      << " needs_load=" << h.needs_load << " rate=" << rate << "Hz"
                      << " loop=[" << h.sample_loop_start_words << "," << h.sample_end_words
                      << "] frames=" << pcm.size() << "\n";
          }
        }
      }
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "uso: kurzweil_probe <imagem> [ARQUIVO.KRZ]\n";
    return 1;
  }

  auto dev = open_kurzweil_cd_image(argv[1]);
  if (!looks_like_kurzweil(*dev)) {
    std::cerr << "nao parece uma imagem Kurzweil (boot sector FAT16 nao bate)\n";
    return 1;
  }

  KurzweilDisk disk(*dev);
  auto root = disk.list_root();
  std::cout << root.size() << " entrada(s) na raiz:\n";
  list_recursive(disk, root, "  ");

  if (argc >= 3) {
    std::string want = argv[2];
    for (const auto &e : root) {
      if (e.is_directory || e.name != want) continue;
      auto data = disk.read_file(e);
      std::cout << "\nextraido '" << e.name << "': " << data.size() << " bytes (declarado "
                << e.size << ")\n";
      probe_krz(data);
      return 0;
    }
    std::cerr << "arquivo nao encontrado na raiz: " << want << "\n";
    return 1;
  }

  return 0;
}
