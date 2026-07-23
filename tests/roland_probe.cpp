// Ferramenta de validacao manual (nao faz parte do ctest) -- usada durante
// o desenvolvimento do suporte Roland pra confirmar offsets do manual
// contra imagens reais antes de escrever o parser de conteudo.
#include "akai2sfz/roland_filesystem.hpp"
#include "akai2sfz/roland_format.hpp"

#include <iostream>

using namespace akai2sfz;
using namespace akai2sfz::roland_raw;

namespace {

std::string midi_note_name(int note) {
  static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  int octave = (note / 12) - 1;
  return std::string(names[note % 12]) + std::to_string(octave);
}

void probe_patch(const RolandDisk &disk, std::size_t patch_index) {
  auto entry = disk.read_dir_entry(FileType::Patch, patch_index);
  std::cout << "\n=== patch [" << patch_index << "] '" << entry.name << "' ===\n";

  auto patch = parse_roland_patch(disk.read_param(FileType::Patch, patch_index));

  // Comprime teclas consecutivas com o mesmo partial numa faixa.
  std::size_t i = 0;
  while (i < patch.partial_at_key.size()) {
    std::uint16_t pidx = patch.partial_at_key[i];
    std::size_t start = i;
    while (i < patch.partial_at_key.size() && patch.partial_at_key[i] == pidx) ++i;
    std::size_t end = i - 1;
    if (pidx == 0xFFFF) continue; // faixa sem som

    int lokey = static_cast<int>(start) + 21;
    int hikey = static_cast<int>(end) + 21;
    std::cout << "  teclas " << midi_note_name(lokey) << "-" << midi_note_name(hikey) << " (MIDI "
              << lokey << "-" << hikey << ") -> partial #" << pidx << "\n";

    if (pidx >= kDirPartial.entry_count) {
      std::cout << "    (indice de partial fora do limite!)\n";
      continue;
    }
    auto partial_entry = disk.read_dir_entry(FileType::Partial, pidx);
    auto partial = parse_roland_partial(disk.read_param(FileType::Partial, pidx));
    std::cout << "    partial '" << partial_entry.name << "', " << partial.sample_slots.size()
              << " slot(s) de sample:\n";

    for (const auto &slot : partial.sample_slots) {
      if (slot.sample_index == 0xFFFF || slot.sample_index >= kDirSample.entry_count) continue;
      auto sample_entry = disk.read_dir_entry(FileType::Sample, slot.sample_index);
      std::cout << "      vel " << static_cast<int>(slot.vel_lower) << "-"
                << static_cast<int>(slot.vel_upper) << " pan=" << static_cast<int>(slot.pan)
                << " level=" << static_cast<int>(slot.level) << " -> sample #"
                << slot.sample_index << " '" << sample_entry.name << "'\n";
    }
  }
}

void probe_sample(const RolandDisk &disk, std::size_t sample_index) {
  auto entry = disk.read_dir_entry(FileType::Sample, sample_index);
  std::cout << "\n=== sample [" << sample_index << "] '" << entry.name << "' ===\n";
  std::cout << "  fat_entry=" << entry.fat_entry << " capacity=" << entry.capacity << " clusters\n";

  auto wave = disk.read_sample_wave(entry);
  std::cout << "  wave extraida: " << wave.size() << " bytes (" << (wave.size() / 2)
            << " frames 16-bit)\n";

  auto sample = parse_roland_sample(disk.read_param(FileType::Sample, sample_index), wave);
  std::cout << "  original_key=" << static_cast<int>(sample.original_key) << " ("
            << midi_note_name(sample.original_key) << ")\n";
  std::cout << "  loop_mode=" << static_cast<int>(sample.loop_mode)
            << " s_loop_enable=" << sample.s_loop_enable << "\n";
  std::cout << "  start_point=" << sample.start_point << " s_loop_start=" << sample.s_loop_start
            << " s_loop_end=" << sample.s_loop_end << " (total frames=" << sample.pcm.size()
            << ")\n";
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "uso: roland_probe <imagem>\n";
    return 1;
  }

  BlockDevice dev(argv[1], kBlockSize);
  if (!looks_like_roland(dev)) {
    std::cerr << "nao parece uma imagem Roland\n";
    return 1;
  }

  RolandDisk disk(dev);
  std::cout << "drive name: '" << disk.drive_name() << "'\n";
  std::cout << "capacity (blocks): " << disk.capacity_blocks() << "\n";
  std::cout << "file counts: vol=" << disk.file_count(FileType::Volume)
            << " perf=" << disk.file_count(FileType::Performance)
            << " patch=" << disk.file_count(FileType::Patch)
            << " partial=" << disk.file_count(FileType::Partial)
            << " sample=" << disk.file_count(FileType::Sample) << "\n\n";

  auto volumes = disk.list_active(FileType::Volume);
  std::cout << volumes.size() << " volume(s) ativo(s):\n";
  for (const auto &v : volumes) std::cout << "  [" << v.index << "] '" << v.name << "'\n";

  auto patches = disk.list_active(FileType::Patch);
  std::cout << "\n" << patches.size() << " patch(es) ativo(s) (primeiros 20):\n";
  for (std::size_t i = 0; i < patches.size() && i < 20; ++i) {
    std::cout << "  [" << patches[i].index << "] '" << patches[i].name << "'\n";
  }

  auto samples = disk.list_active(FileType::Sample);
  std::cout << "\n" << samples.size() << " sample(s) ativo(s) (primeiros 10):\n";
  for (std::size_t i = 0; i < samples.size() && i < 10; ++i) {
    const auto &s = samples[i];
    std::cout << "  [" << s.index << "] '" << s.name << "' fat_entry=" << s.fat_entry
              << " capacity=" << s.capacity << " clusters ("
              << (static_cast<std::uint64_t>(s.capacity) * kClusterBlocks * kBlockSize)
              << " bytes)\n";
  }

  if (argc >= 3) {
    std::size_t patch_index = static_cast<std::size_t>(std::stoul(argv[2]));
    probe_patch(disk, patch_index);
  }
  if (argc >= 4) {
    std::size_t sample_index = static_cast<std::size_t>(std::stoul(argv[3]));
    probe_sample(disk, sample_index);
  }

  return 0;
}
