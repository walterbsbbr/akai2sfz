#pragma once
// Janela principal da GUI akai2sfz -- primeiro corte, sem threading (a
// conversao roda na thread da UI; para imagens muito grandes isso pode valer
// a pena mover para QThread mais adiante, ver README).
//
// Navegacao em 3 colunas (estilo column browser): Particoes -> Volumes ->
// Programs. Cada program na 3a coluna e expansivel: ao abrir, mostra (sob
// demanda) os samples que ele referencia. So programs S3000 tem parser de
// conteudo por enquanto, entao so eles podem ser expandidos/convertidos.

#include "akai2sfz/filesystem.hpp"
#include "akai2sfz/image.hpp"

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

  std::unique_ptr<akai2sfz::BlockDevice> device_;
  std::unique_ptr<akai2sfz::OpenPartition> partition_;
  std::vector<akai2sfz::Partition> partitions_;
  std::size_t currentVolumeIndex_ = 0;
};
