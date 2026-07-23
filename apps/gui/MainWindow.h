#pragma once
// Janela principal da GUI akai2sfz -- primeiro corte, sem threading (a
// conversao roda na thread da UI; para imagens muito grandes isso pode valer
// a pena mover para QThread mais adiante, ver README).
//
// Navegacao em 3 colunas (estilo column browser): Particoes -> Volumes ->
// Programs. Cada program na 3a coluna e expansivel: ao abrir, mostra (sob
// demanda) os samples que ele referencia. S1000 (.a1p) e S3000 (.a3p) sao
// convertiveis e expansiveis igualmente.
//
// Roland (S-750/760/770) usa as MESMAS 3 colunas com significado adaptado:
// Particoes vira um unico pseudo-item "Disco Roland"; Volumes lista os
// volumes reais do disco (ou um pseudo-item unico se nao houver volume-
// scoping implementado -- ver README, e Programs lista os Patches
// diretamente (sem filtrar por volume ainda). O fabricante e detectado
// automaticamente ao carregar a imagem.
//
// E-mu (EIII/ESI-32/EIV, bank EMU3 flat) tambem usa as MESMAS 3 colunas:
// Particoes vira um unico pseudo-item "Disco E-mu"; Volumes lista as pastas
// reais do disco (esse conceito existe de verdade no E-mu, ao contrario do
// Roland); Programs lista os BANKS da pasta selecionada -- cada bank e
// expansivel pra mostrar seus Presets (carregados sob demanda). Diferente
// de Akai/Roland, quem e convertivel em E-mu e o Preset (filho), nao o
// Bank (item de topo), porque um bank agrupa varios presets.
//
// Kurzweil (K2000/K2500/K2600, formato .krz sobre FAT16) segue o mesmo
// desenho de E-mu (arquivo .KRZ ~ Bank, Program ~ Preset, convertivel e o
// Program/filho) mas com Volumes tratado como o Roland: o FAT16 pode ter
// subdiretorios de verdade, mas navegacao por pasta ainda nao foi
// implementada na GUI (so no CLI, via busca recursiva) -- Volumes vira um
// unico pseudo-item e Programs lista TODOS os .KRZ encontrados na arvore
// inteira, nao so na raiz.
//
// Icones (icons.qrc, embutidos no binario via Qt Resource System): um icone
// de CD generico aparece antes do texto em toda linha das colunas 1
// (Particoes) e 2 (Volumes) -- as duas representam conceitos de midia/
// estrutura fisica, nao conteudo. Na coluna 3 (Programs), cada "patch"
// (Program Akai/Patch Roland/Preset E-mu/Program Kurzweil -- a unidade
// convertivel) leva o icone do fabricante correspondente. Tamanho dos
// icones e proporcional ao tamanho da fonte de cada QListWidget/QTreeWidget
// (ver setIconSize() no construtor). Fontes das imagens originais:
// `../../PICS` (logos de terceiros, uso interno/nao redistribuido).
//
// Conversao em lote: programTree_ usa ExtendedSelection, entao o usuario
// pode selecionar varios itens convertiveis de uma vez (Cmd/Shift-click).
// onConvertSelected() converte cada um pra sua PROPRIA subpasta (nomeada
// com o nome do program/patch/preset, sanitizado) dentro do diretorio de
// saida escolhido -- assim SFZs/WAVs de presets diferentes nunca colidem,
// mesmo quando varios sao convertidos de uma vez. Os metodos
// convertSelectedX() so resolvem o alvo e chamam o convert_* apropriado,
// sem mostrar dialogo -- quem agrega os resultados e mostra o resumo final
// (1 dialogo so, mesmo em lote) e o onConvertSelected().

#include "akai2sfz/converter.hpp"
#include "akai2sfz/emu_filesystem.hpp"
#include "akai2sfz/filesystem.hpp"
#include "akai2sfz/image.hpp"
#include "akai2sfz/kurzweil_filesystem.hpp"
#include "akai2sfz/roland_filesystem.hpp"

#include <QIcon>
#include <QMainWindow>

#include <memory>
#include <vector>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QLabel;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);

