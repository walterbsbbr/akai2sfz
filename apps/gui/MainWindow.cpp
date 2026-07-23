#include "MainWindow.h"
#include "akai2sfz/akai_format.hpp"
#include "akai2sfz/converter.hpp"
#include "akai2sfz/roland_converter.hpp"
#include "akai2sfz/roland_format.hpp"

#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <set>

using namespace akai2sfz;

namespace {

QString volTypeLabel(raw::VolType t) {
  switch (t) {
    case raw::VolType::S1000: return "S1000";
    case raw::VolType::S3000: return "S3000";
    case raw::VolType::Cd3000: return "CD3000";
    default: return QString();
  }
}

QString formatSize(std::uint32_t bytes) {
  double v = bytes;
  const char *units[] = {"B", "KB", "MB", "GB"};
  int u = 0;
  while (v >= 1024.0 && u < 3) {
    v /= 1024.0;
    ++u;
  }
  return QString::number(v, 'f', 1) + " " + units[u];
}

bool isProgramType(const std::string &typeName) {
  return typeName.find("PROGRAM") != std::string::npos;
}

const FileEntry *findFile(const std::vector<FileEntry> &files, const std::string &name,
                           const std::string &ext) {
  for (const auto &f : files) {
    if (f.name == name && f.extension == ext) return &f;
  }
  return nullptr;
}

QString midiNoteName(int note) {
  static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  int octave = (note / 12) - 1;
  return QString("%1%2").arg(names[note % 12]).arg(octave);
}

// Papeis customizados nos itens de lista/arvore, para recuperar dados ao converter.
constexpr int kRolePartitionIndex = Qt::UserRole;
constexpr int kRoleVolumeIndex = Qt::UserRole;
constexpr int kRoleFileName = Qt::UserRole; // Akai: nome do arquivo; Roland: nome do patch
constexpr int kRoleExt = Qt::UserRole + 1;
constexpr int kRolePlaceholder = Qt::UserRole + 2;

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("akai2sfz -- leitor de CD Akai/Roland e conversor para SFZ");

  auto *central = new QWidget(this);
  auto *mainLayout = new QVBoxLayout(central);

  // --- topo: imagem ---
  auto *topBox = new QGroupBox("Imagem do CD", central);
  auto *topLayout = new QHBoxLayout(topBox);

  imagePathEdit_ = new QLineEdit(topBox);
  imagePathEdit_->setReadOnly(true);
  imagePathEdit_->setPlaceholderText("Nenhuma imagem carregada...");
  topLayout->addWidget(imagePathEdit_, 1);

  browseBtn_ = new QPushButton("Procurar...", topBox);
  topLayout->addWidget(browseBtn_);

  loadBtn_ = new QPushButton("Carregar", topBox);
  loadBtn_->setEnabled(false);
  topLayout->addWidget(loadBtn_);

  mainLayout->addWidget(topBox);

  // --- meio: 3 colunas -- Particoes | Volumes | Programs (expansivel) ---
  auto *splitter = new QSplitter(Qt::Horizontal, central);

  auto *partBox = new QGroupBox("Particoes", splitter);
  auto *partLayout = new QVBoxLayout(partBox);
  partitionList_ = new QListWidget(partBox);
  partLayout->addWidget(partitionList_);
  splitter->addWidget(partBox);

  auto *volBox = new QGroupBox("Volumes", splitter);
  auto *volLayout = new QVBoxLayout(volBox);
  volumeList_ = new QListWidget(volBox);
  volLayout->addWidget(volumeList_);
  splitter->addWidget(volBox);

