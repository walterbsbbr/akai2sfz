#pragma once
// Layout binario do container de disco E-mu ("EMU3") e do formato de bank
// "EMU3 flat" (EMULATOR THREE / EMULATOR 3X / EMU SI-32 v3), comum a
// EIII/EIIIx/ESI-32/EIV (o container de disco e identico nos quatro; o
// formato de bank aqui coberto e o classico, usado por EIII/ESI -- EIV
// tambem sabe ler bancos EMULATOR THREE/3X mas normalmente usa o formato
// encadeado E4B0/EOS, fora de escopo aqui, ver emu_format.hpp).
//
// Fontes:
//  - Container de disco: driver de kernel Linux `emu3fs` (dagargo/emu3fs),
//    struct/offsets de super.c e emu3_fs.h -- terceiros, mas leem/escrevem
//    discos EIII/ESI/EIV reais havia decadas, alta confianca.
//  - Formato de bank: `emu3bm` (dagargo/emu3bm), emu3bm.c/emu3bm.h/sample.h
//    -- mesma familia de autor do emu3fs, idem.
//  - TODOS os offsets abaixo foram revalidados contra um disco real
//    (orbit.iso, "Emu Orbit - The Dance Planet", Emulator III) com um
//    round-trip completo bank->preset->note_zone->zone->sample batendo
//    campo a campo (nomes ASCII, sample_rate=48000, contagem de frames
//    consistente entre canais, endereco de amostra calculado batendo
//    exatamente com o nome da amostra seguinte no arquivo). Ver git log.

#include <cstddef>
#include <cstdint>

