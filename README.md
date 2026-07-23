# akai2sfz

Conversor nativo em C++ de imagens de CD/HD dos samplers **Akai S900/S1000/S3000**,
**Roland S-750/S-760/S-770** e **E-mu Emulator III/IIIx/ESI-32/IV** (formato de bank
"EMU3 flat") para **SFZ** + WAV.

Este projeto começa como a camada de filesystem Akai (M0 do plano de arquitetura) e vai
crescer em cima do trabalho já existente no diretório `AKAI/`/`ROLAND/` deste
repositório, em vez de reescrever do zero o que já está resolvido. O nome do projeto
ficou `akai2sfz` por continuidade mesmo depois do suporte a Roland entrar (decisão
consciente, não descuido).

## Proveniência — o que vem de onde

O layout binário do filesystem Akai (partição / volume / diretório / FAT / entradas de
CD-ROM) implementado em `include/akai2sfz/raw_format.hpp` é uma reexpressão em C++,
com nomes e offsets conferidos byte a byte, do trabalho de reverse engineering de
duas fontes independentes que concordam entre si:

- **akaiutil** (C), de Klaus Michael Indlekofer — `../akaiutil`, `../akai-fs` e
  `../a_file_explorer/akaiutil`. A implementação mais completa e madura de todo o
  conjunto: cobre S900/S1000/S3000, disquete, HD e CD-ROM. É a referência principal
  para esta camada.
- **akaitools** (Perl), de Hiroyuki Ohsaki, 1997 — `../akaitools-1.5.tar.gz` e
  `../AKAITOOLS/akaitools-1.5/Synth/AkaiDisk.pm` / `AkaiSample.pm`. Os offsets do
  header de sample/program S3000 (`.a3p`/`.a3s`) usados aqui foram validados contra
  este código.
- **"Akai sampler disk and file formats"**, Paul Kellett, 1995-2000
  (mda.smartelectronix.com/akai/akaiinfo.htm; cópia local em
  `../588909048-akai-disk-file-formats-pdf.pdf`). Terceira fonte independente que
  concorda com as duas acima nos offsets de sample S3000, e resolveu dois pontos que
  as outras duas não deixavam claros: `loop_start`/`loop_len` são contados em
  **words**, não bytes (o protótipo Python original dividia por 2 partindo do
  pressuposto errado), e existe um byte de `loop_mode` explícito em `0x13` mais
  confiável do que inferir o modo a partir do campo de tempo em `0x30`. É também a
  **única fonte com o layout completo do S1000** (`.a1p`/`.a1s`, seções 5 e 6) — vai
  ser o ponto de partida do M3, em vez de reverse engineering do zero.

Outras ferramentas do diretório que informaram este projeto:

- **a_file_explorer** (C + Wt), de δέλτα άλφα — GUI web sobre o mesmo akaiutil;
  referência de UX para navegação de disco.
- **akai-sampler-extractor** (Python) — wrapper fino em torno de `akaiutil.exe`;
  confirma que akaiutil também é usado "as-is" por terceiros como front-end de
  extração.
- **AKAITOOLS/akaitools-1.5** (Python + Perl) — o protótipo que primeiro chegou até
  SFZ. `akai2sfz` é a reescrita nativa desse protótipo: a lógica de geração de SFZ e
  o parser S3000 validado servem de referência funcional; a dependência de runtime
  Perl e o parser S1000 (ainda placeholder nesse protótipo) são o que este projeto
  soluciona.

### Roland S-750/S-760/S-770

O layout binário Roland (`include/akai2sfz/roland_raw_format.hpp`) vem de uma fonte
única, mas de altíssima qualidade -- **não é engenharia reversa de terceiros**:

- **"SYS-772 Ver.2.0 for S-750/770 Hard Disk/MO Disk/CD-ROM Format Manual"**,
  revisão 1.00, 7 de maio de 1991, R&D J-1, Roland Electronics Corp. -- documento de
  engenharia do próprio fabricante. Cópia local em
  `../../ROLAND/Roland-S750-CDROM-Format/Roland-CDROM-S750.pdf`.

