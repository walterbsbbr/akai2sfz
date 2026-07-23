#pragma once
// Parser do formato de objeto .krz -- so o parsing "cru" (struct a struct)
// mora aqui, sem decisao de SFZ (mesmo padrao de emu_format.hpp/
// roland_format.hpp vs os respectivos converters).

#include <cstdint>
#include <string>
#include <vector>

namespace akai2sfz {

// Valida o magic "PRAM" e devolve `osize` (offset onde comeca o pool de
// amostras de audio). Lanca std::runtime_error se o magic nao bater.
std::uint32_t krz_read_osize(const std::vector<std::uint8_t> &data);

// Uma entrada na tabela de objetos (posicao ja resolvida, pronta pra
// reparsear via parse_krz_sample/keymap/program). `type_raw` preserva o
// valor de tipo mesmo quando nao e Sample/Keymap/Program (objetos de outro
// tipo aparecem na cadeia mas nao sao decodificados).
struct KrzObjectRef {
  int type_raw = 0;
  int id = 0;
  std::string name;
  std::uint64_t body_start = 0; // logo apos hash/size/name -- inicio do payload especifico do tipo
  std::uint64_t body_end = 0;   // exclusivo -- unico limite confiavel (ver krz_raw_format.hpp)
};

// Percorre a cadeia de blocos inteira (a partir do offset 32) e devolve
// todos os objetos encontrados, na ordem em que aparecem no arquivo (que
// pode nao ser a ordem de criacao -- nao assumir nada sobre ordenacao).
std::vector<KrzObjectRef> list_krz_objects(const std::vector<std::uint8_t> &data);

struct KrzSoundfilehead {
  int root_key = 60;
  bool needs_load = false;
  bool looped = false;
  int max_pitch_cents = 0;
  std::uint32_t sample_start_words = 0;
  std::uint32_t sample_loop_start_words = 0;
  std::uint32_t sample_end_words = 0;
  std::uint32_t sample_period_ns = 0;
};

struct KrzSample {
  std::string name;
  bool stereo = false;
  std::vector<KrzSoundfilehead> headers; // 1 (mono) ou 2 (estereo: [0]=L, [1]=R)
};

KrzSample parse_krz_sample(const std::vector<std::uint8_t> &data, const KrzObjectRef &ref);

// PCM de um Soundfilehead: 16-bit linear PCM, big-endian no arquivo,
// devolvido ja em ordem nativa (host). Le [osize+start*2, osize+(end+1)*2).
std::vector<std::int16_t> krz_extract_pcm(const std::vector<std::uint8_t> &data,
                                           std::uint32_t osize, const KrzSoundfilehead &sfh);

struct KrzKeymapEntry {
  int sample_id = -1;      // -1 = usa KrzKeymap::default_sample_id
  int subsample_number = 0; // 0 = "nao usado" (so relevante quando ha varios headers/subsamples)
  double tuning_cents = 0.0;
};

struct KrzKeymap {
  std::string name;
  int default_sample_id = -1;
  // 128 entradas, indice = tecla MIDI (0-127) -- so a VeloLevel[0] e usada
  // (crossfade de velocidade entre VeloLevels 1-7 nao confirmado, ver README).
  std::vector<KrzKeymapEntry> entries;
};

KrzKeymap parse_krz_keymap(const std::vector<std::uint8_t> &data, const KrzObjectRef &ref);

struct KrzLayer {
  int lokey = 0;
  int hikey = 127;
  bool stereo = false;
  int keymap_id = -1; // -1 = CAL nao encontrado/nao decodificado
};

struct KrzProgram {
  std::string name;
  int mode = 0; // 2=K2000, 3=K2500, 4=K2600 (0 = nao encontrado)
  std::vector<KrzLayer> layers;
};

KrzProgram parse_krz_program(const std::vector<std::uint8_t> &data, const KrzObjectRef &ref);

} // namespace akai2sfz
