#pragma once
// Parser de conteudo dos arquivos Akai: sample e program, para S3000
// (.a3p/.a3s) e S1000 (.a1p/.a1s).
//
// Offsets conferidos contra tres fontes independentes que concordam:
//  - Synth/AkaiSample.pm (Perl, Ohsaki) -- ver
//    ../../AKAITOOLS/akaitools-1.5/Synth/AkaiSample.pm (so S3000)
//  - headers reais extraidos de TZIFFXAK01.iso e RMD2.iso (ver git log)
//  - "Akai sampler disk and file formats", Paul Kellett, 1995-2000
//    (mda.smartelectronix.com/akai/akaiinfo.htm, copia local em
//    ../../588909048-akai-disk-file-formats-pdf.pdf) -- unica fonte com o
//    layout completo do S1000 (secoes 5-6). Revelou tambem que
//    loop_start/loop_len sao em WORDS (nao bytes -- o prototipo Python
//    dividia por 2 por engano) e que existe um byte de loop_mode explicito
//    em 0x13, mais confiavel que inferir do campo de tempo em 0x30.
//
// Achado central do M3: o layout de sample e keygroup do S1000 (150 bytes)
// e o do S3000 (192 bytes) sao byte-a-byte identicos em tudo que o S1000
// tambem tem -- key@0x02, name@0x03, low_key/high_key do keygroup em
// 0x03/0x04, zonas de velocidade em 0x22/0x3A/0x52/0x6A (mesmo stride de 24
// bytes) com sample_name@+0/low_vel@+12/high_vel@+13/loudness@+16/pan@+18.
// S3000 so acrescenta 42 bytes no final de cada registro. A diferenca real
// esta na afinacao (S1000 usa dois bytes signed separados -- cents e
// semitons -- em vez do fixed-point de 16 bits do S3000).

#include <cstdint>
#include <string>
#include <vector>

namespace akai2sfz {

// Compartilhado entre S1000 e S3000 -- mesmos 4 valores nos dois formatos.
enum class AkaiLoopMode : std::uint8_t {
  InRelease = 0,   // continua looping durante o release
  UntilRelease = 1, // faz loop ate o release comecar (~ SFZ loop_sustain)
  None = 2,
  PlayToEnd = 3,
};

struct S3000Sample {
  std::string name;
  std::uint8_t key = 60;     // tecla raiz (nota MIDI)
  double tune = 0.0;         // afinacao fina, em semitons
  std::uint32_t size_words = 0; // numero de samples (words) declarado no header
  std::uint16_t rate = 44100;
  std::uint32_t start = 0;      // marcador de start, em words
  std::uint32_t end = 0;        // marcador de end, em words
  std::uint8_t num_active_loops = 0;
  std::uint8_t first_active_loop = 0; // 0 = nenhum loop ativo
  AkaiLoopMode loop_mode_raw = AkaiLoopMode::None;
  std::uint32_t loop_start = 0; // em words -- NAO dividir por 2
  std::uint32_t loop_len = 0;   // em words -- NAO dividir por 2
  std::uint16_t loop_time_ms = 0; // tempo do loop em ms (nao e um contador)
  std::vector<std::int16_t> pcm; // amostras 16-bit signed, mono

