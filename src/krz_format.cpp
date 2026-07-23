#include "akai2sfz/krz_format.hpp"
#include "akai2sfz/krz_raw_format.hpp"

#include <cstring>
#include <stdexcept>

namespace akai2sfz {

using namespace krz_raw;

namespace {

std::int32_t be32s(const std::uint8_t *p) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(p[0]) << 24
      | static_cast<std::uint32_t>(p[1]) << 16 | static_cast<std::uint32_t>(p[2]) << 8
      | static_cast<std::uint32_t>(p[3]));
}

std::uint32_t be32u(const std::uint8_t *p) {
  return static_cast<std::uint32_t>(be32s(p));
}

std::int16_t be16s(const std::uint8_t *p) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(p[0]) << 8 | p[1]);
}

std::uint16_t be16u(const std::uint8_t *p) {
  return static_cast<std::uint16_t>(be16s(p));
}

void check_bounds(const std::vector<std::uint8_t> &data, std::uint64_t offset, std::uint64_t len,
                   const char *what) {
  if (offset + len > data.size()) {
    throw std::runtime_error(std::string("krz: leitura fora dos limites (") + what + ")");
  }
}

std::string read_krz_name(const std::vector<std::uint8_t> &data, std::uint64_t body_start,
                           std::uint16_t name_ofs) {
  if (name_ofs < 2) return "";
  std::uint64_t len = static_cast<std::uint64_t>(name_ofs) - 2;
  check_bounds(data, body_start + kObj_NameStart, len, "nome de objeto");
  const std::uint8_t *p = data.data() + body_start + kObj_NameStart;
  std::string name;
  for (std::uint64_t i = 0; i < len; ++i) {
    if (p[i] == 0) break;
    if (p[i] >= 32 && p[i] < 127) name.push_back(static_cast<char>(p[i]));
  }
  while (!name.empty() && name.back() == ' ') name.pop_back();
  while (!name.empty() && name.front() == ' ') name.erase(name.begin());
  return name;
}

} // namespace

std::uint32_t krz_read_osize(const std::vector<std::uint8_t> &data) {
  check_bounds(data, 0, kHeaderSize, "header .krz");
  if (std::memcmp(data.data() + kHeader_Magic, kMagic, kMagicLen) != 0) {
    throw std::runtime_error("nao e um arquivo .krz valido (magic 'PRAM' ausente)");
  }
  return be32u(data.data() + kHeader_Osize);
}

std::vector<KrzObjectRef> list_krz_objects(const std::vector<std::uint8_t> &data) {
  std::vector<KrzObjectRef> result;

  std::uint64_t pos = kHeaderSize;
  while (pos + 4 <= data.size()) {
    std::int32_t block_size = be32s(data.data() + pos);
    if (block_size >= 0) break; // fim da tabela

    std::uint64_t body_start = pos + 4;
    std::uint64_t body_end = pos - static_cast<std::uint64_t>(block_size);
    check_bounds(data, body_start, kObj_NameStart, "cabecalho de objeto");
    if (body_end > data.size() || body_end < body_start) {
      throw std::runtime_error("krz: bloco de objeto com tamanho invalido");
    }

    std::uint16_t hash = be16u(data.data() + body_start + kObj_Hash);
    std::uint16_t name_ofs = be16u(data.data() + body_start + kObj_NameOfs);

    KrzObjectRef ref;
    if (hash & kHashHighBit) {
      ref.type_raw = hash >> 10;
      ref.id = hash & 0x3FF;
    } else {
      ref.type_raw = hash >> 8;
      ref.id = hash & 0xFF;
    }
    ref.name = read_krz_name(data, body_start, name_ofs);
    ref.body_start = body_start;
    ref.body_end = body_end;
    result.push_back(std::move(ref));

    pos = body_end;
  }

  return result;
}