  auto *progBox = new QGroupBox("Programs", splitter);
  auto *progLayout = new QVBoxLayout(progBox);
  programTree_ = new QTreeWidget(progBox);
  programTree_->setHeaderHidden(true);
  programTree_->setColumnCount(1);
  progLayout->addWidget(programTree_);
  splitter->addWidget(progBox);

  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 1);
  splitter->setStretchFactor(2, 2);

  mainLayout->addWidget(splitter, 1);

  // --- saida + converter ---
  auto *outBox = new QGroupBox("Conversao", central);
  auto *outLayout = new QHBoxLayout(outBox);
  outLayout->addWidget(new QLabel("Diretorio de saida:", outBox));
  outputDirEdit_ = new QLineEdit(outBox);
  outLayout->addWidget(outputDirEdit_, 1);
  browseOutputBtn_ = new QPushButton("Procurar...", outBox);
  outLayout->addWidget(browseOutputBtn_);
  convertBtn_ = new QPushButton("Converter program selecionado", outBox);
  convertBtn_->setEnabled(false);
  outLayout->addWidget(convertBtn_);
  mainLayout->addWidget(outBox);

  // --- log ---
  logView_ = new QPlainTextEdit(central);
  logView_->setReadOnly(true);
  logView_->setMaximumBlockCount(2000);
  logView_->setFixedHeight(140);
  mainLayout->addWidget(logView_);

  statusLabel_ = new QLabel("Pronto.", central);
  mainLayout->addWidget(statusLabel_);

  setCentralWidget(central);

  connect(browseBtn_, &QPushButton::clicked, this, &MainWindow::onBrowseImage);
  connect(loadBtn_, &QPushButton::clicked, this, &MainWindow::onLoadImage);
  connect(browseOutputBtn_, &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
  connect(convertBtn_, &QPushButton::clicked, this, &MainWindow::onConvertSelected);
  connect(partitionList_, &QListWidget::currentRowChanged, this,
          &MainWindow::onPartitionSelectionChanged);
  connect(volumeList_, &QListWidget::currentRowChanged, this,
          &MainWindow::onVolumeSelectionChanged);
  connect(programTree_, &QTreeWidget::currentItemChanged, this,
          &MainWindow::onProgramCurrentItemChanged);
  connect(programTree_, &QTreeWidget::itemExpanded, this, &MainWindow::onProgramItemExpanded);

  log("akai2sfz iniciado. Abra uma imagem de CD Akai (S1000/S3000) ou Roland (S-750/760/770) "
      "para comecar -- o fabricante e detectado automaticamente.");
}

void MainWindow::log(const QString &line) {
  logView_->appendPlainText(line);
}

void MainWindow::onBrowseImage() {
  QString path = QFileDialog::getOpenFileName(
      this, "Selecione a imagem do CD", QString(),
      "Imagens de CD (*.iso *.cue *.bin *.nrg *.mdf);;Todos os arquivos (*)");
  if (path.isEmpty()) return;

  imagePathEdit_->setText(path);
  loadBtn_->setEnabled(true);
}

void MainWindow::onLoadImage() {
  partitionList_->clear();
  volumeList_->clear();
  programTree_->clear();
  partition_.reset();
  device_.reset();
  rolandDisk_.reset();
  rolandDevice_.reset();
  convertBtn_->setEnabled(false);

  std::string path = imagePathEdit_->text().toStdString();

  // Tenta Roland primeiro (deteccao rapida: assinatura no bloco 0 com
  // block_size=512). Se nao bater, tenta Akai.
  try {
    auto rdev = std::make_unique<BlockDevice>(path, roland_raw::kBlockSize);
    if (looks_like_roland(*rdev)) {
      rolandDevice_ = std::move(rdev);
      rolandDisk_ = std::make_unique<RolandDisk>(*rolandDevice_);
      isRoland_ = true;
      log(QString("Disco Roland detectado: '%1'.")
              .arg(QString::fromStdString(rolandDisk_->drive_name())));
      rebuildPartitionList();
      return;
    }
  } catch (const std::exception &) {
    // nao e Roland (ou erro ao abrir) -- cai para Akai abaixo
  }

  isRoland_ = false;
  try {
    device_ = open_cd_image(path);
    partitions_ = scan_partitions(*device_);
  } catch (const std::exception &e) {
    QMessageBox::critical(this, "Erro", QString("Falha ao abrir a imagem:\n%1").arg(e.what()));
    statusLabel_->setText("Erro ao abrir imagem.");
    return;
  }

  if (partitions_.empty()) {
    QMessageBox::warning(this, "Nenhuma particao",
                          "Nenhuma particao Akai valida foi encontrada nesta imagem, e ela nao "
                          "tem a assinatura de um disco Roland.");
    statusLabel_->setText("Nenhuma particao valida encontrada.");
    return;
  }

  log(QString("%1 particao(oes) Akai encontrada(s).").arg(partitions_.size()));
  rebuildPartitionList();
}

