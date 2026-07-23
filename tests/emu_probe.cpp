// Ferramenta de validacao manual (nao faz parte do ctest) -- usada durante
// o desenvolvimento do suporte E-mu pra confirmar o layout do container
// EMU3 (e depois o formato de bank) contra imagens reais.
#include "akai2sfz/emu_filesystem.hpp"
#include "akai2sfz/emu_format.hpp"

#include <iostream>

namespace {

void probe_bank(const std::vector<std::uint8_t> &bank_bytes) {
  using namespace akai2sfz;
  using namespace akai2sfz::emu_raw;

  BankFormat fmt = detect_emu_bank_format(bank_bytes);
  const char *fmt_name = fmt == BankFormat::EmulatorThree ? "EMULATOR THREE"
      : fmt == BankFormat::Emulator3X                     ? "EMULATOR 3X"
      : fmt == BankFormat::EsiV3                           ? "EMU SI-32 v3"
                                                             : "desconhecido";
  std::cout << "\nformato: " << fmt_name << "\n";
  if (fmt == BankFormat::Unknown) return;

  std::cout << "nome do bank: '" << emu_bank_name(bank_bytes) << "'\n";
  int npresets = emu_bank_preset_count(bank_bytes, fmt);
  int nsamples = emu_bank_sample_count(bank_bytes, fmt);
  std::cout << npresets << " preset(s), " << nsamples << " sample(s)\n";

  for (int pi = 0; pi < npresets; ++pi) {
    EmuPreset p = parse_emu_preset(bank_bytes, fmt, pi);
    std::cout << "\npreset[" << pi << "] '" << p.name << "' note_zones=" << p.note_zones.size()
              << " zones=" << p.zones.size() << " pbr=" << static_cast<int>(p.pitch_bend_range)
              << " vel_pri=[" << static_cast<int>(p.vel_pri_low) << ","
              << static_cast<int>(p.vel_pri_high) << "] vel_sec=["
              << static_cast<int>(p.vel_sec_low) << "," << static_cast<int>(p.vel_sec_high) << "]\n";

    // comprime faixas de tecla com o mesmo note_zone
    std::size_t key = 0;
    while (key < p.note_zone_mapping.size()) {
      std::uint8_t nzi = p.note_zone_mapping[key];
      std::size_t start = key;
      while (key < p.note_zone_mapping.size() && p.note_zone_mapping[key] == nzi) ++key;
      std::size_t end = key - 1;
      if (nzi >= p.note_zones.size()) continue;
      const auto &nz = p.note_zones[nzi];
      std::cout << "  teclas MIDI " << (start + kMidiNoteOffset) << "-" << (end + kMidiNoteOffset)
                << " -> note_zone[" << static_cast<int>(nzi) << "] pri_zone="
                << static_cast<int>(nz.pri_zone) << " sec_zone=" << static_cast<int>(nz.sec_zone)
                << "\n";
      for (std::uint8_t zidx : {nz.pri_zone, nz.sec_zone}) {
        if (zidx == kZoneUnused || zidx >= p.zones.size()) continue;
        const auto &z = p.zones[zidx];
        std::cout << "    zone[" << static_cast<int>(zidx) << "]: original_key="
                  << static_cast<int>(z.original_key) << " sample_id=" << z.sample_id
                  << " pan=" << static_cast<int>(z.vca_pan) << " tuning="
                  << static_cast<int>(z.note_tuning) << "\n";
      }
    }
  }

  for (int si = 1; si <= nsamples && si <= 5; ++si) {
    EmuSample s = parse_emu_sample(bank_bytes, fmt, si);
    std::cout << "\nsample #" << si << " '" << s.name << "' rate=" << s.rate
              << " stereo=" << s.stereo << " loop=" << s.loop_enabled << " loop=["
              << s.loop_start_frame << "," << s.loop_end_frame << "] frames_L="
              << s.pcm_left.size() << " frames_R=" << s.pcm_right.size() << "\n";
  }
}

} // namespace

using namespace akai2sfz;
using namespace akai2sfz::emu_raw;

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "uso: emu_probe <imagem>\n";
    return 1;
  }

  BlockDevice dev(argv[1], kBlockSize);
  if (!looks_like_emu(dev)) {
    std::cerr << "nao parece uma imagem E-mu (sem assinatura 'EMU3')\n";
    return 1;
  }

  EmuDisk disk(dev);
  auto folders = disk.list_folders();
  std::cout << folders.size() << " pasta(s):\n";

  for (const auto &folder : folders) {
    std::cout << "\n=== pasta '" << folder.name << "' (" << folder.content_blocks.size()
              << " bloco(s) de conteudo) ===\n";
    auto files = disk.list_files(folder);
    std::cout << files.size() << " arquivo(s):\n";
    for (const auto &f : files) {
      std::cout << "  '" << f.name << "' start_cluster=" << f.start_cluster
                << " clusters=" << f.clusters << " blocks=" << f.blocks
                << " bytes_in_last_block=" << f.bytes_in_last_block
                << " size=" << f.byte_size(disk.blocks_per_cluster()) << " type=0x" << std::hex
                << static_cast<int>(f.type) << std::dec << " props=";
      for (unsigned char c : f.props) {
        if (c >= 32 && c < 127) std::cout << c;
        else std::cout << "\\x" << std::hex << static_cast<int>(c) << std::dec;
      }
      std::cout << "\n";
    }
  }

  if (argc >= 4) {
    // emu_probe <imagem> <pasta> <arquivo>: extrai e imprime os primeiros
    // bytes do arquivo, pra validar read_file() manualmente.
    std::string folder_name = argv[2];
    std::string file_name = argv[3];
    for (const auto &folder : folders) {
      if (folder.name != folder_name) continue;
      for (const auto &f : disk.list_files(folder)) {
        if (f.name != file_name) continue;
        auto data = disk.read_file(f);
        std::cout << "\nextraido '" << f.name << "': " << data.size() << " bytes\nprimeiros 64: ";
        for (std::size_t i = 0; i < 64 && i < data.size(); ++i) {
          char c = static_cast<char>(data[i]);
          std::cout << (c >= 32 && c < 127 ? c : '.');
        }
        std::cout << "\n";
        probe_bank(data);
        return 0;
      }
    }
    std::cerr << "arquivo nao encontrado: " << folder_name << "/" << file_name << "\n";
    return 1;
  }

  return 0;
}
