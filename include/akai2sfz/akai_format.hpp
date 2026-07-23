#pragma once
// Parser de conteudo dos arquivos S3000 (.a3p/.a3s): sample e program.
//
// Offsets conferidos contra tres fontes independentes que concordam:
//  - Synth/AkaiSample.pm (Perl, Ohsaki) -- ver
//    ../../AKAITOOLS/akaitools-1.5/Synth/AkaiSample.pm
//  - headers reais extraidos de TZIFFXAK01.iso (ver git log deste arquivo)
//  - "Akai sampler disk and file formats", Paul Kellett, 1995-2000
//    (mda.smartelectronix.com/akai/akaiinfo.htm) -- a fonte que resolveu a
//    semantica de loop_start/loop_len (sao em WORDS, nao bytes -- o
//    prototipo Python original dividia por 2 partindo do pressuposto
//    errado de que eram bytes) e revelou o byte de loop_mode explicito em
//    0x13, mais confiavel que a heuristica anterior (loop_times==0xFFFF),
//    que na verdade lia um campo de TEMPO em milissegundos, nao um modo.
//
// S1000 (.a1p/.a1s) ainda nao tem parser de conteudo, mas o mesmo documento
// ja da o layout completo (secoes 5 e 6) -- ver README (M3).

#include <cstdint>
#include <string>
#include <vector>

namespace akai2sfz {

enum class S3000LoopMode : std::uint8_t {
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
  S3000LoopMode loop_mode_raw = S3000LoopMode::None;
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

} // namespace akai2sfz