void MainWindow::rebuildPartitionList() {
  partitionList_->clear();

  if (isRoland_) {
    // Roland nao tem conceito de multiplas particoes -- um unico pseudo-item.
    auto *item = new QListWidgetItem(
        QString("Disco Roland  (%1 blocos)").arg(rolandDisk_->capacity_blocks()), partitionList_);
    item->setData(kRolePartitionIndex, static_cast<qulonglong>(0));
    partitionList_->setCurrentRow(0); // dispara onPartitionSelectionChanged
    return;
  }

  for (std::size_t i = 0; i < partitions_.size(); ++i) {
    QString letter = QString::fromStdString(partition_label(i));
    QString label = QString("Particao %1  (%2 blocos)").arg(letter).arg(partitions_[i].size_blocks);
    auto *item = new QListWidgetItem(label, partitionList_);
    item->setData(kRolePartitionIndex, static_cast<qulonglong>(i));
  }
  if (partitionList_->count() > 0) {
    partitionList_->setCurrentRow(0); // dispara onPartitionSelectionChanged
  }
}

void MainWindow::onPartitionSelectionChanged() {
  volumeList_->clear();
  programTree_->clear();
  convertBtn_->setEnabled(false);

  if (isRoland_) {
    rebuildVolumeList();
    return;
  }

  partition_.reset();
  auto *item = partitionList_->currentItem();
  if (!item || !device_) return;

  std::size_t pi = item->data(kRolePartitionIndex).toULongLong();
  try {
    partition_ = std::make_unique<OpenPartition>(*device_, partitions_[pi]);
  } catch (const std::exception &e) {
    QMessageBox::critical(this, "Erro", QString("Falha ao abrir particao:\n%1").arg(e.what()));
    return;
  }

  rebuildVolumeList();
}

void MainWindow::rebuildVolumeList() {
  volumeList_->clear();

  if (isRoland_) {
    if (!rolandDisk_) return;
    auto volumes = rolandDisk_->list_active(roland_raw::FileType::Volume);
    if (volumes.empty()) {
      // Sem volume-scoping de Patch->Volume implementado ainda (ver README):
      // um unico pseudo-item mostra todos os patches do disco.
      auto *item = new QListWidgetItem("(todos os patches)", volumeList_);
      item->setData(kRoleVolumeIndex, static_cast<qulonglong>(0));
    } else {
      for (const auto &v : volumes) {
        auto *item = new QListWidgetItem(QString::fromStdString(v.name), volumeList_);
        item->setData(kRoleVolumeIndex, static_cast<qulonglong>(v.index));
      }
    }
    if (volumeList_->count() > 0) volumeList_->setCurrentRow(0);
    return;
  }

  if (!partition_) return;
  for (std::size_t vi = 0; vi < partition_->volume_count(); ++vi) {
    raw::VolType vtype = partition_->volume_type(vi);
    QString typeLabel = volTypeLabel(vtype);
    if (typeLabel.isEmpty()) continue; // inativo ou fora de escopo (S900 etc.)

    QString vname = QString::fromStdString(partition_->volume_name(vi));
    auto *item = new QListWidgetItem(QString("%1  [%2]").arg(vname, typeLabel), volumeList_);
    item->setData(kRoleVolumeIndex, static_cast<qulonglong>(vi));
  }
  if (volumeList_->count() > 0) {
    volumeList_->setCurrentRow(0); // dispara onVolumeSelectionChanged
  } else {
    statusLabel_->setText("Particao sem volumes S1000/S3000/CD3000 ativos.");
  }
}