Vários offsets do manual precisaram de correção depois de validados contra CDs reais
(`../../ROLAND/*.iso`, 11 imagens disponíveis) -- documentado em detalhe nos
comentários de `roland_raw_format.hpp` e no changelog do git, mas os dois achados mais
importantes:

- A relação Patch → Partial (`Partial List`, 88 entradas) é indexação **0-based
  direta** por tecla, não uma indireção via "Partial Sel" como o desenho do formato
  sugeria por analogia com outras listas (Volume→Performance, Performance→Patch). O
  valor "não usado" é `0xFFFF`, não `0`.
- O byte "Original Key" do parâmetro de Sample está no offset `0x2A`, não `0x2B` como
  a leitura literal da ordem do manual sugere -- confirmado porque o valor lido em
  `0x2A` batia exatamente com a nota do nome da própria amostra (`GTR:E3 Tap Hrm 1` →
  `original_key=52` = MIDI E3) numa imagem real.

### E-mu Emulator III/IIIx/ESI-32/IV

O container de disco (`include/akai2sfz/emu_raw_format.hpp`, superbloco/cadeia de
clusters/diretório de 2 níveis) e o formato de bank "EMU3 flat"
(`EMULATOR THREE`/`EMULATOR 3X`/`EMU SI-32 v3`) vêm de duas fontes de terceiros que
concordam entre si:

- **emu3fs** (C, driver de kernel Linux), de David García Goñi —
  `../E-MU/emu3fs`. Documenta o container de disco comum a EIII/EIIIx/ESI-32/EIV:
  superbloco de 4 bytes (`"EMU3"`) + 9 campos `u32`, cadeia de clusters de 16 bits
  (estilo FAT, índices 1-based), diretório-raiz (pastas) + pool compartilhado de
  blocos de conteúdo (arquivos), dentries de 32 bytes.
- **emu3bm** (C), do mesmo autor — `../E-MU/emu3bm/src/emu3bm.c`. Documenta o
  formato de bank binário com endereçamento absoluto: cabeçalho de 108 bytes, tabelas
  de endereço de preset/sample (com a peculiaridade de que `EMULATOR THREE` usa
  offsets relativos a um espaço de endereço maior — subtraindo uma constante fixa —
  enquanto `EMULATOR 3X`/`EMU SI-32 v3` usam offsets diretos), preset de 142 bytes,
  zona de 48 bytes, sample de 92 bytes de cabeçalho + PCM 16-bit por canal (canal
  esquerdo inteiro seguido do direito inteiro, **não** intercalado como WAV).

Todos os offsets acima foram revalidados campo a campo contra bancos reais extraídos
de `orbit.iso` (Emulator III, "Emu Orbit — The Dance Planet") e de
`Vol. 1 – Emulator Standards.iso`: a cadeia completa bank → preset → note_zone → zone
→ sample bate (nomes ASCII fazem sentido em cada nível, `sample_rate` plausível,
`original_key` progride monotonicamente ao longo de presets multi-sample como um
piano de 88 teclas). Um achado importante, só descoberto ao testar contra um bank
real com múltiplos samples (não um bank vazio, que mascara o problema):

- Os campos `blocks`/`bytes_in_last_block` da entrada de diretório do **filesystem**
  (não do bank) descrevem o tamanho válido só do **último cluster** da cadeia, não do
  arquivo inteiro — a hipótese inicial (mais simples) de que descreviam o arquivo
  inteiro só não quebrava num bank vazio de 1 cluster (caso degenerado onde as duas
  interpretações coincidem); um bank real de vários samples truncava e derrubava a
  leitura do 2º sample em diante até a correção.

Licença: GPLv2, herdada do akaiutil original (ver `LICENSE`).

## Estado atual

### M0 -- filesystem
- [x] Leitura de blocos de uma imagem plana (ISO simples) via `pread`.
- [x] Varredura e validação de partições (magic + checksum de `akai_parthead_s`).
- [x] Enumeração de volumes na partição (root directory, tipos S1000/S3000/CD3000).
- [x] Enumeração de arquivos dentro de um volume (S1000: 1 bloco / 126 entradas;
      S3000: 2 blocos via FAT / 510 entradas).
