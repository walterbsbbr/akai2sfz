// Deteccao e abertura de containers de CD reais: BIN+CUE, NRG e (por
// autodeteccao) MDF. Ver akai2sfz/image.hpp para o desenho geral.
//
// Estrategia central: quando ha ambiguidade (tamanho de setor, se ha pregap
// embutido no arquivo), monta-se um ou mais SectorLayout candidatos e usa-se
// scan_partitions() -- que ja valida magic+checksum do akai_parthead_s --
// como oraculo: o primeiro candidato que encontra pelo menos uma particao
// valida e aceito. Isso evita ter que decifrar 100% de cada formato
// proprietario; so precisa ser bom o suficiente para achar a track certa.
#include "akai2sfz/image.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace akai2sfz {

namespace fs = std::filesystem;

namespace {

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

// Resolve um nome de arquivo referenciado (ex. numa CUE) tolerando
// diferenca de maiuscula/minuscula -- comum em imagens rippadas no Windows
// e usadas depois num filesystem case-sensitive (ex.: TZIFFXAK.cue referencia
// "TZIFFXAK.BIN" mas o arquivo real e "TZIFFXAK.bin").
fs::path resolve_case_insensitive(const fs::path &dir, const std::string &name) {
  fs::path direct = dir / name;
  if (fs::exists(direct)) return direct;

  std::string wanted_lower = to_lower(name);
  if (fs::exists(dir) && fs::is_directory(dir)) {
    for (const auto &entry : fs::directory_iterator(dir)) {
      if (to_lower(entry.path().filename().string()) == wanted_lower) {
        return entry.path();
      }
    }
  }
  throw std::runtime_error("arquivo referenciado nao encontrado: " + name);
}

std::vector<std::string> split_ws(const std::string &line) {
  std::istringstream iss(line);
  std::vector<std::string> parts;
  std::string tok;
  while (iss >> tok) parts.push_back(tok);
  return parts;
}

// --- deteccao de setor bruto por sync pattern de CD ---
// 12 bytes: 00 FF FF FF FF FF FF FF FF FF FF 00 -- inicio de todo setor
// MODE1/MODE2 "raw" (2352 bytes), independente do conteudo.
bool has_sync_pattern(const std::uint8_t *buf) {
  if (buf[0] != 0x00 || buf[11] != 0x00) return false;
  for (int i = 1; i < 11; ++i) {
    if (buf[i] != 0xFF) return false;
  }
  return true;
}

bool file_has_sync_pattern_at(const std::string &path, std::uint64_t offset) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  f.seekg(static_cast<std::streamoff>(offset));
  std::uint8_t buf[12];
  f.read(reinterpret_cast<char *>(buf), sizeof(buf));
  if (f.gcount() != static_cast<std::streamsize>(sizeof(buf))) return false;
  return has_sync_pattern(buf);
}

// Tenta abrir `path` com cada layout candidato (nessa ordem) e devolve o
// primeiro que resulta em pelo menos uma particao Akai valida. Lanca
// std::runtime_error com uma mensagem clara se nenhum candidato funcionar.
std::unique_ptr<BlockDevice> open_first_valid(const std::string &path,
                                               const std::vector<SectorLayout> &candidates) {
  if (!fs::exists(path)) {
    throw std::runtime_error("arquivo nao encontrado: " + path);
  }
  for (const auto &layout : candidates) {
    try {
      auto dev = std::make_unique<BlockDevice>(path, 0x2000, layout);
      if (!scan_partitions(*dev).empty()) {
        return dev;
      }
    } catch (const std::exception &) {
      // candidato invalido (ex.: arquivo menor que o esperado) -- tenta o proximo
    }
  }
  throw std::runtime_error(
      "nao foi possivel localizar uma particao Akai valida em: " + path
      + " (containers MDF/NRG sem amostra de validacao podem precisar de ajuste -- ver README)");
}

std::uint32_t msf_to_frames(int mm, int ss, int ff) {
  return static_cast<std::uint32_t>((mm * 60 + ss) * 75 + ff);
}