void MainWindow::onVolumeSelectionChanged() {
  programTree_->clear();
  convertBtn_->setEnabled(false);

  if (isRoland_) {
    rebuildProgramTreeRoland();
    return;
  }

  auto *volItem = volumeList_->currentItem();
  if (!volItem) return;
  currentVolumeIndex_ = volItem->data(kRoleVolumeIndex).toULongLong();

  rebuildProgramTree();
}

void MainWindow::rebuildProgramTreeRoland() {
  programTree_->clear();
  if (!rolandDisk_) return;

  auto patches = rolandDisk_->list_active(roland_raw::FileType::Patch);
  for (const auto &p : patches) {
    QString name = QString::fromStdString(p.name);
    auto *item = new QTreeWidgetItem(programTree_, {name});
    item->setData(0, kRoleFileName, name);
    // filho placeholder so para mostrar a seta de expandir; substituido
    // pelas teclas/samples reais em onProgramItemExpanded.
    auto *placeholder = new QTreeWidgetItem(item, {"carregando..."});
    placeholder->setData(0, kRolePlaceholder, true);
  }

  statusLabel_->setText(
      QString("%1 patch(es) (todos os patches do disco -- volume-scoping ainda nao "
              "implementado, ver README).")
          .arg(patches.size()));
}

void MainWindow::rebuildProgramTree() {
  programTree_->clear();
  if (!partition_) return;

  int total = 0;
  int programs = 0;
  for (const auto &f : list_files(*partition_, currentVolumeIndex_)) {
    ++total;
    std::string typeName = file_type_name(f.type);
    if (!isProgramType(typeName)) continue; // so programs aparecem na arvore
    ++programs;

    QString fname = QString::fromStdString(f.name);
    QString ext = QString::fromStdString(f.extension);
    QString label = QString("%1.%2  [%3, %4]")
                         .arg(fname, ext, QString::fromStdString(typeName), formatSize(f.size));
    auto *item = new QTreeWidgetItem(programTree_, {label});
    item->setData(0, kRoleFileName, fname);
    item->setData(0, kRoleExt, ext);

    if (ext == "a3p" || ext == "a1p") {
      // filho placeholder so para mostrar a seta de expandir; substituido
      // por samples reais em onProgramItemExpanded (carregamento sob demanda).
      auto *placeholder = new QTreeWidgetItem(item, {"carregando..."});
      placeholder->setData(0, kRolePlaceholder, true);
    }
  }

  statusLabel_->setText(
      QString("%1 program(s) neste volume (%2 arquivo(s) no total, incluindo samples).")
          .arg(programs)
          .arg(total));
}

void MainWindow::onProgramItemExpanded(QTreeWidgetItem *item) {
  if (item->childCount() != 1) return;
  QTreeWidgetItem *child = item->child(0);
  if (!child->data(0, kRolePlaceholder).toBool()) return; // ja carregado

  item->removeChild(child);
  delete child;

  if (isRoland_) {
    loadPatchPartialsRoland(item);
  } else {
    loadProgramSamples(item);
  }
}