- [x] Extração de arquivo via cadeia de FAT.

### M2 -- conteúdo S3000 + SFZ (adiantado, antes do M1)
- [x] Parser de sample S3000 (`.a3s`): key, tune, loop (start/len em *words*,
      modo via byte explícito em `0x13` -- ver seção de proveniência acima).
- [x] Parser de program S3000 (`.a3p`): 1 keygroup = 192 bytes, até 4 zonas de
      velocidade por keygroup (offsets `0x22`/`0x3A`/`0x52`/`0x6A`) -- cobre o
      caso comum de par estéreo L/R (2 zonas, mesma faixa de velocidade).
- [x] WAV via libsndfile, SFZ com region por zona.

### M3 -- conteúdo S1000 + SFZ
- [x] Parser de sample S1000 (`.a1s`, header de 150 bytes): mesmos offsets do
      S3000 para tudo que os dois formatos compartilham (`key@0x02`,
      `name@0x03`, loop em `0x26`/`0x2C`/`0x13`) -- só a afinação muda (dois
      bytes signed separados -- cents e semitons -- em vez do fixed-point de
      16 bits do S3000).
- [x] Parser de program S1000 (`.a1p`): mesmo layout de keygroup/zona do
      S3000 (`low_key@0x03`, zonas em `0x22`/`0x3A`/`0x52`/`0x6A`), mais o
      campo explícito "vel zones used" (`0x1F`) em vez de inferir pelo nome
      vazio.
- [x] **"Key tracking" por zona** (`0x84+zona`, doc Kellett): quando `FIXED`,
      a amostra deve soar sempre na mesma altura, então `pitch_keycenter` usa
      a tecla do keygroup em vez da tecla raiz do sample -- sem isso, kits de
      bateria tocavam ~2 oitavas abaixo do esperado (achado ao validar contra
      `RMD2.iso`, um CD S1000 real já presente no diretório: kit de bateria
      `DRYKIT01 A`, 4 zonas de velocidade por batida, todas fixed-pitch).
- [x] `sfz_writer` generalizado (`SfzRegion`) para não depender de S1000 nem
      S3000 -- o `converter.cpp` monta as regions a partir de qualquer um dos
      dois parsers.
- [x] CLI: `list` / `extract` / `convert` (detecta `.a3p` vs `.a1p` sozinho).
- [x] GUI Qt6: browser de 3 colunas (Partições → Volumes → Programs,
      expansível para ver os samples referenciados) -- S1000 e S3000 tratados
      igualmente.

### M1 -- containers reais (BIN+CUE, NRG, MDF)
- [x] `SectorLayout` no `BlockDevice`: mapeia bloco Akai (8192 bytes) para
      bytes físicos considerando tamanho de setor físico, offset dos dados
      úteis dentro do setor e um deslocamento de base -- o caso "plano" (ISO
      simples) é o mesmo comportamento do M0, sem overhead extra.
- [x] **BIN+CUE**: lê a cue sheet de verdade (`FILE`/`TRACK MODEx/yyyy`/
      `INDEX 01`). Validado contra `TZIFFXAK.bin`+`.cue` reais: setor bruto
      `MODE1/2352` (12 bytes de sync `00 FF×10 00` a cada 2352 bytes, dados
      úteis nos 2048 bytes a partir do offset 16) -- confirmado porque o
      campo de tamanho da partição Akai aparecia exatamente ali. Resolve
      `.bin` sem `.cue` ao lado tolerando diferença de maiúscula/minúscula
      no nome referenciado (comum em imagens rippadas no Windows).
- [x] **NRG (Nero)**: lê o footer v2 (assinatura `NER5` de 4 bytes seguida
      de um offset big-endian de 8 bytes, terminando exatamente no fim do
      arquivo -- não o contrário, como a maioria da documentação informal
      sugere) e caminha pelos chunks até achar `CUEX`, que dá o pregap e o
      LBA 0 da track 1. Validado contra um NRG real (Mellotron): setor
      "cooked" de 2048 bytes, pregap de 150 frames *incluído* no arquivo
      (dados começam em `150 × 2048 = 307200`, não no byte 0). v1 (`NERO`)
      implementado por analogia, sem amostra para validar.