// Le uma cue sheet simples (um FILE, uma ou mais TRACK) e devolve o
// BlockDevice para a primeira track -- e onde o volume sampler Akai sempre
// esta em CDs deste tipo. Multiplas tracks (ex.: discos mistos com audio
// depois dos dados) nao sao suportadas -- fora de escopo para CDs de sampler.
std::unique_ptr<BlockDevice> open_via_cue(const std::string &cue_path) {
  std::ifstream f(cue_path);
  if (!f) {
    throw std::runtime_error("nao foi possivel abrir a cue sheet: " + cue_path);
  }

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
      // FILE "nome do arquivo" BINARY -- reconstroi o nome entre aspas
      // (pode conter espacos).
      auto first_quote = line.find('"');
      auto last_quote = line.rfind('"');
      if (first_quote != std::string::npos && last_quote > first_quote) {
        bin_name = line.substr(first_quote + 1, last_quote - first_quote - 1);
      }
      continue;
    }

    if (!have_track && parts[0] == "TRACK" && parts.size() >= 3) {
      // TRACK 01 MODE1/2352
      auto slash = parts[2].find('/');
      if (slash != std::string::npos) {
        std::string mode_str = parts[2].substr(0, slash);
        std::string size_str = parts[2].substr(slash + 1);
        if (mode_str.size() >= 5 && mode_str.substr(0, 4) == "MODE") {
          mode = mode_str[4] - '0';
        }
        try {
          sector_size = static_cast<std::size_t>(std::stoul(size_str));
        } catch (...) {
          // mantem o default (2352) se nao conseguir parsear
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
        break; // so a primeira track/index01 interessa
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
  layout.user_data_size = 0x800; // 2048, sempre
  if (sector_size == 0x800) {
    layout.user_data_offset = 0; // ja "cooked"
  } else if (sector_size == 2352 && mode == 2) {
    layout.user_data_offset = 24; // XA Mode 2 Form 1 (nao validado contra amostra real)
  } else {
    layout.user_data_offset = 16; // MODE1/2352, validado contra TZIFFXAK.bin/.cue real
  }

  return open_first_valid(bin_path.string(), {layout});
}

// --- NRG (Nero) ---
// Le o footer (v2: assinatura "NER5" de 4 bytes seguida de um offset BE de
// 8 bytes, terminando exatamente no fim do arquivo; v1 mais antigo: "NERO"
// de 4 bytes + offset BE de 4 bytes -- nao validado contra amostra real,
// best-effort) para achar os chunks, e dentro deles o chunk CUEX para achar
// o pregap e o LBA 0 da track 1. Validado contra um NRG v2 real (ver git
// log): setor cooked de 2048, pregap de 150 frames incluido no arquivo.
std::unique_ptr<BlockDevice> open_nrg(const std::string &path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) throw std::runtime_error("nao foi possivel abrir: " + path);
  std::uint64_t file_size = static_cast<std::uint64_t>(f.tellg());
  if (file_size < 12) throw std::runtime_error("arquivo NRG muito pequeno: " + path);

  auto read_at = [&](std::uint64_t off, std::size_t len) {
    std::vector<std::uint8_t> buf(len);
    f.seekg(static_cast<std::streamoff>(off));
    f.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(len));
    if (f.gcount() != static_cast<std::streamsize>(len)) {
      throw std::runtime_error("leitura incompleta no footer NRG: " + path);
    }
    return buf;
  };

  auto be32 = [](const std::uint8_t *p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16)
        | (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
  };
  auto be64 = [&](const std::uint8_t *p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
  };

  std::uint64_t chunk_offset = 0;
  {
    // layout real (confirmado contra amostra): [...chunk anterior] "NER5"
    // (4 bytes) [offset BE de 8 bytes] -- assinatura vem ANTES do offset,
    // terminando exatamente no fim do arquivo.
    auto tail = read_at(file_size - 12, 12);
    if (std::memcmp(tail.data(), "NER5", 4) == 0) {
      chunk_offset = be64(tail.data() + 4);
    } else {
      auto tail4 = read_at(file_size - 8, 8);
      if (std::memcmp(tail4.data(), "NERO", 4) == 0) {
        chunk_offset = be32(tail4.data() + 4); // NRG v1, nao validado contra amostra real
      } else {
        throw std::runtime_error("assinatura NRG nao reconhecida: " + path);
      }
    }
  }

  // le os chunks do footer inteiros de uma vez (footer costuma ter poucos KB)
  if (chunk_offset >= file_size) {
    throw std::runtime_error("offset de chunks NRG invalido: " + path);
  }
  auto chunks = read_at(chunk_offset, static_cast<std::size_t>(file_size - chunk_offset));

  std::int32_t pregap_lba = 0, track1_lba = 0;
  bool have_pregap = false, have_track1 = false;

  std::size_t pos = 0;
  while (pos + 8 <= chunks.size()) {
    char tag[5] = {0};
    std::memcpy(tag, chunks.data() + pos, 4);
    std::uint32_t len = be32(chunks.data() + pos + 4);
    std::size_t payload_off = pos + 8;
    if (std::string(tag) == "END!") break;
    if (payload_off + len > chunks.size()) break; // chunk truncado/corrompido

    if (std::string(tag) == "CUEX") {
      // entradas de 8 bytes: adr/ctrl(1) track(1) index(1) reservado(1) lba(4, BE signed)
      for (std::size_t e = 0; e + 8 <= len; e += 8) {
        const std::uint8_t *entry = chunks.data() + payload_off + e;
        std::uint8_t track = entry[1];
        std::uint8_t index = entry[2];
        std::int32_t lba = static_cast<std::int32_t>(be32(entry + 4));
        if (track == 1 && index == 0) {
          pregap_lba = lba;
          have_pregap = true;
        } else if (track == 1 && index == 1) {
          track1_lba = lba;
          have_track1 = true;
        }
      }
    }

    pos = payload_off + len;
  }

  std::uint32_t pregap_frames = (have_pregap && have_track1)
      ? static_cast<std::uint32_t>(track1_lba - pregap_lba)
      : 0;

  // dois candidatos, na ordem que a amostra real confirmou ser mais comum
  // (cooked 2048); scan_partitions() decide qual (se algum) esta certo.
  SectorLayout cooked;
  cooked.base_byte_offset = static_cast<std::uint64_t>(pregap_frames) * 0x800;
  cooked.physical_sector_size = 0x800;
  cooked.user_data_offset = 0;
  cooked.user_data_size = 0x800;

  SectorLayout raw;
  raw.base_byte_offset = static_cast<std::uint64_t>(pregap_frames) * 2352;
  raw.physical_sector_size = 2352;
  raw.user_data_offset = 16;
  raw.user_data_size = 0x800;

  return open_first_valid(path, {cooked, raw});
}