void MainWindow::loadPatchPartialsRoland(QTreeWidgetItem *patchItem) {
  using namespace roland_raw;
  if (!rolandDisk_) return;

  std::string patchName = patchItem->data(0, kRoleFileName).toString().toStdString();

  try {
    RolandDirEntry patchEntry;
    bool found = false;
    for (std::size_t i = 0; i < kDirPatch.entry_count && !found; ++i) {
      auto e = rolandDisk_->read_dir_entry(FileType::Patch, i);
      if (e.is_active() && e.name == patchName) {
        patchEntry = e;
        found = true;
      }
    }
    if (!found) {
      new QTreeWidgetItem(patchItem, {"(patch nao encontrado)"});
      return;
    }

    RolandPatch patch = parse_roland_patch(rolandDisk_->read_param(FileType::Patch, patchEntry.index));

    std::size_t key = 0;
    int shown = 0;
    while (key < patch.partial_at_key.size()) {
      std::uint16_t pidx = patch.partial_at_key[key];
      std::size_t start = key;
      while (key < patch.partial_at_key.size() && patch.partial_at_key[key] == pidx) ++key;
      std::size_t end = key - 1;
      if (pidx == kPartialListUnused) continue;
      if (pidx >= kDirPartial.entry_count) continue;

      int lokey = static_cast<int>(start) + 21;
      int hikey = static_cast<int>(end) + 21;
      auto partialEntry = rolandDisk_->read_dir_entry(FileType::Partial, pidx);
      auto partial = parse_roland_partial(rolandDisk_->read_param(FileType::Partial, pidx));

      QString rangeLabel = (lokey == hikey)
          ? QString("%1: %2").arg(midiNoteName(lokey), QString::fromStdString(partialEntry.name))
          : QString("%1-%2: %3")
                .arg(midiNoteName(lokey), midiNoteName(hikey),
                     QString::fromStdString(partialEntry.name));
      auto *rangeItem = new QTreeWidgetItem(patchItem, {rangeLabel});

      for (const auto &slot : partial.sample_slots) {
        if (slot.sample_index >= kDirSample.entry_count) continue;
        auto sampleEntry = rolandDisk_->read_dir_entry(FileType::Sample, slot.sample_index);
        QString sLabel = QString("vel %1-%2  %3")
                              .arg(slot.vel_lower)
                              .arg(slot.vel_upper)
                              .arg(QString::fromStdString(sampleEntry.name));
        new QTreeWidgetItem(rangeItem, {sLabel});
      }
      ++shown;
    }

    if (shown == 0) {
      new QTreeWidgetItem(patchItem, {"(sem teclas mapeadas)"});
    }
  } catch (const std::exception &e) {
    new QTreeWidgetItem(patchItem, {QString("(erro ao ler patch: %1)").arg(e.what())});
  }
}

void MainWindow::loadProgramSamples(QTreeWidgetItem *programItem) {
  if (!partition_) return;

  std::string programName = programItem->data(0, kRoleFileName).toString().toStdString();
  QString ext = programItem->data(0, kRoleExt).toString();
  auto files = list_files(*partition_, currentVolumeIndex_);
  const FileEntry *progEntry = findFile(files, programName, ext.toStdString());
  if (!progEntry) {
    new QTreeWidgetItem(programItem, {"(program nao encontrado)"});
    return;
  }

  try {
    std::set<std::string> uniqueSamples;
    std::string sampleExt;

    if (ext == "a3p") {
      auto bytes = extract_file(*partition_, *progEntry);
      S3000Program program = parse_s3000_program(bytes);
      for (const auto &kg : program.keygroups) {
        for (const auto &zone : kg.zones) uniqueSamples.insert(zone.sample_name);
      }
      sampleExt = "a3s";
    } else { // a1p
      auto bytes = extract_file(*partition_, *progEntry);
      S1000Program program = parse_s1000_program(bytes);
      for (const auto &kg : program.keygroups) {
        for (const auto &zone : kg.zones) uniqueSamples.insert(zone.sample_name);
      }
      sampleExt = "a1s";
    }

    if (uniqueSamples.empty()) {
      new QTreeWidgetItem(programItem, {"(sem samples referenciados)"});
      return;
    }

    for (const auto &sname : uniqueSamples) {
      const FileEntry *sampleEntry = findFile(files, sname, sampleExt);
      QString sampleExtQ = QString::fromStdString(sampleExt);
      QString label = sampleEntry
                           ? QString("%1.%2  [%3]")
                                 .arg(QString::fromStdString(sname), sampleExtQ,
                                      formatSize(sampleEntry->size))
                           : QString("%1.%2  [nao encontrado no volume]")
                                 .arg(QString::fromStdString(sname), sampleExtQ);
      new QTreeWidgetItem(programItem, {label});
    }
  } catch (const std::exception &e) {
    new QTreeWidgetItem(programItem, {QString("(erro ao ler program: %1)").arg(e.what())});
  }
}

