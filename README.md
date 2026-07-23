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

## Estado atual (M0)

- [x] Leitura de blocos de uma imagem plana (ISO simples) via `pread`.
- [x] Varredura e validação de partições (magic + checksum de `akai_parthead_s`).
- [x] Enumeração de volumes na partição (root directory, tipos S1000/S3000/CD3000).
- [x] Enumeração de arquivos dentro de um volume (S1000: 1 bloco / 126 entradas;
      S3000: 2 blocos via FAT / 510 entradas).
- [x] Extração de arquivo via cadeia de FAT.
- [x] CLI: `akai2sfz list <imagem>` e `akai2sfz extract <imagem> <caminho> <saida>`.
- [ ] Containers MDF/NRG/BIN+CUE reais (hoje só imagem plana) — M1.
- [ ] Parser de conteúdo `.a3p`/`.a3s` (S3000) e geração de SFZ — M2.
- [ ] Reverse engineering de `.a1p`/`.a1s` (S1000) — M3.
- [ ] Pareamento estéreo, 4 zonas de velocidade, opcodes de envelope/filtro — M4.
- [ ] Empacotamento e (opcional) GUI — M5.

Plano completo de arquitetura e auditoria dos 6 repositórios: ver o artefato gerado
na sessão que criou este projeto (auditoria de 2026-07-22).

## Build

```sh
cmake -B build -S .
cmake --build build
```

## Uso

```sh
./build/apps/cli/akai2sfz list caminho/para/imagem.iso
./build/apps/cli/akai2sfz extract caminho/para/imagem.iso "/VOLUME/PROGRAMA" ./saida
```
