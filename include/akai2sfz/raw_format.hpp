#pragma once
// Layout binario do filesystem Akai S1000/S3000 (particao/harddisk/CD-ROM).
//
// Offsets e constantes conferidos byte a byte contra duas fontes independentes:
//   - akaiutil.h / akaiutil.cc (C), Klaus Michael Indlekofer, 2008-2021 (GPLv2)
//     ver ../../akai-fs/include/akaiutil/akaiutil.h
//   - Synth/AkaiDisk.pm (Perl), Hiroyuki Ohsaki, 1997-2024 (GPLv2)
//     ver ../../AKAITOOLS/akaitools-1.5/Synth/AkaiDisk.pm
//
// As duas implementacoes concordam em todos os offsets criticos (DIR_ENTRY_OFFSET
// 0xCA, FAT_OFFSET 0x70A, PARTHEAD_OFFSET 0x4400, PARTTABLE_OFFSET 0x4500), o que
// da alta confianca nesse layout. Este arquivo nao tenta cobrir S900, disquetes ou
// particoes DD (fora do escopo de "imagem de CD S1000/S3000" para SFZ) -- ver README.

#include <cstdint>
#include <cstddef>

namespace akai2sfz::raw {

using u8 = std::uint8_t;

// --- blocos ---
constexpr std::size_t kHdBlockSize = 0x2000; // 8 KiB, blocksize de harddisk/CD

// --- volume directory entry (arquivo dentro de um volume) ---
// struct akai_voldir_entry_s: name[12] tag[4] type[1] size[3] start[2] osver[2] = 24 bytes
constexpr std::size_t kNameLen = 12;
constexpr std::size_t kFileEntrySize = 24;
constexpr std::size_t kFileEntry_Name = 0;
constexpr std::size_t kFileEntry_Tag = 12; // 4 bytes
constexpr std::size_t kFileEntry_Type = 16;
constexpr std::size_t kFileEntry_Size = 17; // 3 bytes, 24bit little-endian
constexpr std::size_t kFileEntry_Start = 20; // 2 bytes
constexpr std::size_t kFileEntry_OsVer = 22; // 2 bytes

constexpr u8 kFileTypeFree = 0x00;

// --- root directory entry (volume dentro de uma particao) ---
// struct akai_rootdir_entry_s: name[12] type[1] lnum[1] start[2] = 16 bytes
constexpr std::size_t kRootEntrySize = 16;
constexpr std::size_t kRootEntry_Name = 0;
constexpr std::size_t kRootEntry_Type = 12;
constexpr std::size_t kRootEntry_Lnum = 13;
constexpr std::size_t kRootEntry_Start = 14; // 2 bytes

enum class VolType : u8 {
  Inactive = 0x00,
  S1000 = 0x01,
  S3000 = 0x03,
  Cd3000 = 0x07, // CD3000 CD-ROM, compativel com S3000
};

// --- volume directory sizes (numero de entradas de arquivo) ---
constexpr std::size_t kVoldirEntries_S1000Hd = 126;
constexpr std::size_t kVoldirEntries_S3000Hd = 510;
constexpr std::size_t kVoldirEntries1Blk = 341; // quantas cabem no 1o bloco (341*24=8184 <= 8192)
constexpr std::size_t kVolparamSize = 0x30; // 48 bytes, nao decodificado (nao usado por akai2sfz)

// --- sampler partition header (akai_parthead_s), 3 blocos = 0x6000 bytes ---
constexpr std::size_t kPartheadBlocks = 3;
constexpr std::size_t kPartheadSize = kPartheadBlocks * kHdBlockSize;

constexpr std::size_t kParthead_Size = 0x0000;      // 2 bytes: tamanho da particao em blocos
constexpr std::size_t kParthead_Magic = 0x0002;     // 98 * 2 bytes
constexpr std::size_t kParthead_MagicCount = 98;
constexpr std::uint32_t kParthead_MagicVal = 3333;  // magic[i] == (i * 3333) & 0xFFFF
constexpr std::size_t kParthead_Chksum = 0x00C6;    // 4 bytes: bsize + soma(magic), little-endian
constexpr std::size_t kParthead_Vol = 0x00CA;       // 100 * 16 bytes: root directory de volumes
constexpr std::size_t kParthead_VolCount = 100;
constexpr std::size_t kParthead_Fat = 0x070A;       // FAT da particao, 2 bytes/entrada
constexpr std::size_t kParthead_FatMaxEntries = 0x1E00; // 7680, tamanho maximo de particao em blocos
constexpr std::size_t kParthead_PartTab = 0x4400;   // tabela de particoes (so na 1a particao sampler)
constexpr std::size_t kParthead_PartTabTable = 0x4500;

// --- codigos especiais de FAT ---
constexpr std::uint16_t kFatFree = 0x0000;
constexpr std::uint16_t kFatBad = 0x2000;
constexpr std::uint16_t kFatSys = 0x4000;         // bloco reservado p/ sistema (S1000/S3000)
constexpr std::uint16_t kFatDirEnd1000Hd = 0x4000; // fim de cadeia p/ diretorio de volume S1000 (== kFatSys!)
constexpr std::uint16_t kFatDirEnd3000 = 0x8000;   // fim de cadeia p/ diretorio de volume S3000
constexpr std::uint16_t kFatFileEnd = 0xC000;      // fim de cadeia p/ arquivo (S1000/S3000)

// marcador de "sem mais particoes", usado ao varrer sequencialmente uma imagem
constexpr std::uint16_t kPartEndMark = 0x8000;

// --- CD-ROM info header (akai_cdinfohead_s), logo apos o partition header ---
// fnum[2] volesiz[100][2] cdlabel[12] = 214 bytes (0xD6)
constexpr std::size_t kCdInfoBlock = kPartheadBlocks; // bloco 3 dentro da particao
constexpr std::size_t kCdInfo_Fnum = 0x0000;
constexpr std::size_t kCdInfo_VolESiz = 0x0002; // 100 * 2 bytes
constexpr std::size_t kCdInfo_CdLabel = 0x00CA; // 12 bytes (2 + 100*2 = 202 = 0xCA)

} // namespace akai2sfz::raw