namespace akai2sfz::emu_raw {

using u8 = std::uint8_t;

constexpr std::size_t kBlockSize = 0x200; // 512 bytes

// --- Superbloco (bloco 0) ---
constexpr std::size_t kSb_Signature = 0x00; // "EMU3", 4 bytes
constexpr char kSbSignatureText[] = "EMU3";
constexpr std::size_t kSb_BlocksMinus1 = 0x04;         // u32 LE; total_blocks = valor+1
constexpr std::size_t kSb_StartRootBlock = 0x08;       // u32 LE
constexpr std::size_t kSb_RootBlocks = 0x0C;           // u32 LE
constexpr std::size_t kSb_StartDirContentBlock = 0x10; // u32 LE
constexpr std::size_t kSb_DirContentBlocks = 0x14;     // u32 LE
constexpr std::size_t kSb_StartClusterListBlock = 0x18; // u32 LE
constexpr std::size_t kSb_ClusterListBlocks = 0x1C;    // u32 LE
constexpr std::size_t kSb_StartDataBlock = 0x20;       // u32 LE
constexpr std::size_t kSb_Clusters = 0x24;             // u32 LE
constexpr std::size_t kSb_ClusterSizeShiftByte = 0x28; // u8; cluster_size = 1 << (15+valor)

// Nota: o campo "blocks" do superbloco (0x04) pode nao bater com o tamanho
// real da imagem em discos reais (visto em orbit.iso: superbloco declara mais
// blocos do que o arquivo tem) -- o driver emu3fs documenta isso como um
// caso conhecido ("Formula 4000"). So usamos esse campo para fins
// informativos; a leitura real e sempre limitada pelo tamanho real do
// BlockDevice.

// --- Entrada de diretorio: 32 bytes ---
// name[16] + unknown(1) + id(1) + uniao de 14 bytes (arquivo OU pasta)
constexpr std::size_t kDentrySize = 32;
constexpr std::size_t kDentry_Name = 0x00;
constexpr std::size_t kDentry_NameLen = 16;
constexpr std::size_t kDentry_Unknown = 0x10;
constexpr std::size_t kDentry_Id = 0x11;
constexpr std::size_t kDentry_Union = 0x12; // 14 bytes

// Uniao "arquivo" (emu3_file_attrs), a partir de kDentry_Union:
constexpr std::size_t kFattrs_StartCluster = 0x00; // u16 LE, relativo a kDentry_Union
constexpr std::size_t kFattrs_Clusters = 0x02;     // u16 LE
constexpr std::size_t kFattrs_Blocks = 0x04;       // u16 LE
constexpr std::size_t kFattrs_Bytes = 0x06;        // u16 LE
constexpr std::size_t kFattrs_Type = 0x08;         // u8
constexpr std::size_t kFattrs_Props = 0x09;        // 5 bytes (ex.: "\0E3B0" em bancos EIII reais)
constexpr std::size_t kFattrsPropsLen = 5;

// Uniao "pasta" (emu3_dir_attrs), a partir de kDentry_Union: 7 x i16 LE
constexpr std::size_t kDattrs_BlockList = 0x00; // relativo a kDentry_Union
constexpr std::size_t kBlocksPerDir = 7;

constexpr std::size_t kEntriesPerBlock = kBlockSize / kDentrySize; // 16

// tipo de arquivo (kFattrs_Type)
constexpr u8 kFType_Del = 0x00;
constexpr u8 kFType_Sys = 0x80;
constexpr u8 kFType_Std = 0x81;
constexpr u8 kFType_Upd = 0x83;

// marcador de pasta (kDentry_Id de uma entrada de raiz)
constexpr u8 kDType1 = 0x40;
constexpr u8 kDType2 = 0x80;

// --- Cadeia de clusters ("FAT" de 16 bits, indices 1-based) ---
constexpr std::uint16_t kLastFileCluster = 0x7FFF; // fim de cadeia
constexpr std::uint16_t kFreeCluster = 0x0000;

// --- Assinaturas de formato de bank "EMU3 flat" (16 bytes, com \0 final) ---
constexpr char kFmt_EmulatorThree[] = "EMULATOR THREE ";
constexpr char kFmt_Emulator3X[] = "EMULATOR 3X    ";
constexpr char kFmt_EsiV3[] = "EMU SI-32 v3   ";
constexpr std::size_t kFormatSize = 16;

enum class BankFormat { Unknown, EmulatorThree, Emulator3X, EsiV3 };

// --- Cabecalho do bank (struct emu3_bank, 108 bytes) ---
constexpr std::size_t kBank_Format = 0x00; // 16 bytes
constexpr std::size_t kBank_Name = 0x10;   // 16 bytes
constexpr std::size_t kBankHeaderSize = 0x6C; // 108

// Constantes de layout por sub-formato (nomes espelham emu3bm.c exatamente).
constexpr std::size_t kSampleAddrStart_Emu3X = 0x1bd2;
constexpr std::size_t kSampleAddrStart_EmuThree = 0x204;
constexpr std::uint32_t kPresetOffset_EmuThree = 0x1a6fe;
constexpr std::uint32_t kPresetStart_Emu3X = 0x2b72;
constexpr std::uint32_t kPresetStart_EmuThree = 0x74a;
constexpr std::uint32_t kSampleOffset = 0x400000;
constexpr int kMaxSamples_Emu3X = 999;
constexpr int kMaxSamples_EmuThree = 99;
constexpr std::size_t kPresetSizeAddrStart_Emu3X = 0x17ca;
constexpr std::size_t kPresetSizeAddrStart_EmuThree = 0x6c;
constexpr int kMaxPresets_Emu3X = 256;
constexpr int kMaxPresets_EmuThree = 100;

// --- Preset (struct emu3_preset, 142 bytes) ---
constexpr std::size_t kPreset_Name = 0x00;             // 16 bytes
constexpr std::size_t kPreset_RtControls = 0x10;       // 12 bytes (nao usado na conversao)
constexpr std::size_t kPreset_Unknown0 = 0x1C;         // 16 bytes
constexpr std::size_t kPreset_PitchBendRange = 0x2C;
constexpr std::size_t kPreset_VelPriLow = 0x2D;
constexpr std::size_t kPreset_VelPriHigh = 0x2E;
constexpr std::size_t kPreset_VelSecLow = 0x2F;
constexpr std::size_t kPreset_VelSecHigh = 0x30;
constexpr std::size_t kPreset_LinkLsb = 0x31;
constexpr std::size_t kPreset_LinkMsb = 0x32;
constexpr std::size_t kPreset_Unknown1 = 0x33; // 2 bytes
constexpr std::size_t kPreset_NoteZonesCount = 0x35;
constexpr std::size_t kPreset_NoteZoneMappings = 0x36; // 88 bytes
constexpr std::size_t kPresetSize = 142; // 0x8E

constexpr std::size_t kNumNotes = 88; // teclas fisicas E-mu
constexpr int kMidiNoteOffset = 21;   // nota E-mu 0 = MIDI 21 (A-1)
constexpr u8 kNoteZoneUnmapped = 0xFF;

// --- Note-zone (struct emu3_preset_note_zone, 4 bytes) ---
constexpr std::size_t kNoteZoneSize = 4;
constexpr std::size_t kNoteZone_OptLsb = 0x00;
constexpr std::size_t kNoteZone_OptMsb = 0x01;
constexpr std::size_t kNoteZone_PriZone = 0x02;
constexpr std::size_t kNoteZone_SecZone = 0x03;
constexpr u8 kZoneUnused = 0xFF; // pri_zone/sec_zone: sem camada

// --- Zona (struct emu3_preset_zone, 48 bytes) ---
constexpr std::size_t kZoneSize = 48; // 0x30
constexpr std::size_t kZone_OriginalKey = 0x00;
constexpr std::size_t kZone_SampleIdLsb = 0x01;
constexpr std::size_t kZone_SampleIdMsb = 0x02;
constexpr std::size_t kZone_ParamA = 0x03;
constexpr std::size_t kZone_VcaEnvelope = 0x04; // 5 bytes: A,H,D,S,R
constexpr std::size_t kZone_LfoRate = 0x09;
constexpr std::size_t kZone_LfoDelay = 0x0A;
constexpr std::size_t kZone_LfoVariation = 0x0B;
constexpr std::size_t kZone_VcfCutoff = 0x0C;
constexpr std::size_t kZone_VcfQ = 0x0D;
constexpr std::size_t kZone_VcfEnvAmount = 0x0E;
constexpr std::size_t kZone_VcfEnvelope = 0x0F; // 5 bytes
constexpr std::size_t kZone_AuxEnvelope = 0x14; // 5 bytes
constexpr std::size_t kZone_AuxEnvAmount = 0x19;
constexpr std::size_t kZone_AuxEnvDest = 0x1A;
constexpr std::size_t kZone_VelToAuxEnv = 0x1B;
constexpr std::size_t kZone_VelToVcaLevel = 0x1C;
constexpr std::size_t kZone_VelToVcaAttack = 0x1D;
constexpr std::size_t kZone_VelToPitch = 0x1E;
constexpr std::size_t kZone_VelToPan = 0x1F;
constexpr std::size_t kZone_VelToVcfCutoff = 0x20;
constexpr std::size_t kZone_VelToVcfQ = 0x21;
constexpr std::size_t kZone_VelToVcfAttack = 0x22;
constexpr std::size_t kZone_VelToSampleStart = 0x23;
constexpr std::size_t kZone_LfoToPitch = 0x24;
constexpr std::size_t kZone_LfoToVca = 0x25;
constexpr std::size_t kZone_LfoToCutoff = 0x26;
constexpr std::size_t kZone_LfoToPan = 0x27;
constexpr std::size_t kZone_VcaLevel = 0x28;
constexpr std::size_t kZone_NoteTuning = 0x29;
constexpr std::size_t kZone_VcfTracking = 0x2A;
constexpr std::size_t kZone_NoteOnDelay = 0x2B;
constexpr std::size_t kZone_VcaPan = 0x2C;
constexpr std::size_t kZone_VcfTypeLfoShape = 0x2D;
constexpr std::size_t kZone_RtEnableFlags = 0x2E;
constexpr std::size_t kZone_Flags = 0x2F;

// --- Sample (struct emu3_sample, cabecalho de 92 bytes + PCM16 intercalado
// por canal -- esquerdo inteiro seguido de direito inteiro, NAO intercalado
// L/R como WAV) ---
constexpr std::size_t kSampleHeaderSize = 92; // 0x5C
constexpr std::size_t kSample_Name = 0x00;    // 16 bytes
constexpr std::size_t kSample_Header = 0x10;
constexpr std::size_t kSample_StartL = 0x14;
constexpr std::size_t kSample_StartR = 0x18;
constexpr std::size_t kSample_EndL = 0x1C;
constexpr std::size_t kSample_EndR = 0x20;
constexpr std::size_t kSample_LoopStartL = 0x24;
constexpr std::size_t kSample_LoopStartR = 0x28;
constexpr std::size_t kSample_LoopEndL = 0x2C;
constexpr std::size_t kSample_LoopEndR = 0x30;
constexpr std::size_t kSample_SampleRate = 0x34;
constexpr std::size_t kSample_PlaybackRate = 0x38; // u16, nao usado (so relevante <44100Hz)
constexpr std::size_t kSample_Options = 0x3A;      // u16, ver bits abaixo

constexpr std::uint16_t kOpt_Loop = 0x0001;
constexpr std::uint16_t kOpt_LoopRelease = 0x0008;
constexpr std::uint16_t kOpt_MonoL = 0x0020;
constexpr std::uint16_t kOpt_MonoR = 0x0040;
constexpr std::uint16_t kOpt_Stereo = kOpt_MonoL | kOpt_MonoR;

} // namespace akai2sfz::emu_raw