KrzSample parse_krz_sample(const std::vector<std::uint8_t> &data, const KrzObjectRef &ref) {
  std::uint64_t name_len_area = kObj_NameStart; // ja consumido; meta comeca depois do nome
  // meta_start = body_start + NameStart + (name_ofs-2), mas nao guardamos
  // name_ofs no ref -- refaz a leitura minima aqui.
  std::uint16_t name_ofs = be16u(data.data() + ref.body_start + kObj_NameOfs);
  std::uint64_t meta_start = ref.body_start + name_len_area + (name_ofs >= 2 ? name_ofs - 2 : 0);

  check_bounds(data, meta_start, kSampleMetaSize, "metadado de sample");
  const std::uint8_t *m = data.data() + meta_start;

  KrzSample s;
  s.name = ref.name;
  std::int16_t num_headers = be16s(m + kSample_NumHeaders);
  std::uint8_t flags = m[kSample_Flags];
  s.stereo = (flags == 1);

  int nheaders = static_cast<int>(num_headers) + 1;
  if (nheaders < 1) nheaders = 1;

  std::uint64_t sfh_start = meta_start + kSampleMetaSize;
  check_bounds(data, sfh_start, static_cast<std::uint64_t>(nheaders) * kSoundfileheadSize,
               "Soundfilehead");

  for (int h = 0; h < nheaders; ++h) {
    const std::uint8_t *hp = data.data() + sfh_start + static_cast<std::uint64_t>(h) * kSoundfileheadSize;
    KrzSoundfilehead sfh;
    sfh.root_key = static_cast<std::int8_t>(hp[kSfh_RootKey]);
    std::uint8_t sfh_flags = hp[kSfh_Flags];
    sfh.needs_load = (sfh_flags & kSfhFlag_NeedsLoad) != 0;
    sfh.looped = (sfh_flags & kSfhFlag_LoopOff) == 0;
    sfh.max_pitch_cents = be16s(hp + kSfh_MaxPitch);
    sfh.sample_start_words = be32u(hp + kSfh_SampleStart);
    sfh.sample_loop_start_words = be32u(hp + kSfh_SampleLoopStart);
    sfh.sample_end_words = be32u(hp + kSfh_SampleEnd);
    sfh.sample_period_ns = be32u(hp + kSfh_SamplePeriod);
    s.headers.push_back(sfh);
  }

  // resto ate ref.body_end e o bloco de envelope, de tamanho variavel --
  // deliberadamente nao lido (ver krz_raw_format.hpp).
  return s;
}

std::vector<std::int16_t> krz_extract_pcm(const std::vector<std::uint8_t> &data,
                                           std::uint32_t osize, const KrzSoundfilehead &sfh) {
  if (sfh.sample_end_words < sfh.sample_start_words) return {};
  std::uint64_t frames = static_cast<std::uint64_t>(sfh.sample_end_words - sfh.sample_start_words) + 1;
  std::uint64_t byte_start = static_cast<std::uint64_t>(osize)
      + static_cast<std::uint64_t>(sfh.sample_start_words) * 2;
  check_bounds(data, byte_start, frames * 2, "PCM de sample");

  std::vector<std::int16_t> pcm(frames);
  const std::uint8_t *p = data.data() + byte_start;
  for (std::uint64_t i = 0; i < frames; ++i) {
    pcm[i] = be16s(p + i * 2);
  }
  return pcm;
}