- [x] **Auto-detecção como rede de segurança**: sempre que há ambiguidade
      (setor cooked vs. raw, ou o container `.nrg`/`.mdf` não dá certeza
      total), `open_first_valid()` monta os candidatos plausíveis e usa
      `scan_partitions()` -- que já valida magic+checksum do
      `akai_parthead_s` -- como oráculo: o primeiro candidato que acha uma
      partição Akai real é aceito. Isso evita ter que decifrar 100% de cada
      formato proprietário.
- [x] **MDF**: sem amostra real disponível neste projeto -- usa a mesma
      auto-detecção (padrão de sync de CD presente/ausente) que o fallback
      de `.bin` sem `.cue`. Não validado; ver riscos conhecidos abaixo.

### M5 -- empacotamento (macOS)
- [x] GUI como bundle `.app` de verdade (`MACOSX_BUNDLE`, `Info.plist`
      próprio, versionado a partir de `project(... VERSION ...)`).
- [x] Alvo `package_gui`: `macdeployqt` (bundla Qt e as dependências não-Qt
      que ele arrasta junto, incluindo `libsndfile`) → reassinatura ad-hoc
      (`codesign --force --deep --sign -`, necessária no Apple Silicon
      porque o `macdeployqt` invalida a assinatura ao copiar libs para
      dentro do bundle) → `.dmg` via `hdiutil`. Testado montando o `.dmg` e
      abrindo o `.app` de dentro dele, como um usuário real faria.
- [x] CLI com `install()` (`cmake --install build`), mas continua
      dependendo de `libsndfile` via Homebrew quando rodada fora do bundle
      da GUI -- não há um pacote standalone da CLI ainda (ver riscos).

### RM0-RM2 -- suporte Roland S-750/S-760/S-770
- [x] Filesystem Roland (`roland_filesystem.hpp/cpp`): ID area (detecção pela
      assinatura `'S770 MR25A'`, mesma para os três modelos), FAT (16-bit, mesma
      semântica geral do Akai), 5 diretórios planos (Volume/Performance/Patch/
      Partial/Sample -- indexação direta, sem cadeia de FAT como no Akai). Ao
      contrário do Akai, uma imagem Roland tem um único conjunto (sem múltiplas
      partições). Validado contra 2 CDs bem diferentes (bateria e guitarras): as
      contagens do ID area batem exatamente com o número de entradas ativas
      encontradas, e os `fat_entry`/`capacity` de samples consecutivas são
      sequenciais (evidência de que os offsets do diretório estão corretos).
- [x] Parser de conteúdo (`roland_format.hpp/cpp`): Patch (mapa de 88 teclas →
      Partial), Partial (até 4 samples/velocity layers com crossfade real via
      `vel_lower`/`vel_upper`/fade width), Sample (pontos de loop, `original_key`).
      Cadeia completa `Patch → Partial → Sample` validada ponta a ponta contra
      dados reais (nomes se repetem/fazem sentido em cada nível, ex.:
      `KIK:Gretsch Kik5` → partial `KIK:Gretsch kik5` → sample `KIK:Gretsch kik5`).
- [x] Conversão (`roland_converter.cpp`): reaproveita `SfzRegion`/`write_sfz`/
      `write_wav_mono16` -- as MESMAS funções usadas pelo lado Akai, sem
      modificação. `pitch_keycenter` usa a própria tecla quando o Partial é
      mapeado numa única tecla (comum em percussão, onde `original_key` pode não
      ser confiável) e o `original_key` real quando a faixa é mais larga (comum
      em instrumentos melódicos) -- ver riscos abaixo.
