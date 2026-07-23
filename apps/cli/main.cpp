// akai2sfz CLI -- M0: list/extract sobre a camada de filesystem Akai.
#include "akai2sfz/filesystem.hpp"
#include "akai2sfz/image.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace akai2sfz;

namespace {

void print_usage(const char *argv0) {
  std::cerr << "uso:\n"
            << "  " << argv0 << " list <imagem> [-p particao(1-based)]\n"
            << "  " << argv0 << " extract <imagem> <VOLUME/ARQUIVO> <dir_saida> [-p particao]\n";
}

std::string vol_type_label(raw::VolType t) {
  switch (t) {
    case raw::VolType::S1000: return "S1000 VOLUME";
    case raw::VolType::S3000: return "S3000 VOLUME";
    case raw::VolType::Cd3000: return "CD3000 VOLUME";
    default: return "";
  }
}

int cmd_list(const std::string &image_path, std::size_t partition_1based) {
  BlockDevice dev(image_path);
  auto partitions = scan_partitions(dev);

  if (partitions.empty()) {
    std::cerr << "nenhuma particao Akai valida encontrada em " << image_path << "\n";
    return 1;
  }
  if (partition_1based == 0 || partition_1based > partitions.size()) {
    std::cerr << "particao " << partition_1based << " nao existe (imagem tem "
              << partitions.size() << ")\n";
    return 1;
  }

  std::cerr << partitions.size() << " particao(oes) encontrada(s); listando #"
            << partition_1based << " (bloco inicial " << partitions[partition_1based - 1].start_block
            << ", " << partitions[partition_1based - 1].size_blocks << " blocos)\n\n";

  OpenPartition part(dev, partitions[partition_1based - 1]);

  std::size_t total_entries = 0;
  for (std::size_t vi = 0; vi < part.volume_count(); ++vi) {
    raw::VolType vtype = part.volume_type(vi);
    if (vtype == raw::VolType::Inactive) continue;
    std::string label = vol_type_label(vtype);
    if (label.empty()) continue; // fora de escopo (S900 etc.)

    std::string vname = part.volume_name(vi);
    std::cout << "/" << vname << ":\n";

    auto files = list_files(part, vi);
    for (const auto &f : files) {
      std::cout << "  " << file_type_name(f.type) << "\t" << f.size << "\t"
                << f.name << "." << f.extension << "\n";
      ++total_entries;
    }
    std::cout << "\n";
  }

  std::cerr << "total: " << total_entries << " arquivo(s)\n";
  return 0;
}

// Divide "VOLUME/ARQUIVO" (sem barra inicial obrigatoria) em nome de volume e de arquivo.
bool split_vol_file(const std::string &path, std::string &vol, std::string &file) {
  std::string p = path;
  if (!p.empty() && p.front() == '/') p.erase(p.begin());
  auto slash = p.find('/');
  if (slash == std::string::npos) return false;
  vol = p.substr(0, slash);
  file = p.substr(slash + 1);
  return !vol.empty() && !file.empty();
}

int cmd_extract(const std::string &image_path, const std::string &akai_path,
                 const std::string &out_dir, std::size_t partition_1based) {
  std::string vol_name, file_name;
  if (!split_vol_file(akai_path, vol_name, file_name)) {
    std::cerr << "caminho invalido, esperado VOLUME/ARQUIVO: " << akai_path << "\n";
    return 1;
  }

  BlockDevice dev(image_path);
  auto partitions = scan_partitions(dev);
  if (partition_1based == 0 || partition_1based > partitions.size()) {
    std::cerr << "particao invalida\n";
    return 1;
  }
  OpenPartition part(dev, partitions[partition_1based - 1]);

  for (std::size_t vi = 0; vi < part.volume_count(); ++vi) {
    raw::VolType vtype = part.volume_type(vi);
    if (vtype == raw::VolType::Inactive) continue;
    if (part.volume_name(vi) != vol_name) continue;

    for (const auto &f : list_files(part, vi)) {
      if (f.name != file_name) continue;

      auto data = extract_file(part, f);
      std::string out_path = out_dir + "/" + f.name + "." + f.extension;
      std::ofstream out(out_path, std::ios::binary);
      if (!out) {
        std::cerr << "nao foi possivel criar: " << out_path << "\n";
        return 1;
      }
      out.write(reinterpret_cast<const char *>(data.data()),
                static_cast<std::streamsize>(data.size()));
      std::cerr << "extraido: " << out_path << " (" << data.size() << " bytes)\n";
      return 0;
    }
  }

  std::cerr << "arquivo nao encontrado: " << akai_path << "\n";
  return 1;
}

} // namespace

int main(int argc, char **argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  if (args.empty()) {
    print_usage(argv[0]);
    return 1;
  }

  // opcao -p <n>, pode estar em qualquer posicao
  std::size_t partition = 1;
  for (std::size_t i = 0; i < args.size();) {
    if (args[i] == "-p" && i + 1 < args.size()) {
      partition = static_cast<std::size_t>(std::stoul(args[i + 1]));
      args.erase(args.begin() + static_cast<long>(i), args.begin() + static_cast<long>(i) + 2);
    } else {
      ++i;
    }
  }

  try {
    if (args[0] == "list" && args.size() >= 2) {
      return cmd_list(args[1], partition);
    }
    if (args[0] == "extract" && args.size() >= 4) {
      return cmd_extract(args[1], args[2], args[3], partition);
    }
  } catch (const std::exception &e) {
    std::cerr << "erro: " << e.what() << "\n";
    return 1;
  }

  print_usage(argv[0]);
  return 1;
}
