# akai2sfz

Conversor nativo em C++ de imagens de CD/HD dos samplers Akai **S900 / S1000 / S3000**
(partição, volume, FAT, programs e samples) para **SFZ** + WAV.

Este projeto começa como a camada de filesystem (M0 do plano de arquitetura) e vai
crescer em cima do trabalho já existente no diretório `AKAI/` deste repositório, em vez
de reescrever do zero o que já está resolvido.

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
- [x] WAV via libsndfile, SFZ com region por zona (lokey/hikey, pitch_keycenter
      a partir da tecla raiz do *sample*, velocity, tune combinado, pan, loop).
- [x] CLI: `list` / `extract` / `convert`.
- [x] GUI Qt6: `list`/`extract`/`convert` num browser de 3 colunas (Partições →
      Volumes → Programs, expansível para ver os samples referenciados).

### Pendente
- [ ] Containers MDF/NRG/BIN+CUE reais (hoje só imagem plana) — M1.
- [ ] Reverse engineering de `.a1p`/`.a1s` (S1000) — M3. O layout completo já
      está documentado (ver proveniência acima, doc Kellett seções 5-6) --
      falta implementar e validar contra os CDs S1000 reais já disponíveis
      (ex.: `RMD2.iso`).
- [ ] Empacotamento (M5).

Plano completo de arquitetura e auditoria dos 6 repositórios: ver o artefato gerado
na sessão que criou este projeto (auditoria de 2026-07-22).

## Build

Dependências (macOS via Homebrew): `cmake`, `pkgconf`, `libsndfile`, `qt` (Qt6).

```sh
cmake -B build -S . -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build
```

Se o Qt6 não for encontrado, o build da GUI é pulado automaticamente e só a
CLI é gerada (`-DAKAI2SFZ_BUILD_GUI=OFF` força isso).

## Uso

### CLI

```sh
./build/apps/cli/akai2sfz list caminho/para/imagem.iso
./build/apps/cli/akai2sfz extract caminho/para/imagem.iso "/VOLUME/ARQUIVO" ./saida
./build/apps/cli/akai2sfz convert caminho/para/imagem.iso "/VOLUME/PROGRAM" ./saida
```

### GUI

```sh
./build/apps/gui/akai2sfz_gui
```

Abra uma imagem ISO, navegue Partições → Volumes → Programs, clique num
program para expandir e ver os samples referenciados, escolha o diretório de
saída e converta.
