#pragma once
// Layout binario do formato de objeto ".krz" dos Kurzweil K2000/K2500/K2600
// (magic "PRAM"): header + tabela de objetos encadeada PARA TRAS (cada
// bloco comeca com um tamanho negativo cujo modulo da a distancia ate o
// anterior) + pool de amostras de audio compartilhado.
//
// Fontes:
//  - `wav2krz` (Python, xrisw.aeae) -- tradução do projeto Java "KurzFiler"
//    (kurzobjects/, filemethods/kurzweil/, sourceforge.net/projects/kurzfiler).
//    Fonte com maior confianca: escreve arquivos .krz carregaveis em
//    hardware real (inclusive um bugfix de afinação documentado no git,
//    "-1ct bug"). Deu a maior parte dos offsets abaixo.
//  - `krz2wav` (Haxe, github "krz2wav") -- leitor independente, confirma o
//    header PRAM, a cadeia de blocos negativa e o parsing de Sample (tipo
//    38) de forma consistente com o wav2krz.
//
// TODOS os offsets abaixo foram revalidados campo a campo contra um CD real
// (TZMSKRZ.bin+.cue, 31 arquivos .KRZ) -- e essa validacao CORRIGIU dois
// pontos que as fontes de terceiros erravam ou nao documentavam:
//  - O campo keymap_id do segmento CAL (referencia Program->Keymap) fica no
//    byte 12 do payload, nao nos bytes 7-8 como uma fonte secundaria
//    sugeria -- confirmado progredindo identico ao id do sample/keymap em
//    31 layers consecutivas de um program de bateria real (200, 201, 202...).
//  - O tamanho do bloco de envelope de um Sample NAO e fixo em 2x12 bytes
//    (o que o wav2krz sempre escreve) -- em arquivos gravados por hardware
//    real varia (12 ou 14 bytes observados). O limite confiavel e sempre o
//    fim do bloco (`block_size`), nunca uma contagem fixa de envelopes.
// Ver git log para o processo de validacao completo.

#include <cstddef>
#include <cstdint>