  // true se o sample de fato usa loop (considera num_active_loops,
  // first_active_loop e loop_mode_raw==None -- ver akai_format.cpp).
  bool has_loop() const;
};

// Uma zona de velocidade dentro de um keygroup. Um keygroup S3000 tem ate 4
// zonas (offsets 0x22, 0x3A, 0x52, 0x6A dentro do registro de 192 bytes,
// conferido contra GRV070-1.a3p real); zonas nao usadas tem sample_name vazio.
// Na pratica, pares estereo Akai costumam usar 2 zonas com a MESMA faixa de
// velocidade (0-127) para o canal L e R tocando juntos, nao velocity-switch.
struct S3000Zone {
  std::string sample_name;
  std::uint8_t low_vel = 0;
  std::uint8_t high_vel = 127;
  std::int8_t vol_offset = 0; // dB
  std::int8_t pan_offset = 0; // -50..+50
};

struct S3000Keygroup {
  std::uint8_t low_key = 0;
  std::uint8_t high_key = 127;
  double tune = 0.0;        // semitons
  std::uint8_t pitch = 60;  // usado para transpose = pitch-60
  std::vector<S3000Zone> zones; // 1 a 4 zonas com sample_name nao vazio
};

struct S3000Program {
  std::string name;
  std::uint8_t midi_prog = 0;
  std::uint8_t midi_chan = 0;
  std::uint8_t low_key = 0;
  std::uint8_t high_key = 127;
  std::vector<S3000Keygroup> keygroups;
};

// `raw` e o conteudo bruto de um arquivo .a3s (header de 192 bytes + PCM).
S3000Sample parse_s3000_sample(const std::vector<std::uint8_t> &raw);

// `raw` e o conteudo bruto de um arquivo .a3p.
S3000Program parse_s3000_program(const std::vector<std::uint8_t> &raw);

// --- S1000 (.a1p/.a1s) ---
//
// Estrutura identica a S3000 (mesmos campos), mas o header/keygroup e menor
// (150 bytes) e a afinacao vem em dois bytes signed separados (cents e
// semitons) em vez do fixed-point de 16 bits do S3000 -- ver doc Kellett,
// secoes 5-6. Tipos separados de S3000Sample/S3000Keygroup/S3000Program
// porque a semantica de tune difere e porque os dois formatos evoluiram
// como codigos irmaos, nao um caso especial um do outro.

struct S1000Sample {
  std::string name;
  std::uint8_t key = 60;
  double tune = 0.0; // semitons (cents_byte/100.0 + semi_byte -- ver .cpp)
  std::uint32_t size_words = 0;
  std::uint16_t rate = 44100;
  std::uint32_t start = 0;
  std::uint32_t end = 0;
  std::uint8_t num_active_loops = 0;
  AkaiLoopMode loop_mode_raw = AkaiLoopMode::None;
  std::uint32_t loop_start = 0; // em words
  std::uint32_t loop_len = 0;   // em words
  std::uint16_t loop_time_ms = 0;
  std::vector<std::int16_t> pcm;

  bool has_loop() const;
};

// Zona de velocidade dentro de um keygroup S1000 -- mesmos offsets do
// S3000 (stride de 24 bytes a partir de 0x22), mais um loop_mode por zona
// (offset +19, nao presente no S3000Zone) documentado explicitamente pelo
// doc Kellett para o S1000.
struct S1000Zone {
  std::string sample_name;
  std::uint8_t low_vel = 0;
  std::uint8_t high_vel = 127;
  std::int8_t loudness = 0; // dB
  std::int8_t pan = 0;      // -50..+50
  // "key tracking" do keygroup (offset 0x84+indice da zona, 0=TRACK
  // 1=FIXED, doc Kellett). Quando true, a amostra deve tocar sempre na
  // mesma altura, ignorando a tecla raiz do sample -- comum em kits de
  // bateria onde varias zonas de velocidade de uma mesma batida sao
  // mapeadas numa unica tecla (ex.: RMD2.iso/DRYKIT01, ver git log).
  bool fixed_pitch = false;
};

struct S1000Keygroup {
  std::uint8_t low_key = 0;
  std::uint8_t high_key = 127;
  double tune = 0.0; // semitons
  std::vector<S1000Zone> zones; // ate 4, conforme "vel zones used" (0x1F)
};

struct S1000Program {
  std::string name;
  std::uint8_t midi_prog = 0;
  std::uint8_t midi_chan = 0;
  std::uint8_t low_key = 24;
  std::uint8_t high_key = 127;
  std::vector<S1000Keygroup> keygroups;
};

// `raw` e o conteudo bruto de um arquivo .a1s (header de 150 bytes + PCM).
S1000Sample parse_s1000_sample(const std::vector<std::uint8_t> &raw);

// `raw` e o conteudo bruto de um arquivo .a1p.
S1000Program parse_s1000_program(const std::vector<std::uint8_t> &raw);

} // namespace akai2sfz