// MDF (Alcohol 120%) e .bin sem .cue ao lado: sem metadado externo
// confiavel, tenta layout plano e raw MODE1/2352 a partir do byte 0 (sem
// pregap embutido -- diferente do NRG). Nao ha amostra .mdf real neste
// projeto para validar; ver README.
std::unique_ptr<BlockDevice> open_autodetect(const std::string &path) {
  bool raw_likely = file_has_sync_pattern_at(path, 0);

  SectorLayout flat;
  flat.base_byte_offset = 0;
  flat.physical_sector_size = 0x800;
  flat.user_data_offset = 0;
  flat.user_data_size = 0x800;

  SectorLayout raw;
  raw.base_byte_offset = 0;
  raw.physical_sector_size = 2352;
  raw.user_data_offset = 16;
  raw.user_data_size = 0x800;

  // testa primeiro o que o sync-pattern sugere, mas tenta o outro tambem
  // (open_first_valid so aceita o que scan_partitions confirmar).
  std::vector<SectorLayout> candidates = raw_likely ? std::vector<SectorLayout>{raw, flat}
                                                     : std::vector<SectorLayout>{flat, raw};
  return open_first_valid(path, candidates);
}

} // namespace

std::unique_ptr<BlockDevice> open_cd_image(const std::string &path) {
  std::string ext = file_extension_lower(path);

  if (ext == "cue") {
    return open_via_cue(path);
  }
  if (ext == "nrg") {
    return open_nrg(path);
  }
  if (ext == "bin") {
    fs::path cue_guess = fs::path(path).replace_extension(".cue");
    try {
      fs::path resolved = resolve_case_insensitive(cue_guess.parent_path(), cue_guess.filename().string());
      return open_via_cue(resolved.string());
    } catch (const std::exception &) {
      return open_autodetect(path); // sem .cue ao lado: tenta na unha
    }
  }
  if (ext == "mdf") {
    return open_autodetect(path);
  }

  // .iso ou extensao desconhecida: layout plano, igual ao M0.
  return std::make_unique<BlockDevice>(path);
}

} // namespace akai2sfz