namespace akai2sfz::krz_raw {

using u8 = std::uint8_t;

// --- Header do arquivo (32 bytes, big-endian) ---
constexpr std::size_t kHeader_Magic = 0x00; // "PRAM", 4 bytes
constexpr char kMagic[] = "PRAM";
constexpr std::size_t kMagicLen = 4;
constexpr std::size_t kHeader_Osize = 0x04; // u32 BE -- offset onde comeca o pool de audio
constexpr std::size_t kHeaderSize = 32;     // magic(4) + osize(4) + rest[6](24)

// --- Tabela de objetos: cadeia de blocos, cada um comecando com um
// int32 BE. Negativo = bloco de objeto valido, `body_end = pos - block_size`
// (pos = posicao do proprio campo block_size). Zero = fim da tabela (pool
// de audio comeca logo em seguida, coincidindo com `osize`). ---

// Corpo do objeto, a partir de `body_start = pos + 4`:
constexpr std::size_t kObj_Hash = 0x00;    // u16 BE
constexpr std::size_t kObj_Size = 0x02;    // u16 BE -- informativo, nao usado para navegar
constexpr std::size_t kObj_NameOfs = 0x04; // u16 BE -- nome ocupa (NameOfs-2) bytes a partir daqui
constexpr std::size_t kObj_NameStart = 0x06;

// Decodificacao do hash (tipo+id combinados, ver krz2wav):
//   se (hash & 0x8000): type = hash>>10, id = hash & 0x3FF  (espaco de 10 bits)
//   senao:              type = hash>>8,  id = hash & 0xFF   (espaco de 8 bits)
// Sample/Keymap/Program (tipos 38/37/36, todos >=32) sempre caem no
// primeiro caso -- confirmado: hash de "808 Kick" (Sample id=200) =
// (38<<10)+200 = 0x98C8, bit 0x8000 setado.
constexpr std::uint16_t kHashHighBit = 0x8000;

enum class ObjectType : int {
  Program = 36,
  Keymap = 37,
  Sample = 38,
};

// --- Sample (tipo 38): 12 bytes de metadado + N Soundfilehead de 32 bytes
// + bloco de envelope de tamanho VARIAVEL (ver nota acima -- so confiavel
// via `body_end`, nunca assumir 2x12 bytes fixos). ---
constexpr std::size_t kSample_BaseId = 0x00;     // i16 BE, nao usado
constexpr std::size_t kSample_NumHeaders = 0x02; // i16 BE, contagem real = valor+1
constexpr std::size_t kSample_HeadersOfs = 0x04; // i16 BE, nao usado
constexpr std::size_t kSample_Flags = 0x06;      // i8: 0=mono, 1=estereo
constexpr std::size_t kSample_Ks1 = 0x07;        // i8, nao usado
constexpr std::size_t kSample_CopyId = 0x08;     // i16 BE, nao usado
constexpr std::size_t kSample_Ks2 = 0x0A;        // i16 BE, nao usado
constexpr std::size_t kSampleMetaSize = 12;

constexpr std::size_t kSfh_RootKey = 0x00;           // i8, tecla MIDI raiz
constexpr std::size_t kSfh_Flags = 0x01;             // u8: 0x40=needs_load(RAM), 0x80 CLARO=com loop
constexpr std::size_t kSfh_VolumeAdjust = 0x02;      // i8, nao usado
constexpr std::size_t kSfh_AltVolumeAdjust = 0x03;   // i8, nao usado
constexpr std::size_t kSfh_MaxPitch = 0x04;          // i16 BE, cents (root key + fine-tune ja embutidos)
constexpr std::size_t kSfh_OffsetToName = 0x06;      // i16 BE, nao usado
constexpr std::size_t kSfh_SampleStart = 0x08;       // i32 BE, em PALAVRAS de 16 bits, offset em `osize`
constexpr std::size_t kSfh_AltSampleStart = 0x0C;    // i32 BE, nao usado (camada alternativa/velocity)
constexpr std::size_t kSfh_SampleLoopStart = 0x10;   // i32 BE, palavras
constexpr std::size_t kSfh_SampleEnd = 0x14;         // i32 BE, palavras
constexpr std::size_t kSfh_OffsetToEnvelope = 0x18;  // i16 BE, nao usado
constexpr std::size_t kSfh_AltOffsetToEnvelope = 0x1A; // i16 BE, nao usado
constexpr std::size_t kSfh_SamplePeriod = 0x1C;      // i32 BE, nanossegundos por amostra
constexpr std::size_t kSoundfileheadSize = 32;

constexpr u8 kSfhFlag_NeedsLoad = 0x40;
constexpr u8 kSfhFlag_LoopOff = 0x80; // bit CLARO = com loop (invertido!)

// --- Keymap (tipo 37): 12 bytes de metadado + tabela de 8 offsets
// auto-relativos (i16 BE cada) apontando pra ate 8 "VeloLevel" (128
// entradas cada, uma por tecla MIDI 0-127). Offsets podem colidir no mesmo
// endereco fisico (varias zonas de velocidade compartilhando 1 tabela --
// confirmado no CD real: as 8 apontam pro mesmo lugar quando o keymap so
// mapeia 1 sample pra faixa toda). So a VeloLevel[0] e usada aqui -- ver
// riscos no README sobre crossfade de velocidade nao confirmado. ---
constexpr std::size_t kKeymap_SampleId = 0x00;       // i16 BE, sample-id default (usado se method sem bit 0x02)
constexpr std::size_t kKeymap_Method = 0x02;         // i16 BE, bitmask do formato de cada KeymapEntry
constexpr std::size_t kKeymap_BasePitch = 0x04;      // i16 BE, nao usado
constexpr std::size_t kKeymap_CentsPerEntry = 0x06;  // i16 BE, esperado 100
constexpr std::size_t kKeymap_EntriesPerVel = 0x08;  // i16 BE, contagem real = valor+1 (esperado 128)
constexpr std::size_t kKeymap_EntrySize = 0x0A;      // i16 BE, bytes por KeymapEntry (derivado de Method)
constexpr std::size_t kKeymapMetaSize = 12;
constexpr std::size_t kKeymap_LevelOffsets = 0x0C;   // 8 x i16 BE, auto-relativos
constexpr std::size_t kKeymapLevelCount = 8;

// bits de `method` -- tamanho de cada campo por KeymapEntry, NESSA ORDEM:
// tuning -> volume_adjust -> sample_id -> subsample_number.
constexpr std::uint16_t kMethod_Tuning2Byte = 0x10;
constexpr std::uint16_t kMethod_Tuning1Byte = 0x08;
constexpr std::uint16_t kMethod_VolumeAdjust = 0x04;
constexpr std::uint16_t kMethod_SampleId = 0x02;
constexpr std::uint16_t kMethod_SubsampleNumber = 0x01;

// --- Program (tipo 36): sequencia de "segments" tageados (tag de 1 byte +
// dados de tamanho fixo por tag) ate um tag 0x00. Tabela de tamanhos
// validada contra 4 programs reais (1160 segmentos, zero tags
// desconhecidas). ---
constexpr u8 kSeg_Pgm = 8;   // 15 bytes: data[0]=modo(2=K2000,3=K2500,4=K2600) data[1]=numLayers
constexpr u8 kSeg_Lyr = 9;   // 15 bytes: data[3]=lokey data[4]=hikey data[5]=vel_zone data[8]=0x24 estereo/0x04 mono
constexpr u8 kSeg_Fx = 15;   // 7 bytes, nivel de program (nao decodificado)
constexpr u8 kSeg_AsrBase = 16;  // (tag&0xF8) -- 7 bytes
constexpr u8 kSeg_LfoBase = 20;  // (tag&0xF8) -- 7 bytes
constexpr u8 kSeg_FunBase = 24;  // (tag&0xF8) -- 3 bytes
constexpr u8 kSeg_EncBase = 32;  // (tag&0xF8) -- 15 bytes
constexpr u8 kSeg_CalBase = 64;  // (tag&0xF8) -- 31 bytes; data[12]=keymap_id (1 byte)
constexpr u8 kSeg_HobBase = 80;  // (tag&0xF8) -- 15 bytes
constexpr u8 kSeg_KdfxBase = 104; // (tag&0xF8) -- 7 bytes
constexpr u8 kSeg_Kb3Base = 120;  // (tag&0xF8) -- 31 bytes
constexpr u8 kSeg_End = 0;

constexpr std::size_t kPgm_Mode = 0x00;
constexpr std::size_t kPgm_NumLayers = 0x01;
constexpr std::size_t kLyr_LoKey = 0x03;
constexpr std::size_t kLyr_HiKey = 0x04;
constexpr std::size_t kLyr_StereoMarker = 0x08;
constexpr u8 kLyrStereoMarker_Stereo = 0x24;
constexpr u8 kLyrStereoMarker_Mono = 0x04;
constexpr std::size_t kCal_KeymapId = 0x0C; // 1 byte -- ver nota de proveniencia acima

} // namespace akai2sfz::krz_raw