- [x] CLI: `list`/`convert` detectam Roland automaticamente (tentativa de leitura
      da assinatura antes de cair para Akai). `<ALVO>` para Roland é só o nome do
      Patch (sem prefixo de volume, já que a busca é global -- ver "volume
      scoping" nos riscos). `extract` ainda é só Akai.
- [x] GUI: mesmas 3 colunas, significado adaptado -- Partições vira um único
      pseudo-item "Disco Roland"; Volumes lista os volumes reais (ou um único
      pseudo-item se não houver); Programs lista os Patches diretamente,
      expansível para ver tecla → Partial → Sample.

### EM0-EM2 -- suporte E-mu Emulator III/IIIx/ESI-32/IV
- [x] Filesystem E-mu (`emu_filesystem.hpp/cpp`): superbloco (`EMU3`, offsets
      `0x00`-`0x28`), cadeia de clusters de 16 bits, diretório-raiz (pastas) +
      pool de blocos de conteúdo (arquivos). Ao contrário do Roland, uma
      imagem E-mu PODE ter várias pastas de topo -- viram os "Volumes" reais
      da GUI. Validado contra 2 CDs reais (`orbit.iso` e
      `Vol. 1 – Emulator Standards.iso`): superbloco, listagem de pastas e
      extração de arquivo conferidos byte a byte via uma ferramenta de sonda
      (`tests/emu_probe.cpp`, não faz parte do ctest).
- [x] Parser de conteúdo "EMU3 flat" (`emu_format.hpp/cpp`): bank → preset →
      note_zone → zone → sample, com as três variantes de endereçamento
      (`EMULATOR THREE`/`EMULATOR 3X`/`EMU SI-32 v3`) espelhando
      `emu3bm.c` linha por linha. Cadeia completa validada contra bancos reais
      de dois discos bem diferentes: um synth (`Orbit bas 1`, 20 presets/20
      samples estéreo) e um piano multi-sample real (`9ft Grand 88 Raw`, 46
      zonas cobrindo as 88 teclas, `original_key` progredindo corretamente).
- [x] Conversão (`emu_converter.cpp`): reaproveita `SfzRegion`/`write_sfz`/
      `write_wav_mono16` sem modificação -- mesma generalização que já valeu a
      pena para Roland. Amostras estéreo (comuns em bancos E-mu, ao contrário
      de Akai/Roland) viram 2 arquivos WAV mono (`_L`/`_R`) com pan hard
      -100/+100, já que o formato guarda os dois canais como blocos contíguos
      dentro do mesmo objeto sample (não intercalados).
- [x] CLI: `list`/`convert` detectam E-mu automaticamente (mesmo bloco de 512
      bytes do Roland, assinatura diferente). `<ALVO>` para E-mu é
      `PASTA/BANK/PRESET` (3 níveis, já que um bank agrupa vários presets).
      `extract` ainda é só Akai.
- [x] GUI: mesmas 3 colunas, mas com um nível a mais na coluna Programs --
      Partições vira um único pseudo-item "Disco E-mu"; Volumes lista as
      pastas reais (esse conceito existe de verdade aqui, ao contrário do
      Roland); Programs lista os Banks (carregados sem parsing, só a
      listagem do filesystem), expansíveis sob demanda para mostrar os
      Presets -- e é o Preset (filho), não o Bank, que é convertível.

### Riscos conhecidos / não totalmente validados
- A codificação de afinação do S1000 (cents/semitons como bytes separados,
  `decode_s1000_tune` em `akai_format.cpp`) segue a descrição literal do doc
  Kellett, mas não há uma segunda fonte para confirmar se há reescala
  envolvida (o doc é inconsistente entre seções sobre se o byte de "cents"
  vai de -50..50 diretamente ou de -128..127 mapeado nesse intervalo).
- O campo `pitch` do keygroup S3000 (offset `0x84`, usado para `transpose`)
  vem do protótipo Python original e nunca foi confirmado contra uma fonte
  independente -- nos testes reais ele aparece constante (`transpose=-59`
  em todos os keygroups de um mesmo program), o que sugere que pode não ser
  o que o nome indica. S1000 não tem um campo equivalente documentado, por
  isso `transpose` fica sempre 0 nesse formato.
- **MDF** e **NRG v1** não têm amostra real para validar neste projeto --
  dependem inteiramente da auto-detecção por padrão de sync de CD (para
  MDF) ou de uma leitura do footer feita por analogia com o v2 (para NRG
  v1). `open_first_valid()` sempre confirma via `scan_partitions()` antes
  de aceitar um layout, então o pior caso é "não encontrei partição válida"
  em vez de dados corrompidos silenciosos -- mas um MDF ou NRG v1 real pode
  expor um layout que os candidatos atuais não cobrem.
- O `.dmg` gerado pelo M5 é assinado **ad-hoc** (`codesign --sign -`), não
  com um Developer ID da Apple -- suficiente para uso pessoal ou
  compartilhar entre máquinas confiáveis, mas o Gatekeeper vai reclamar
  ("app de desenvolvedor não identificado") em instalações completamente
  novas até o usuário liberar manualmente. Não há notarização.
- **Roland -- "volume scoping" não implementado**: a hierarquia real é
  `Volume → Performance → Patch` (cada Volume tem uma lista de até 64
  Performances, cada Performance uma lista de até 32 Patches), mas o
  `convert_roland_patch()`/a GUI não percorrem essa cadeia -- listam e buscam
  Patches globalmente no disco inteiro, ignorando a qual Volume pertencem.
  Funciona bem nos CDs de teste (1-2 volumes), fica menos preciso em discos
  com vários volumes distintos e patches de mesmo nome em volumes diferentes.
- **Roland -- pan/tune sem calibração**: o mapeamento de `pan` (-64..+63 do
  Roland para -100..100 do SFZ) e `tune` (coarse+fine) usa uma escala linear
  razoável mas não confirmada contra áudio real -- diferente do
  `pitch_keycenter`/loop points, que foram validados byte a byte contra
  amostras reais, esses dois só foram conferidos estruturalmente (o código
  roda e produz valores no intervalo esperado, não necessariamente calibrados).
- **Roland -- taxa de amostragem fixa em 44100 Hz**: o manual não documenta o
  mapeamento de bits do campo "Sample Frequency/Mode" pro S-750/770
  especificamente (só formatos anteriores, com tabela provavelmente
  diferente). Se um CD real usar uma taxa diferente de 44.1kHz, o WAV gerado
  vai tocar na velocidade/altura erradas -- nenhuma das amostras testadas
  neste projeto expôs esse problema de forma óbvia, mas não foi confirmado.
- **Roland -- `loop_mode=Alternate` (ping-pong) e `Reverse`** são mapeados
  para `loop_continuous` do SFZ por falta de equivalente nativo -- perde a
  direção alternada/reversa do loop original.
- **E-mu -- camadas pri/sec tratadas como sobrepostas, não crossfade por
  velocidade**: cada preset tem 2 "camadas" (pri/sec) com faixas de
  velocidade próprias (`vel_pri_low/high`, `vel_sec_low/high`) para permitir
  crossfade, mas em nenhum preset com uma camada secundária real testado (em
  2 discos, incluindo pianos multi-camada) esses campos mostraram uma divisão
  coerente (tipicamente `[0,0]`/`[0,255]`, ou valores irregulares nos poucos
  casos com `sec_zone` de fato ativo) -- diferente do `pitch_keycenter`/loop
  points, validados byte a byte. Até confirmar contra áudio real, as duas
  camadas são emitidas com faixa de velocidade completa (0-127, sobrepostas)
  em vez de crossfade -- mais seguro que arriscar silenciar uma camada com um
  corte de velocidade errado, mas não é o comportamento original do hardware.
- **E-mu -- volume e filtro não mapeados**: `vca_level`, `vcf_cutoff` e os
  envelopes (`vca_envelope`/`vcf_envelope`/`aux_envelope`) são parseados mas
  não convertidos para SFZ ainda -- só `pitch_keycenter`, faixa de
  tecla/velocidade, `tune` (fórmula exata `v*1.5625` cents, do próprio
  `emu3bm.c`), `pan` (escala linear -64..63 → -100..100, não calibrada contra
  áudio) e loop (ligado/desligado + pontos, sempre como `loop_continuous` --
  o formato não expôs uma distinção sustain/continuous nos bits que foram
  decodificados) chegam no `.sfz` gerado.
- **E-mu -- EMAX/EMAX II fora de escopo**: nenhum dos dois formatos (disco ou
  bank) é coberto por `emu3fs`/`emu3bm` nem por este projeto -- precisaria de
  reverse engineering a partir do zero contra uma imagem de CD real do EMAX,
  que não está disponível no repositório de amostras usado até agora.
- **E-mu -- Emulator IV em formato encadeado (E4B0/EOS) e Emulator X
  (E5B0/EBL) não implementados**: este projeto cobre só o formato de bank
  "flat" clássico (que EIII/ESI-32 usam sempre, e que EIV também sabe ler).
  Bancos EIV/EOS mais recentes e Emulator X usam containers encadeados
  completamente diferentes (chunks `FORM`/`E4B0`/`E4P1`/`E3S1` e
  `FORM`/`E5B0`/`TOC2`/`E5P1`/`E5S1` respectivamente) -- fora de escopo desta
  fase; ficaria para um marco futuro dedicado (ver planejamento da sessão que
  adicionou o suporte a E-mu).

Plano completo de arquitetura e auditoria dos 6 repositórios Akai: ver o artefato da
sessão que criou este projeto (2026-07-22). Plano de integração Roland: ver o artefato
da sessão que adicionou esse suporte (2026-07-23).

## Build

Dependências (macOS via Homebrew): `cmake`, `pkgconf`, `libsndfile`, `qt` (Qt6).

```sh
cmake -B build -S . -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build
```

Se o Qt6 não for encontrado, o build da GUI é pulado automaticamente e só a
CLI é gerada (`-DAKAI2SFZ_BUILD_GUI=OFF` força isso).

### Empacotar a GUI num `.app` + `.dmg` autônomos

```sh
cmake --build build --target package_gui
```

Gera `build/apps/gui/akai2sfz_gui.app` (Qt e `libsndfile` já embutidos,
assinado ad-hoc) e `build/apps/gui/akai2sfz_gui.dmg` ao lado.

## Uso

### CLI

Aceita `.iso`, `.cue`, `.bin` (com ou sem `.cue` ao lado), `.nrg` e `.mdf`. O
fabricante (Akai, Roland ou E-mu) é detectado automaticamente pela imagem.

```sh
# Akai -- <ALVO> = VOLUME/PROGRAM
./build/akai2sfz list caminho/para/imagem.iso
./build/akai2sfz extract caminho/para/imagem.cue "/VOLUME/ARQUIVO" ./saida
./build/akai2sfz convert caminho/para/imagem.nrg "/VOLUME/PROGRAM" ./saida

# Roland -- <ALVO> = nome do Patch (sem prefixo de volume, busca e global)
./build/akai2sfz list caminho/para/imagem_roland.iso
./build/akai2sfz convert caminho/para/imagem_roland.iso "KIK:Gretsch Kik5" ./saida

# E-mu -- <ALVO> = PASTA/BANK/PRESET
./build/akai2sfz list caminho/para/imagem_emu.iso
./build/akai2sfz convert caminho/para/imagem_emu.iso "Default Folder/Orbit bas 1/001 - Membrace" ./saida
```

`extract` e a opção `-p` (partição) ainda são só para Akai.

### GUI

```sh
./build/apps/gui/akai2sfz_gui.app/Contents/MacOS/akai2sfz_gui   # direto do build
# ou, depois de `cmake --build build --target package_gui`:
open build/apps/gui/akai2sfz_gui.app
```

Abra uma imagem (Akai, Roland ou E-mu, qualquer formato de container
suportado), navegue Partições → Volumes → Programs. Para Akai/Roland, clique
num program/patch para expandir e ver os samples referenciados, e converta o
próprio item. Para E-mu, expanda um Bank para ver os Presets dentro dele --
é o Preset que se seleciona e converte, não o Bank. Escolha o diretório de
saída e converta.