void MainWindow::onProgramCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *) {
  if (!current || current->parent() != nullptr) {
    // nada selecionado, ou selecionado e um sample/tecla (filho) -- nao convertivel diretamente
    convertBtn_->setEnabled(false);
    return;
  }
  if (isRoland_) {
    convertBtn_->setEnabled(true); // todo item de topo em modo Roland e um Patch
    return;
  }
  QString ext = current->data(0, kRoleExt).toString();
  convertBtn_->setEnabled(ext == "a3p" || ext == "a1p");
}

void MainWindow::onBrowseOutputDir() {
  QString dir = QFileDialog::getExistingDirectory(this, "Diretorio de saida");
  if (!dir.isEmpty()) outputDirEdit_->setText(dir);
}

void MainWindow::convertSelectedRoland(QTreeWidgetItem *patchItem, const QString &outDir) {
  QString patchName = patchItem->data(0, kRoleFileName).toString();

  log(QString("Convertendo patch '%1'...").arg(patchName));
  statusLabel_->setText("Convertendo...");

  ConvertResult result = convert_roland_patch(*rolandDisk_, patchName.toStdString(),
                                               outDir.toStdString());

  for (const auto &w : result.warnings) {
    log("aviso: " + QString::fromStdString(w));
  }

  if (!result.success) {
    log("erro: " + QString::fromStdString(result.error));
    QMessageBox::critical(this, "Conversao falhou", QString::fromStdString(result.error));
    statusLabel_->setText("Conversao falhou.");
    return;
  }

  log(QString("OK: %1 (%2 WAV)")
          .arg(QString::fromStdString(result.sfz_path))
          .arg(result.wav_paths.size()));
  statusLabel_->setText("Conversao concluida.");
  QMessageBox::information(
      this, "Conversao concluida",
      QString("SFZ: %1\nArquivos WAV: %2")
          .arg(QString::fromStdString(result.sfz_path))
          .arg(result.wav_paths.size()));
}

void MainWindow::onConvertSelected() {
  QTreeWidgetItem *progItem = programTree_->currentItem();
  if (!progItem || progItem->parent() != nullptr) return;

  QString outDir = outputDirEdit_->text();
  if (outDir.isEmpty()) {
    QMessageBox::warning(this, "Diretorio de saida",
                          "Escolha um diretorio de saida antes de converter.");
    return;
  }

  if (isRoland_) {
    if (!rolandDisk_) return;
    convertSelectedRoland(progItem, outDir);
    return;
  }

  if (!partition_) return;
  QString volName = QString::fromStdString(partition_->volume_name(currentVolumeIndex_));
  QString fileName = progItem->data(0, kRoleFileName).toString();

  log(QString("Convertendo %1/%2...").arg(volName, fileName));
  statusLabel_->setText("Convertendo...");

  ConvertResult result = convert_program(*partition_, volName.toStdString(),
                                          fileName.toStdString(), outDir.toStdString());

  for (const auto &w : result.warnings) {
    log("aviso: " + QString::fromStdString(w));
  }

  if (!result.success) {
    log("erro: " + QString::fromStdString(result.error));
    QMessageBox::critical(this, "Conversao falhou", QString::fromStdString(result.error));
    statusLabel_->setText("Conversao falhou.");
    return;
  }

  log(QString("OK: %1 (%2 WAV)")
          .arg(QString::fromStdString(result.sfz_path))
          .arg(result.wav_paths.size()));
  statusLabel_->setText("Conversao concluida.");
  QMessageBox::information(
      this, "Conversao concluida",
      QString("SFZ: %1\nArquivos WAV: %2")
          .arg(QString::fromStdString(result.sfz_path))
          .arg(result.wav_paths.size()));
}
