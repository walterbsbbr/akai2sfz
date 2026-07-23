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

#include "akai2sfz/filesystem.hpp"
#include "akai2sfz/image.hpp"
#include "akai2sfz/roland_filesystem.hpp"

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
  void onProgramCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
  void onProgramItemExpanded(QTreeWidgetItem *item);
  void onConvertSelected();
  void onBrowseOutputDir();

private:
  void log(const QString &line);
  void rebuildPartitionList();
  void rebuildVolumeList();
  void rebuildProgramTree();
  void loadProgramSamples(QTreeWidgetItem *programItem);

  // Ramos especificos de fabricante chamados pelos metodos acima quando
  // isRoland_ e verdadeiro.
  void rebuildProgramTreeRoland();
  void loadPatchPartialsRoland(QTreeWidgetItem *patchItem);
  void convertSelectedRoland(QTreeWidgetItem *patchItem, const QString &outDir);

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

  // Akai
  std::unique_ptr<akai2sfz::BlockDevice> device_;
  std::unique_ptr<akai2sfz::OpenPartition> partition_;
  std::vector<akai2sfz::Partition> partitions_;
  std::size_t currentVolumeIndex_ = 0;

  // Roland
  std::unique_ptr<akai2sfz::BlockDevice> rolandDevice_;
  std::unique_ptr<akai2sfz::RolandDisk> rolandDisk_;
};
