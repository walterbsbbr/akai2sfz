#pragma once
// Layout binario do filesystem Roland S-750/S-760/S-770 (HD/MO/CD-ROM).
//
// Fonte: "SYS-772 Ver.2.0 for S-750/770 Hard Disk/MO Disk/CD-ROM Format
// Manual", revisao 1.00, 7 de maio de 1991, R&D J-1, Roland Electronics
// Corp. -- documento de engenharia do proprio fabricante, nao engenharia
// reversa de terceiros. Copia local em
// ../../../ROLAND/Roland-S750-CDROM-Format/Roland-CDROM-S750.pdf
//
// Todos os limites de area abaixo foram recomputados (contagem de entradas
// x tamanho de entrada = blocos declarados) e batem exatamente com o
// manual. A assinatura do ID area ('S770 MR25A' em 0x04) e o campo de
// capacidade (0x110) foram confirmados contra 3 CDs reais (ver git log) --
// curiosamente os discos se identificam como "S-760 System Disk" (a versao
// rack do S-770, mesma familia de formato).
//
// Diferente do Akai, nao ha conceito de multiplas particoes por imagem: um
// disco Roland tem uma unica ID area, uma unica FAT e um unico conjunto de
// 5 diretorios/5 areas de parametro.

#include <cstdint>
#include <cstddef>