KrzKeymap parse_krz_keymap(const std::vector<std::uint8_t> &data, const KrzObjectRef &ref) {
  std::uint16_t name_ofs = be16u(data.data() + ref.body_start + kObj_NameOfs);
  std::uint64_t meta_start = ref.body_start + kObj_NameStart + (name_ofs >= 2 ? name_ofs - 2 : 0);

  check_bounds(data, meta_start, kKeymapMetaSize + kKeymapLevelCount * 2, "metadado de keymap");
  const std::uint8_t *m = data.data() + meta_start;

  KrzKeymap km;
  km.name = ref.name;
  km.default_sample_id = be16s(m + kKeymap_SampleId);
  std::uint16_t method = be16u(m + kKeymap_Method);
  std::int16_t entries_per_vel = be16s(m + kKeymap_EntriesPerVel);
  std::int16_t entry_size = be16s(m + kKeymap_EntrySize);
  int nkeys = static_cast<int>(entries_per_vel) + 1;
  if (nkeys < 1 || entry_size < 0) {
    throw std::runtime_error("krz: keymap com geometria invalida");
  }

  // So a VeloLevel[0] e usada -- ver comentario em krz_raw_format.hpp.
  std::uint64_t level0_field = meta_start + kKeymap_LevelOffsets;
  std::int16_t level0_ofs = be16s(m + kKeymap_LevelOffsets);
  std::uint64_t level0_start = static_cast<std::uint64_t>(static_cast<std::int64_t>(level0_field)
      + level0_ofs);

  std::uint64_t entries_bytes = static_cast<std::uint64_t>(nkeys) * static_cast<std::uint64_t>(entry_size);
  check_bounds(data, level0_start, entries_bytes, "entradas de keymap");
  const std::uint8_t *ep = data.data() + level0_start;

  km.entries.resize(static_cast<std::size_t>(nkeys));
  for (int k = 0; k < nkeys; ++k) {
    const std::uint8_t *e = ep + static_cast<std::uint64_t>(k) * entry_size;
    std::size_t off = 0;
    KrzKeymapEntry entry;

    if (method & kMethod_Tuning2Byte) {
      entry.tuning_cents = be16s(e + off);
      off += 2;
    } else if (method & kMethod_Tuning1Byte) {
      entry.tuning_cents = static_cast<std::int8_t>(e[off]);
      off += 1;
    }
    if (method & kMethod_VolumeAdjust) {
      off += 1; // nao usado na conversao
    }
    if (method & kMethod_SampleId) {
      entry.sample_id = be16s(e + off);
      off += 2;
    }
    if (method & kMethod_SubsampleNumber) {
      entry.subsample_number = e[off];
      off += 1;
    }

    km.entries[static_cast<std::size_t>(k)] = entry;
  }

  return km;
}

KrzProgram parse_krz_program(const std::vector<std::uint8_t> &data, const KrzObjectRef &ref) {
  std::uint16_t name_ofs = be16u(data.data() + ref.body_start + kObj_NameOfs);
  std::uint64_t p = ref.body_start + kObj_NameStart + (name_ofs >= 2 ? name_ofs - 2 : 0);

  KrzProgram pg;
  pg.name = ref.name;

  KrzLayer *current_layer = nullptr;

  while (p < ref.body_end) {
    check_bounds(data, p, 1, "tag de segmento");
    std::uint8_t tag = data[p];
    if (tag == kSeg_End) break;

    std::size_t len;
    std::uint8_t base = tag & 0xF8;
    if (tag == kSeg_Pgm) {
      len = 15;
    } else if (tag == kSeg_Lyr) {
      len = 15;
    } else if (tag == kSeg_Fx) {
      len = 7;
    } else if (base == kSeg_FunBase) {
      len = 3;
    } else if (base == kSeg_AsrBase || base == kSeg_LfoBase || base == kSeg_KdfxBase) {
      len = 7;
    } else if (base == kSeg_EncBase || base == kSeg_HobBase) {
      len = 15;
    } else if (base == kSeg_CalBase || base == kSeg_Kb3Base) {
      len = 31;
    } else {
      // tag desconhecida -- nao ha como saber o tamanho, para aqui com o
      // que ja foi decodificado (nunca observado em dados reais testados,
      // ver README).
      break;
    }

    check_bounds(data, p + 1, len, "dados de segmento");
    const std::uint8_t *seg = data.data() + p + 1;

    if (tag == kSeg_Pgm) {
      pg.mode = seg[kPgm_Mode];
    } else if (tag == kSeg_Lyr) {
      KrzLayer layer;
      layer.lokey = seg[kLyr_LoKey];
      layer.hikey = seg[kLyr_HiKey];
      layer.stereo = seg[kLyr_StereoMarker] == kLyrStereoMarker_Stereo;
      pg.layers.push_back(layer);
      current_layer = &pg.layers.back();
    } else if (base == kSeg_CalBase && current_layer != nullptr) {
      current_layer->keymap_id = seg[kCal_KeymapId];
    }

    p += 1 + len;
  }

  return pg;
}

} // namespace akai2sfz
