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

### Empacotar a GUI num `.app` + `.dmg` autônomos

```sh
cmake --build build --target package_gui
```

Gera `build/apps/gui/akai2sfz_gui.app` (Qt e `libsndfile` já embutidos,
assinado ad-hoc) e `build/apps/gui/akai2sfz_gui.dmg` ao lado.

## Uso

### CLI

Aceita `.iso`, `.cue`, `.bin` (com ou sem `.cue` ao lado), `.nrg` e `.mdf`.

```sh
./build/akai2sfz list caminho/para/imagem.iso
./build/akai2sfz extract caminho/para/imagem.cue "/VOLUME/ARQUIVO" ./saida
./build/akai2sfz convert caminho/para/imagem.nrg "/VOLUME/PROGRAM" ./saida
```

### GUI

```sh
./build/apps/gui/akai2sfz_gui.app/Contents/MacOS/akai2sfz_gui   # direto do build
# ou, depois de `cmake --build build --target package_gui`:
open build/apps/gui/akai2sfz_gui.app
```

Abra uma imagem (ISO/CUE/BIN/NRG/MDF), navegue Partições → Volumes →
Programs, clique num program para expandir e ver os samples referenciados,
escolha o diretório de saída e converta.