namespace akai2sfz::roland_raw {

using u8 = std::uint8_t;

constexpr std::size_t kBlockSize = 0x200; // 512 bytes
constexpr std::size_t kClusterBlocks = 18; // 1 cluster = 18 blocos = 9216 bytes

// --- ID area (bloco 0) ---
constexpr std::size_t kId_Signature = 0x04; // "S770 MR25A", 10 bytes (mesma p/ S-750/760/770)
constexpr std::size_t kId_SignatureLen = 10;
constexpr char kId_SignatureText[] = "S770 MR25A";
constexpr std::size_t kId_DriveName = 0x100; // 16 bytes
constexpr std::size_t kId_Capacity = 0x110;  // 4 bytes LE, em blocos
constexpr std::size_t kId_FileNumVolume = 0x114;      // 2 bytes LE
constexpr std::size_t kId_FileNumPerformance = 0x116; // 2 bytes LE
constexpr std::size_t kId_FileNumPatch = 0x118;       // 2 bytes LE
constexpr std::size_t kId_FileNumPartial = 0x11A;     // 2 bytes LE
constexpr std::size_t kId_FileNumSample = 0x11C;      // 2 bytes LE

// --- FAT ---
constexpr std::size_t kFatStartBlock = 0x0404;
constexpr std::size_t kFatBlocks = 0x0100;
constexpr std::uint16_t kFatIdMark = 0xFFFA;
constexpr std::uint16_t kFatFree = 0x0000;
constexpr std::uint16_t kFatReserved = 0x0001;
constexpr std::uint16_t kFatBad = 0xFFF7;
constexpr std::uint16_t kFatEndMin = 0xFFF8; // FFF8h..FFFFh = fim de cadeia

// --- tipos de arquivo (byte 0x10 da entrada de diretorio) ---
enum class FileType : u8 {
  Volume = 0x40,
  Performance = 0x41,
  Patch = 0x42,
  Partial = 0x43,
  Sample = 0x44,
};

// --- entrada de diretorio: 32 bytes ---
constexpr std::size_t kDirEntrySize = 32;
constexpr std::size_t kDirEntry_Name = 0x00;    // 16 bytes ASCII, espaco-preenchido
constexpr std::size_t kDirEntry_NameLen = 16;
constexpr std::size_t kDirEntry_Type = 0x10;    // 1 byte, ver FileType
constexpr std::size_t kDirEntry_Attr = 0x11;    // 1 byte (ignorado)
// offsets 0x12-0x1B: forward/backward link, link number, program change --
// nao usados para leitura (so relevantes para escrita/gerenciamento pelo
// sampler); layout exato nao totalmente confirmado, ver README.
constexpr std::size_t kDirEntry_FatEntry = 0x1C;  // 2 bytes LE -- so Sample: 1o bloco da FAT
constexpr std::size_t kDirEntry_Capacity = 0x1E;  // 2 bytes LE -- so Sample: tamanho em clusters
constexpr u8 kNameVirginal = 0x00; // byte 0 do nome: slot nunca usado
constexpr u8 kNameDeleted = 0xFE;  // byte 0 do nome: arquivo apagado

struct DirAreaInfo {
  std::size_t start_block;
  std::size_t entry_count;
};

// Diretorio: 5 tabelas planas (sem cadeia de FAT -- indexacao direta).
constexpr DirAreaInfo kDirVolume{0x0504, 128};
constexpr DirAreaInfo kDirPerformance{0x050C, 512};
constexpr DirAreaInfo kDirPatch{0x052C, 1024};
constexpr DirAreaInfo kDirPartial{0x056C, 4096};
constexpr DirAreaInfo kDirSample{0x066C, 8192};

struct ParamAreaInfo {
  std::size_t start_block;
  std::size_t entry_count;
  std::size_t entry_size; // bytes
};

// Parametros: mesma indexacao 1:1 da entrada de diretorio correspondente
// (Patch #N tem seus parametros sempre no slot N desta area -- nao ha
// alocacao dinamica aqui, so as amostras de audio usam a FAT).
constexpr ParamAreaInfo kParamVolume{0x086C, 128, 256};
constexpr ParamAreaInfo kParamPerformance{0x08AC, 512, 512};
constexpr ParamAreaInfo kParamPatch{0x0AAC, 1024, 512};
constexpr ParamAreaInfo kParamPartial{0x0EAC, 4096, 128};
constexpr ParamAreaInfo kParamSample{0x12AC, 8192, 48};

constexpr std::size_t kWaveDataStartBlock = 0x15AC;

// --- Volume parameter (256 bytes) ---
constexpr std::size_t kVolParam_Name = 0x00; // 16 bytes
constexpr std::size_t kVolParam_PerformanceList = 0x20; // 64 x 2 bytes (indices 1-based, 0=vazio)
constexpr std::size_t kVolParam_PerformanceListCount = 64;

// --- Performance parameter (512 bytes: 256 de parametro + 64 de patch list + reservado) ---
constexpr std::size_t kPerfParam_Name = 0x00; // 16 bytes
constexpr std::size_t kPerfParam_PatchSel = 0x10; // 32 bytes, 1 byte por parte (1-32)
constexpr std::size_t kPerfParam_MidiCh = 0x30;   // 16 bytes, 2 partes por byte (nibble)
constexpr std::size_t kPerfParam_PatchList = 0x100; // 32 x 2 bytes
constexpr std::size_t kPerfParam_PatchListCount = 32;

// --- Patch parameter (512 bytes: 256 de parametro + 176 de partial list + reservado) ---
constexpr std::size_t kPatchParam_Name = 0x00; // 16 bytes
// "Partial Sel" (88 bytes, 1 por tecla): proposito real nao confirmado.
// Hipotese inicial (indice local para indireccao em Partial List) foi
// refutada contra dados reais -- o valor observado (ex.: 0xFF) nao bate com
// o indice de fato usado. Mantido aqui so como referencia de offset.
constexpr std::size_t kPatchParam_PartialSel = 0x20; // 88 bytes, 1 por tecla
constexpr std::size_t kPatchParam_PartialSelCount = 88;
// "Partial List" (88 x 2 bytes, 1 por tecla): indice 0-based DIRETO na
// Partial directory; 0xFFFF = tecla sem som. Confirmado contra
// KIK:Gretsch Kik5 em Roland S770 Drum Samples.iso (a unica tecla ativa,
// C2, resolve exatamente para o partial 'KIK:Gretsch kik5').
constexpr std::size_t kPatchParam_PartialList = 0x100; // 88 x 2 bytes
constexpr std::size_t kPatchParam_PartialListCount = 88;
constexpr std::uint16_t kPartialListUnused = 0xFFFF;

// --- Partial parameter (128 bytes) ---
constexpr std::size_t kPartialParam_Name = 0x00; // 16 bytes
// 4 slots de sample (velocity layers), 16 bytes cada, a partir de 0x10
constexpr std::size_t kPartialParam_SampleBase = 0x10;
constexpr std::size_t kPartialParam_SampleStride = 16;
constexpr std::size_t kPartialParam_SampleCount = 4;
// offsets relativos dentro de cada slot de sample (ver manual pagina 9-10)
// 2 bytes: indice 0-based DIRETO na Sample directory; 0xFFFF = slot vazio
// (mesma convencao de kPatchParam_PartialList, confirmada contra dados reais).
constexpr std::size_t kPSample_Sel = 0x00;
constexpr std::size_t kPSample_PitchKF = 0x02;
constexpr std::size_t kPSample_Level = 0x03;
constexpr std::size_t kPSample_Pan = 0x04;
constexpr std::size_t kPSample_CoarseTune = 0x05;
constexpr std::size_t kPSample_FineTune = 0x06;
constexpr std::size_t kPSample_VelLower = 0x07;
constexpr std::size_t kPSample_LowerFadeWidth = 0x08;
constexpr std::size_t kPSample_VelUpper = 0x09;
constexpr std::size_t kPSample_UpperFadeWidth = 0x0A;

// --- Sample parameter (48 bytes) ---
constexpr std::size_t kSampleParam_Name = 0x00; // 16 bytes
// 4 pontos de 4 bytes cada: fine(1) + endereco 24-bit LE (address, address+1, MSB)
constexpr std::size_t kSampleParam_StartPoint = 0x10;
constexpr std::size_t kSampleParam_SLoopStart = 0x14;
constexpr std::size_t kSampleParam_SLoopEnd = 0x18;
constexpr std::size_t kSampleParam_RLoopStart = 0x1C;
constexpr std::size_t kSampleParam_RLoopEnd = 0x20;
constexpr std::size_t kSampleParam_LoopMode = 0x24;      // 0=Fwd 1=Alt 2=1Shot 3=Reverse (ver *1 abaixo)
constexpr std::size_t kSampleParam_SLoopEnable = 0x25;
constexpr std::size_t kSampleParam_SLoopTune = 0x26;
constexpr std::size_t kSampleParam_RLoopTune = 0x27;
constexpr std::size_t kSampleParam_SegTop = 0x28;
constexpr std::size_t kSampleParam_SegLen = 0x29;
// O manual lista "(5) Sample Frequency/Mode" antes de "(6) Original Key",
// mas os dados reais mostram a ordem invertida no disco -- confirmado
// contra GTR:E3 Tap Hrm 1 em Roland Hans Zimmer Guitars I.iso: o byte em
// 0x2A vale 52 (MIDI E3, batendo com o nome da propria amostra), nao 0x2B
// como a leitura literal do manual sugeriria.
constexpr std::size_t kSampleParam_OriginalKey = 0x2A;
constexpr std::size_t kSampleParam_FreqMode = 0x2B; // sample rate / sample mode
// offset 0x2C-0x2F: (ignore)

} // namespace akai2sfz::roland_raw
