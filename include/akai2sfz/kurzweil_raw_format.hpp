#pragma once
// Layout binario do container de disco dos CDs Kurzweil K2000/K2500/K2600:
// FAT16 padrao (com ou sem MBR), sem nenhuma extensao proprietaria. Nada
// aqui e "engenharia reversa Kurzweil" -- e o mesmo BPB/FAT16/dentry 8.3 de
// qualquer disquete/HD DOS da epoca; a unica particularidade observada e um
// OEM-name "KMSI" (Kurzweil Music System Inc., conforme a ferramenta
// `kcdread` -- dagargo/kcdread, GPLv2 -- ja notava) no boot sector.
//
// Todos os offsets abaixo foram revalidados contra um CD real
// (TZMSKRZ.bin+.cue, raw MODE1/2352, 30 arquivos .KRZ na raiz): boot sector
// sem MBR (bytes 0x1FE-0x1FF == 0), BPB decodificado (512 B/setor, 32
// setores/cluster, media descriptor 0xF8), diretorio raiz e cadeia de FAT
// lidos e um arquivo .KRZ extraido por inteiro batendo com o tamanho
// declarado no dentry. Ver git log.

#include <cstddef>
#include <cstdint>

namespace akai2sfz::kurzweil_raw {

using u8 = std::uint8_t;

constexpr std::size_t kSectorSize = 512; // BPB exige exatamente isso

// --- Boot sector / BPB (a partir do inicio do volume) ---
// Se bytes 0x1FE-0x1FF do bloco 0 forem 0x55 0xAA, o bloco 0 e uma MBR: o
// volume real comeca no LBA da 1a entrada de particao (offset 0x1C6, 4
// bytes LE, dentro da entrada em 0x1BE). Caso contrario, o bloco 0 e o
// proprio boot sector (volume "super-floppy", sem particionamento).
constexpr std::size_t kMbr_Signature = 0x1FE; // 2 bytes: 0x55 0xAA
constexpr std::size_t kMbr_PartitionTable = 0x1BE;
constexpr std::size_t kMbr_Partition_LbaStart = 0x1C6; // offset dentro da entrada de 16 bytes

constexpr std::size_t kBpb_BytesPerSector = 0x0B;   // u16 LE -- deve ser 512
constexpr std::size_t kBpb_SectorsPerCluster = 0x0D; // u8
constexpr std::size_t kBpb_ReservedSectors = 0x0E;  // u16 LE
constexpr std::size_t kBpb_NumFats = 0x10;          // u8
constexpr std::size_t kBpb_RootEntries = 0x11;      // u16 LE
constexpr std::size_t kBpb_TotalSectors16 = 0x13;   // u16 LE (0 nos CDs Kurzweil)
constexpr std::size_t kBpb_MediaDescriptor = 0x15;  // u8 -- 0xF8 (disco fixo) nos CDs Kurzweil
constexpr std::size_t kBpb_SectorsPerFat = 0x16;    // u16 LE

// --- FAT16: fim de cadeia / livre ---
constexpr std::uint16_t kFatEndMin = 0xFFF8; // 0xFFF8..0xFFFF = fim de cadeia
constexpr std::uint16_t kFatFree = 0x0000;

// --- Entrada de diretorio: 32 bytes, 8.3 classico, sem VFAT/LFN ---
constexpr std::size_t kDirEntrySize = 32;
constexpr std::size_t kDirEntry_Name = 0x00;    // 8 bytes, espaco-preenchido
constexpr std::size_t kDirEntry_Ext = 0x08;     // 3 bytes, espaco-preenchido
constexpr std::size_t kDirEntry_Attr = 0x0B;    // 1 byte
constexpr std::size_t kDirEntry_StartCluster = 0x1A; // u16 LE
constexpr std::size_t kDirEntry_Size = 0x1C;    // u32 LE

constexpr u8 kAttrDirectory = 0x10;
constexpr u8 kAttrVolumeLabel = 0x08;
constexpr u8 kNameFree = 0x00;    // fim do diretorio
constexpr u8 kNameDeleted = 0xE5;
constexpr u8 kNameDot = '.';      // "." / ".."

} // namespace akai2sfz::kurzweil_raw