private slots:
  void onBrowseImage();
  void onLoadImage();
  void onPartitionSelectionChanged();
  void onVolumeSelectionChanged();
  void onProgramSelectionChanged();
  void onProgramItemExpanded(QTreeWidgetItem *item);
  void onConvertSelected();
  void onBrowseOutputDir();

private:
  void log(const QString &line);
  void rebuildPartitionList();
  void rebuildVolumeList();
  void rebuildProgramTree();
  void loadProgramSamples(QTreeWidgetItem *programItem);

  // true se `item` for uma unidade convertivel (Program Akai/Patch Roland/
  // Preset E-mu/Program Kurzweil) dado o fabricante atual -- usado tanto
  // pra habilitar o botao quanto pra filtrar a selecao no lote.
  bool isConvertibleItem(QTreeWidgetItem *item) const;
  // Nome "humano" do item (pra nome da subpasta de saida e pro log).
  QString itemConvertName(QTreeWidgetItem *item) const;
  // Despacha pro convertSelectedX apropriado.
  akai2sfz::ConvertResult convertItem(QTreeWidgetItem *item, const QString &outDir);
  akai2sfz::ConvertResult convertSelectedAkai(QTreeWidgetItem *programItem, const QString &outDir);

  // Ramos especificos de fabricante chamados pelos metodos acima quando
  // isRoland_ e verdadeiro.
  void rebuildProgramTreeRoland();
  void loadPatchPartialsRoland(QTreeWidgetItem *patchItem);
  akai2sfz::ConvertResult convertSelectedRoland(QTreeWidgetItem *patchItem, const QString &outDir);

  // Idem para E-mu (isEmu_ == true).
  void rebuildProgramTreeEmu();
  void loadBankPresetsEmu(QTreeWidgetItem *bankItem);
  akai2sfz::ConvertResult convertSelectedEmu(QTreeWidgetItem *presetItem, const QString &outDir);

  // Idem para Kurzweil (isKurzweil_ == true).
  void rebuildProgramTreeKurzweil();
  void loadProgramsKurzweil(QTreeWidgetItem *krzFileItem);
  akai2sfz::ConvertResult convertSelectedKurzweil(QTreeWidgetItem *programItem, const QString &outDir);

  QLineEdit *imagePathEdit_ = nullptr;
  QPushButton *browseBtn_ = nullptr;
  QPushButton *loadBtn_ = nullptr;

  QListWidget *partitionList_ = nullptr;
  QListWidget *volumeList_ = nullptr;
  QTreeWidget *programTree_ = nullptr;

  QLineEdit *outputDirEdit_ = nullptr;
  QPushButton *browseOutputBtn_ = nullptr;
  QPushButton *convertBtn_ = nullptr;
  QPlainTextEdit *logView_ = nullptr;
  QLabel *statusLabel_ = nullptr;

  bool isRoland_ = false;
  bool isEmu_ = false;
  bool isKurzweil_ = false;

  // Akai
  std::unique_ptr<akai2sfz::BlockDevice> device_;
  std::unique_ptr<akai2sfz::OpenPartition> partition_;
  std::vector<akai2sfz::Partition> partitions_;
  std::size_t currentVolumeIndex_ = 0;

  // Roland
  std::unique_ptr<akai2sfz::BlockDevice> rolandDevice_;
  std::unique_ptr<akai2sfz::RolandDisk> rolandDisk_;

  // E-mu
  std::unique_ptr<akai2sfz::BlockDevice> emuDevice_;
  std::unique_ptr<akai2sfz::EmuDisk> emuDisk_;
  std::string currentFolderName_;

  // Kurzweil
  std::unique_ptr<akai2sfz::BlockDevice> kurzweilDevice_;
  std::unique_ptr<akai2sfz::KurzweilDisk> kurzweilDisk_;

  // Icones (carregados uma vez no construtor a partir de icons.qrc).
  QIcon cdIcon_;
  QIcon akaiIcon_;
  QIcon rolandIcon_;
  QIcon emuIcon_;
  QIcon kurzweilIcon_;
};
