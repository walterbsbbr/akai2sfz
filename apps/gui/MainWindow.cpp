#include "MainWindow.h"
#include "akai2sfz/akai_format.hpp"
#include "akai2sfz/converter.hpp"
#include "akai2sfz/emu_converter.hpp"
#include "akai2sfz/emu_format.hpp"
#include "akai2sfz/krz_converter.hpp"
#include "akai2sfz/krz_format.hpp"
#include "akai2sfz/krz_raw_format.hpp"
#include "akai2sfz/roland_converter.hpp"
#include "akai2sfz/roland_format.hpp"

#include <QAbstractItemView>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
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
constexpr int kRoleFileName = Qt::UserRole; // Akai: nome do arquivo; Roland: nome do patch; E-mu: nome do bank
constexpr int kRoleExt = Qt::UserRole + 1;
constexpr int kRolePlaceholder = Qt::UserRole + 2;
constexpr int kRoleFolderName = Qt::UserRole; // E-mu: nome da pasta (item da coluna Volumes)
constexpr int kRolePresetName = Qt::UserRole + 3; // E-mu: nome do preset (item filho de um bank)

// Tema "rack de hardware vintage" -- ver comentario no .h. Aplicado so na
// MainWindow (nao em nivel de QApplication), entao dialogos nativos
// (QFileDialog/QMessageBox) ficam de fora de proposito -- continuam com a
// aparencia nativa do SO, convencao comum mesmo em apps fortemente
// tematizados. text-transform/letter-spacing nao existem em QSS -- por isso
// os textos de botao/titulo de grupo ja vao em CAIXA ALTA direto no C++, e
// o letter-spacing do subtitulo da marca e feito via QFont::setLetterSpacing.
const char *kStyleSheet = R"(
  QMainWindow, QWidget#central {
    background: #1b1d1a;
  }
  QWidget {
    color: #eae7db;
    font-size: 12px;
  }

  QWidget#nameplate {
    border-bottom: 1px solid #3b3d34;
  }
  QLabel#brandName {
    font-size: 17px;
    font-weight: 800;
    color: #eae7db;
  }
  QLabel#brandSub {
    font-size: 10px;
    font-weight: 700;
    color: #ffb02e;
  }
  QLabel#supportsLabel {
    font-size: 10px;
    font-weight: 700;
    color: #8c8f83;
  }

  QGroupBox {
    background: #24261f;
    border: 1px solid #3b3d34;
    border-radius: 8px;
    margin-top: 12px;
    padding: 12px 8px 8px 8px;
  }
  QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    top: 2px;
    padding: 2px 8px;
    background: #2b2d24;
    border: 1px solid #3b3d34;
    border-radius: 4px;
    color: #8c8f83;
    font-size: 10px;
    font-weight: 700;
  }

  QLineEdit {
    background: #0a0a08;
    border: 1px solid #131410;
    border-radius: 6px;
    padding: 7px 10px;
    color: #ffb02e;
    font-family: Menlo, Consolas, monospace;
    font-size: 12px;
    selection-background-color: #ffb02e;
    selection-color: #0a0a08;
  }
  QLineEdit:disabled { color: #7a5a1e; }

  QPushButton {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #34362c, stop:1 #26271f);
    border: 1px solid #43453a;
    border-radius: 6px;
    padding: 8px 14px;
    color: #eae7db;
    font-weight: 700;
    font-size: 11px;
  }
  QPushButton:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3c3e32, stop:1 #2c2d24);
  }
  QPushButton:pressed { background: #202119; }
  QPushButton:disabled { color: #5f6259; border-color: #2c2e27; background: #232420; }

  QPushButton#loadBtn {
    border-color: #7a5a1e;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a2f16, stop:1 #2c2410);
    color: #ffb02e;
  }
  QPushButton#loadBtn:disabled { color: #6b5527; border-color: #453619; background: #262219; }

  QPushButton#convertBtn {
    border: 1px solid #b9822c;
    border-radius: 7px;
    padding: 10px 18px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffc35c, stop:1 #e69a1f);
    color: #241a04;
    font-size: 11.5px;
    font-weight: 800;
  }
  QPushButton#convertBtn:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffcf78, stop:1 #f0a52a);
  }
  QPushButton#convertBtn:disabled {
    background: #3a3b33;
    border-color: #45473c;
    color: #6c6f64;
  }

  QListWidget, QTreeWidget {
    background: #24261f;
    border: 1px solid #3b3d34;
    border-radius: 8px;
    padding: 4px;
    outline: 0;
  }
  QListWidget::item, QTreeWidget::item {
    padding: 5px 6px;
    border-radius: 5px;
  }
  QListWidget::item:hover, QTreeWidget::item:hover {
    background: rgba(255, 255, 255, 14);
  }
  QListWidget::item:selected, QTreeWidget::item:selected {
    background: rgba(255, 176, 46, 36);
    color: #fbe2b0;
  }
  QTreeWidget::branch {
    background: transparent;
  }

  QLabel#statusLabel {
    font-size: 11px;
    color: #8c8f83;
    padding: 6px 2px 0 2px;
    border-top: 1px solid #3b3d34;
  }

  QTextEdit#logView {
    background: #0a0a08;
    border: 1px solid #131410;
    border-radius: 8px;
    padding: 8px 10px;
    color: #ffb02e;
    font-family: Menlo, Consolas, monospace;
    font-size: 11px;
  }

  QSplitter::handle {
    background: #1b1d1a;
    width: 8px;
  }

  QScrollBar:vertical {
    background: #1b1d1a;
    width: 11px;
    margin: 0;
  }
  QScrollBar::handle:vertical {
    background: #45473c;
    border-radius: 5px;
    min-height: 24px;
  }
  QScrollBar::handle:vertical:hover { background: #55574a; }
  QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
)";

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("WJ-VSC (Vintage Sampler Converter) -- Akai/Roland/E-mu/Kurzweil CD reader "
                 "and SFZ converter");

  // Icones (ver comentario no .h) -- carregados antes de qualquer widget
  // porque o nameplate abaixo ja usa akaiIcon_/rolandIcon_/emuIcon_/
  // kurzweilIcon_ pro selo "Supports".
  cdIcon_ = QIcon(":/icons/cd.png");
  akaiIcon_ = QIcon(":/icons/akai.png");
  rolandIcon_ = QIcon(":/icons/roland.png");
  emuIcon_ = QIcon(":/icons/emu.png");
  kurzweilIcon_ = QIcon(":/icons/kurzweil.png");
  setWindowIcon(cdIcon_);

  setStyleSheet(kStyleSheet);

  auto *central = new QWidget(this);
  central->setObjectName("central");
  auto *mainLayout = new QVBoxLayout(central);
  mainLayout->setContentsMargins(14, 12, 14, 12);
  mainLayout->setSpacing(12);

  // --- nameplate: marca + selo de fabricantes suportados ---
  auto *nameplate = new QWidget(central);
  nameplate->setObjectName("nameplate");
  auto *nameplateLayout = new QHBoxLayout(nameplate);
  nameplateLayout->setContentsMargins(4, 0, 4, 10);

  auto *brandIconLabel = new QLabel(nameplate);
  brandIconLabel->setPixmap(cdIcon_.pixmap(32, 32));
  nameplateLayout->addWidget(brandIconLabel);

  auto *brandTextLayout = new QVBoxLayout();
  brandTextLayout->setSpacing(1);
  auto *brandNameLabel = new QLabel("WJ-VSC", nameplate);
  brandNameLabel->setObjectName("brandName");
  auto *brandSubLabel = new QLabel("Vintage Sampler Converter", nameplate);
  brandSubLabel->setObjectName("brandSub");
  {
    // QSS nao tem letter-spacing -- feito via QFont direto no widget.
    QFont f = brandSubLabel->font();
    f.setLetterSpacing(QFont::PercentageSpacing, 122);
    brandSubLabel->setFont(f);
  }
  brandTextLayout->addWidget(brandNameLabel);
  brandTextLayout->addWidget(brandSubLabel);
  nameplateLayout->addLayout(brandTextLayout);

  nameplateLayout->addStretch(1);

  auto *supportsLabel = new QLabel("SUPPORTS", nameplate);
  supportsLabel->setObjectName("supportsLabel");
  nameplateLayout->addWidget(supportsLabel);
  for (const QIcon &icon : {akaiIcon_, rolandIcon_, emuIcon_, kurzweilIcon_}) {
    auto *logo = new QLabel(nameplate);
    logo->setPixmap(icon.pixmap(18, 18));
    nameplateLayout->addWidget(logo);
  }

  mainLayout->addWidget(nameplate);

  // --- topo: imagem ---
  auto *topBox = new QGroupBox("CD IMAGE", central);
  auto *topLayout = new QHBoxLayout(topBox);

  imagePathEdit_ = new QLineEdit(topBox);
  imagePathEdit_->setReadOnly(true);
  imagePathEdit_->setPlaceholderText("No image loaded...");
  topLayout->addWidget(imagePathEdit_, 1);

  browseBtn_ = new QPushButton("BROWSE...", topBox);
  topLayout->addWidget(browseBtn_);

  loadBtn_ = new QPushButton("LOAD", topBox);
  loadBtn_->setObjectName("loadBtn");
  loadBtn_->setEnabled(false);
  topLayout->addWidget(loadBtn_);

  mainLayout->addWidget(topBox);

  // --- meio: 3 colunas -- Particoes | Volumes | Programs (expansivel) ---
  auto *splitter = new QSplitter(Qt::Horizontal, central);

  auto *partBox = new QGroupBox("PARTITIONS", splitter);
  auto *partLayout = new QVBoxLayout(partBox);
  partitionList_ = new QListWidget(partBox);
  partLayout->addWidget(partitionList_);
  splitter->addWidget(partBox);

  auto *volBox = new QGroupBox("VOLUMES", splitter);
  auto *volLayout = new QVBoxLayout(volBox);
  volumeList_ = new QListWidget(volBox);
  volLayout->addWidget(volumeList_);
  splitter->addWidget(volBox);

  auto *progBox = new QGroupBox("PROGRAMS", splitter);
  auto *progLayout = new QVBoxLayout(progBox);
  programTree_ = new QTreeWidget(progBox);
  programTree_->setHeaderHidden(true);
  programTree_->setColumnCount(1);
  // Selecao multipla (Cmd/Shift-click) -- permite converter varios presets
  // de uma vez, ver onConvertSelected().
  programTree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  progLayout->addWidget(programTree_);
  splitter->addWidget(progBox);

  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 1);
  splitter->setStretchFactor(2, 2);

  // Icones nas listas (ver comentario no .h): tamanho proporcional ao texto
  // -- um pouco maior que a altura da fonte usada nas listas/arvore.
  int iconEdge = static_cast<int>(partitionList_->fontMetrics().height() * 1.4);
  QSize iconSize(iconEdge, iconEdge);
  partitionList_->setIconSize(iconSize);
  volumeList_->setIconSize(iconSize);
  programTree_->setIconSize(iconSize);

  mainLayout->addWidget(splitter, 1);

  // --- saida + converter ---
  auto *outBox = new QGroupBox("CONVERSION", central);
  auto *outLayout = new QHBoxLayout(outBox);
  outLayout->addWidget(new QLabel("OUTPUT DIRECTORY", outBox));
  outputDirEdit_ = new QLineEdit(outBox);
  outLayout->addWidget(outputDirEdit_, 1);
  browseOutputBtn_ = new QPushButton("BROWSE...", outBox);
  outLayout->addWidget(browseOutputBtn_);
  convertBtn_ = new QPushButton("CONVERT SELECTED PROGRAM", outBox);
  convertBtn_->setObjectName("convertBtn");
  convertBtn_->setEnabled(false);
  outLayout->addWidget(convertBtn_);
  mainLayout->addWidget(outBox);

  // --- log ---
  logView_ = new QTextEdit(central);
  logView_->setObjectName("logView");
  logView_->setReadOnly(true);
  logView_->document()->setMaximumBlockCount(2000);
  logView_->setFixedHeight(140);
  mainLayout->addWidget(logView_);

  statusLabel_ = new QLabel(central);
  statusLabel_->setObjectName("statusLabel");
  mainLayout->addWidget(statusLabel_);
  setStatus("Ready.", StatusKind::Ok);

  setCentralWidget(central);

  connect(browseBtn_, &QPushButton::clicked, this, &MainWindow::onBrowseImage);
  connect(loadBtn_, &QPushButton::clicked, this, &MainWindow::onLoadImage);
  connect(browseOutputBtn_, &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
  connect(convertBtn_, &QPushButton::clicked, this, &MainWindow::onConvertSelected);
  connect(partitionList_, &QListWidget::currentRowChanged, this,
          &MainWindow::onPartitionSelectionChanged);
  connect(volumeList_, &QListWidget::currentRowChanged, this,
          &MainWindow::onVolumeSelectionChanged);
  connect(programTree_, &QTreeWidget::itemSelectionChanged, this,
          &MainWindow::onProgramSelectionChanged);
  connect(programTree_, &QTreeWidget::itemExpanded, this, &MainWindow::onProgramItemExpanded);

  log("WJ-VSC started. Open an Akai (S1000/S3000), Roland (S-750/760/770), E-mu "
      "(EIII/ESI-32/EIV) or Kurzweil (K2000/K2500/K2600) CD image to begin -- the "
      "manufacturer is detected automatically.");
}

void MainWindow::log(const QString &line) {
  // Cores semanticas, mesma linguagem visual do painel LCD do mockup:
  // ambar = leitura normal, verde = sucesso, vermelho = erro.
  QString color = "#ffb02e";
  if (line.startsWith("error:")) {
    color = "#ff6b6b";
  } else if (line.startsWith("warning:")) {
    color = "#ffcf7a";
  } else if (line.startsWith("OK:")) {
    color = "#6cff9e";
  }
  logView_->append(
      QString("<span style=\"color:%1;\">%2</span>").arg(color, line.toHtmlEscaped()));
}

void MainWindow::setStatus(const QString &text, StatusKind kind) {
  QString color;
  switch (kind) {
    case StatusKind::Ok: color = "#6cff9e"; break;
    case StatusKind::Warn: color = "#ffb02e"; break;
    case StatusKind::Error: color = "#ff6b6b"; break;
    case StatusKind::Info:
    default: color = "#8c8f83"; break;
  }
  statusLabel_->setText(QString("<span style=\"color:%1;\">&#9679;</span>&nbsp;&nbsp;%2")
                             .arg(color, text.toHtmlEscaped()));
}

void MainWindow::onBrowseImage() {
  QString path = QFileDialog::getOpenFileName(
      this, "Select CD Image", QString(),
      "CD Images (*.iso *.cue *.bin *.nrg *.mdf);;All files (*)");
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
  emuDisk_.reset();
  emuDevice_.reset();
  kurzweilDisk_.reset();
  kurzweilDevice_.reset();
  convertBtn_->setEnabled(false);
  convertBtn_->setText("CONVERT SELECTED PROGRAM");

  std::string path = imagePathEdit_->text().toStdString();

  // Tenta Roland e E-mu primeiro (deteccao rapida: os dois usam
  // block_size=512, cada um com sua propria assinatura no bloco 0 -- um so
  // BlockDevice serve pra testar ambos). Se nenhum bater, tenta Akai.
  try {
    auto rdev = std::make_unique<BlockDevice>(path, roland_raw::kBlockSize);
    if (looks_like_roland(*rdev)) {
      rolandDevice_ = std::move(rdev);
      rolandDisk_ = std::make_unique<RolandDisk>(*rolandDevice_);
      isRoland_ = true;
      isEmu_ = false;
      isKurzweil_ = false;
      log(QString("Roland disk detected: '%1'.")
              .arg(QString::fromStdString(rolandDisk_->drive_name())));
      rebuildPartitionList();
      return;
    }
    if (looks_like_emu(*rdev)) {
      emuDevice_ = std::move(rdev);
      emuDisk_ = std::make_unique<EmuDisk>(*emuDevice_);
      isRoland_ = false;
      isEmu_ = true;
      isKurzweil_ = false;
      log("E-mu disk detected (EIII/ESI-32/EIV, EMU3 container).");
      rebuildPartitionList();
      return;
    }
  } catch (const std::exception &) {
    // nao e Roland nem E-mu (ou erro ao abrir) -- cai para Kurzweil/Akai abaixo
  }

  // Kurzweil usa seu proprio abridor de container (block_size=2048, sem
  // relacao com o BlockDevice de 512 B acima).
  try {
    auto kdev = open_kurzweil_cd_image(path);
    if (looks_like_kurzweil(*kdev)) {
      kurzweilDevice_ = std::move(kdev);
      kurzweilDisk_ = std::make_unique<KurzweilDisk>(*kurzweilDevice_);
      isRoland_ = false;
      isEmu_ = false;
      isKurzweil_ = true;
      log("Kurzweil disk detected (FAT16, .krz format).");
      rebuildPartitionList();
      return;
    }
  } catch (const std::exception &) {
    // nao e Kurzweil (ou erro ao abrir) -- cai para Akai abaixo
  }

  isRoland_ = false;
  isEmu_ = false;
  isKurzweil_ = false;
  try {
    device_ = open_cd_image(path);
    partitions_ = scan_partitions(*device_);
  } catch (const std::exception &e) {
    QMessageBox::critical(this, "Error", QString("Failed to open image:\n%1").arg(e.what()));
    setStatus("Error opening image.", StatusKind::Error);
    return;
  }

  if (partitions_.empty()) {
    QMessageBox::warning(this, "No partition",
                          "No valid Akai partition was found in this image, and it doesn't "
                          "have the signature of a Roland, E-mu, or Kurzweil disk.");
    setStatus("No valid partition found.", StatusKind::Error);
    return;
  }

  log(QString("%1 Akai partition(s) found.").arg(partitions_.size()));
  rebuildPartitionList();
}

void MainWindow::rebuildPartitionList() {
  partitionList_->clear();

  if (isRoland_) {
    // Roland nao tem conceito de multiplas particoes -- um unico pseudo-item.
    auto *item = new QListWidgetItem(
        QString("Roland disk  (%1 blocks)").arg(rolandDisk_->capacity_blocks()), partitionList_);
    item->setIcon(cdIcon_);
    item->setData(kRolePartitionIndex, static_cast<qulonglong>(0));
    partitionList_->setCurrentRow(0); // dispara onPartitionSelectionChanged
    return;
  }

  if (isEmu_) {
    // E-mu tambem nao tem conceito de multiplas particoes -- um unico pseudo-item.
    auto *item = new QListWidgetItem("E-mu disk", partitionList_);
    item->setIcon(cdIcon_);
    item->setData(kRolePartitionIndex, static_cast<qulonglong>(0));
    partitionList_->setCurrentRow(0); // dispara onPartitionSelectionChanged
    return;
  }

  if (isKurzweil_) {
    auto *item = new QListWidgetItem("Kurzweil disk", partitionList_);
    item->setIcon(cdIcon_);
    item->setData(kRolePartitionIndex, static_cast<qulonglong>(0));
    partitionList_->setCurrentRow(0); // dispara onPartitionSelectionChanged
    return;
  }

  for (std::size_t i = 0; i < partitions_.size(); ++i) {
    QString letter = QString::fromStdString(partition_label(i));
    QString label = QString("Partition %1  (%2 blocks)").arg(letter).arg(partitions_[i].size_blocks);
    auto *item = new QListWidgetItem(label, partitionList_);
    item->setIcon(cdIcon_);
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
  convertBtn_->setText("CONVERT SELECTED PROGRAM");

  if (isRoland_ || isEmu_ || isKurzweil_) {
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
    QMessageBox::critical(this, "Error", QString("Failed to open partition:\n%1").arg(e.what()));
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
      auto *item = new QListWidgetItem("(all patches)", volumeList_);
      item->setIcon(cdIcon_);
      item->setData(kRoleVolumeIndex, static_cast<qulonglong>(0));
    } else {
      for (const auto &v : volumes) {
        auto *item = new QListWidgetItem(QString::fromStdString(v.name), volumeList_);
        item->setIcon(cdIcon_);
        item->setData(kRoleVolumeIndex, static_cast<qulonglong>(v.index));
      }
    }
    if (volumeList_->count() > 0) volumeList_->setCurrentRow(0);
    return;
  }

  if (isEmu_) {
    if (!emuDisk_) return;
    for (const auto &folder : emuDisk_->list_folders()) {
      QString name = QString::fromStdString(folder.name);
      auto *item = new QListWidgetItem(name, volumeList_);
      item->setIcon(cdIcon_);
      item->setData(kRoleFolderName, name);
    }
    if (volumeList_->count() > 0) {
      volumeList_->setCurrentRow(0);
    } else {
      setStatus("E-mu disk has no active folders.", StatusKind::Warn);
    }
    return;
  }

  if (isKurzweil_) {
    // Navegacao por pasta ainda nao implementada na GUI (o FAT16 pode ter
    // subdiretorios de verdade, mas a busca de .KRZ e sempre recursiva na
    // arvore inteira) -- um unico pseudo-item, mesmo padrao do Roland.
    auto *item = new QListWidgetItem("(all .krz files)", volumeList_);
    item->setIcon(cdIcon_);
    item->setData(kRoleVolumeIndex, static_cast<qulonglong>(0));
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
    item->setIcon(cdIcon_);
    item->setData(kRoleVolumeIndex, static_cast<qulonglong>(vi));
  }
  if (volumeList_->count() > 0) {
    volumeList_->setCurrentRow(0); // dispara onVolumeSelectionChanged
  } else {
    setStatus("Partition has no active S1000/S3000/CD3000 volumes.", StatusKind::Warn);
  }
}

void MainWindow::onVolumeSelectionChanged() {
  programTree_->clear();
  convertBtn_->setEnabled(false);
  convertBtn_->setText("CONVERT SELECTED PROGRAM");

  if (isRoland_) {
    rebuildProgramTreeRoland();
    return;
  }

  if (isEmu_) {
    auto *item = volumeList_->currentItem();
    if (!item) return;
    currentFolderName_ = item->data(kRoleFolderName).toString().toStdString();
    rebuildProgramTreeEmu();
    return;
  }

  if (isKurzweil_) {
    rebuildProgramTreeKurzweil();
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
    item->setIcon(0, rolandIcon_);
    item->setData(0, kRoleFileName, name);
    // filho placeholder so para mostrar a seta de expandir; substituido
    // pelas teclas/samples reais em onProgramItemExpanded.
    auto *placeholder = new QTreeWidgetItem(item, {"loading..."});
    placeholder->setData(0, kRolePlaceholder, true);
  }

  setStatus(QString("%1 patch(es) (all patches on the disk -- volume scoping not implemented "
                     "yet, see README).")
                .arg(patches.size()),
            StatusKind::Info);
}

void MainWindow::rebuildProgramTreeEmu() {
  programTree_->clear();
  if (!emuDisk_) return;

  for (const auto &folder : emuDisk_->list_folders()) {
    if (folder.name != currentFolderName_) continue;

    auto files = emuDisk_->list_files(folder);
    for (const auto &f : files) {
      QString name = QString::fromStdString(f.name);
      auto *item = new QTreeWidgetItem(programTree_, {name});
      item->setData(0, kRoleFileName, name);
      // filho placeholder so para mostrar a seta de expandir; substituido
      // pelos presets reais em onProgramItemExpanded.
      auto *placeholder = new QTreeWidgetItem(item, {"loading..."});
      placeholder->setData(0, kRolePlaceholder, true);
    }

    setStatus(QString("%1 bank(s) in this folder.").arg(files.size()), StatusKind::Info);
    return;
  }
}

namespace {
void collectKrzFilesRecursive(const akai2sfz::KurzweilDisk &disk,
                               const std::vector<akai2sfz::KurzweilDirEntry> &entries,
                               std::vector<akai2sfz::KurzweilDirEntry> *out) {
  for (const auto &e : entries) {
    if (e.is_directory) {
      collectKrzFilesRecursive(disk, disk.list_directory(e), out);
    } else {
      out->push_back(e);
    }
  }
}
} // namespace

void MainWindow::rebuildProgramTreeKurzweil() {
  programTree_->clear();
  if (!kurzweilDisk_) return;

  std::vector<KurzweilDirEntry> files;
  collectKrzFilesRecursive(*kurzweilDisk_, kurzweilDisk_->list_root(), &files);

  for (const auto &f : files) {
    QString name = QString::fromStdString(f.name);
    auto *item = new QTreeWidgetItem(programTree_, {name});
    item->setData(0, kRoleFileName, name);
    // filho placeholder so para mostrar a seta de expandir; substituido
    // pelos Programs reais em onProgramItemExpanded.
    auto *placeholder = new QTreeWidgetItem(item, {"loading..."});
    placeholder->setData(0, kRolePlaceholder, true);
  }

  setStatus(QString("%1 .krz file(s) found in the image.").arg(files.size()), StatusKind::Info);
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
    item->setIcon(0, akaiIcon_);
    item->setData(0, kRoleFileName, fname);
    item->setData(0, kRoleExt, ext);

    if (ext == "a3p" || ext == "a1p") {
      // filho placeholder so para mostrar a seta de expandir; substituido
      // por samples reais em onProgramItemExpanded (carregamento sob demanda).
      auto *placeholder = new QTreeWidgetItem(item, {"loading..."});
      placeholder->setData(0, kRolePlaceholder, true);
    }
  }

  setStatus(QString("%1 program(s) in this volume (%2 file(s) total, including samples).")
                .arg(programs)
                .arg(total),
            StatusKind::Info);
}

void MainWindow::onProgramItemExpanded(QTreeWidgetItem *item) {
  if (item->childCount() != 1) return;
  QTreeWidgetItem *child = item->child(0);
  if (!child->data(0, kRolePlaceholder).toBool()) return; // ja carregado

  item->removeChild(child);
  delete child;

  if (isRoland_) {
    loadPatchPartialsRoland(item);
  } else if (isEmu_) {
    loadBankPresetsEmu(item);
  } else if (isKurzweil_) {
    loadProgramsKurzweil(item);
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
      new QTreeWidgetItem(patchItem, {"(patch not found)"});
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
      new QTreeWidgetItem(patchItem, {"(no keys mapped)"});
    }
  } catch (const std::exception &e) {
    new QTreeWidgetItem(patchItem, {QString("(error reading patch: %1)").arg(e.what())});
  }
}

void MainWindow::loadBankPresetsEmu(QTreeWidgetItem *bankItem) {
  if (!emuDisk_) return;

  std::string bankName = bankItem->data(0, kRoleFileName).toString().toStdString();

  try {
    EmuFolder folder;
    bool foundFolder = false;
    for (auto &f : emuDisk_->list_folders()) {
      if (f.name == currentFolderName_) {
        folder = std::move(f);
        foundFolder = true;
        break;
      }
    }
    if (!foundFolder) {
      new QTreeWidgetItem(bankItem, {"(folder not found)"});
      return;
    }

    EmuFileEntry bankEntry;
    bool foundBank = false;
    for (auto &f : emuDisk_->list_files(folder)) {
      if (f.name == bankName) {
        bankEntry = std::move(f);
        foundBank = true;
        break;
      }
    }
    if (!foundBank) {
      new QTreeWidgetItem(bankItem, {"(bank not found)"});
      return;
    }

    auto bankBytes = emuDisk_->read_file(bankEntry);
    emu_raw::BankFormat format = detect_emu_bank_format(bankBytes);
    if (format == emu_raw::BankFormat::Unknown) {
      new QTreeWidgetItem(bankItem, {"(unknown bank format)"});
      return;
    }

    int npresets = emu_bank_preset_count(bankBytes, format);
    for (int pi = 0; pi < npresets; ++pi) {
      EmuPreset p = parse_emu_preset(bankBytes, format, pi);
      QString name = QString::fromStdString(p.name);
      auto *item = new QTreeWidgetItem(bankItem, {name});
      item->setIcon(0, emuIcon_);
      item->setData(0, kRolePresetName, name);
    }

    if (npresets == 0) {
      new QTreeWidgetItem(bankItem, {"(no presets)"});
    }
  } catch (const std::exception &e) {
    new QTreeWidgetItem(bankItem, {QString("(error reading bank: %1)").arg(e.what())});
  }
}

void MainWindow::loadProgramsKurzweil(QTreeWidgetItem *krzFileItem) {
  if (!kurzweilDisk_) return;

  std::string fileName = krzFileItem->data(0, kRoleFileName).toString().toStdString();

  try {
    std::vector<KurzweilDirEntry> files;
    collectKrzFilesRecursive(*kurzweilDisk_, kurzweilDisk_->list_root(), &files);

    const KurzweilDirEntry *entry = nullptr;
    for (const auto &f : files) {
      if (f.name == fileName) {
        entry = &f;
        break;
      }
    }
    if (!entry) {
      new QTreeWidgetItem(krzFileItem, {"(file not found)"});
      return;
    }

    auto bytes = kurzweilDisk_->read_file(*entry);
    krz_read_osize(bytes); // valida o magic "PRAM"
    auto objects = list_krz_objects(bytes);

    int shown = 0;
    for (const auto &o : objects) {
      if (o.type_raw != static_cast<int>(krz_raw::ObjectType::Program)) continue;
      QString name = QString::fromStdString(o.name);
      auto *item = new QTreeWidgetItem(krzFileItem, {name});
      item->setIcon(0, kurzweilIcon_);
      item->setData(0, kRolePresetName, name);
      ++shown;
    }

    if (shown == 0) {
      new QTreeWidgetItem(krzFileItem, {"(no programs)"});
    }
  } catch (const std::exception &e) {
    new QTreeWidgetItem(krzFileItem, {QString("(error reading .krz: %1)").arg(e.what())});
  }
}

void MainWindow::loadProgramSamples(QTreeWidgetItem *programItem) {
  if (!partition_) return;

  std::string programName = programItem->data(0, kRoleFileName).toString().toStdString();
  QString ext = programItem->data(0, kRoleExt).toString();
  auto files = list_files(*partition_, currentVolumeIndex_);
  const FileEntry *progEntry = findFile(files, programName, ext.toStdString());
  if (!progEntry) {
    new QTreeWidgetItem(programItem, {"(program not found)"});
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
      new QTreeWidgetItem(programItem, {"(no samples referenced)"});
      return;
    }

    for (const auto &sname : uniqueSamples) {
      const FileEntry *sampleEntry = findFile(files, sname, sampleExt);
      QString sampleExtQ = QString::fromStdString(sampleExt);
      QString label = sampleEntry
                           ? QString("%1.%2  [%3]")
                                 .arg(QString::fromStdString(sname), sampleExtQ,
                                      formatSize(sampleEntry->size))
                           : QString("%1.%2  [not found in volume]")
                                 .arg(QString::fromStdString(sname), sampleExtQ);
      new QTreeWidgetItem(programItem, {label});
    }
  } catch (const std::exception &e) {
    new QTreeWidgetItem(programItem, {QString("(error reading program: %1)").arg(e.what())});
  }
}

bool MainWindow::isConvertibleItem(QTreeWidgetItem *item) const {
  if (!item) return false;
  if (isEmu_ || isKurzweil_) {
    // Ao contrario de Akai/Roland, quem e convertivel em E-mu/Kurzweil e o
    // Preset/Program (filho de um Bank/.KRZ), nao o item de topo -- um
    // bank/.KRZ agrupa varios presets/programs, entao ele proprio nao e
    // uma unidade convertivel.
    return item->parent() != nullptr && item->data(0, kRolePresetName).isValid();
  }
  if (item->parent() != nullptr) return false; // sample/tecla (filho) em Akai/Roland
  if (isRoland_) return true; // todo item de topo em modo Roland e um Patch
  QString ext = item->data(0, kRoleExt).toString();
  return ext == "a3p" || ext == "a1p";
}

QString MainWindow::itemConvertName(QTreeWidgetItem *item) const {
  if (isEmu_ || isKurzweil_) return item->data(0, kRolePresetName).toString();
  return item->data(0, kRoleFileName).toString();
}

void MainWindow::onProgramSelectionChanged() {
  int count = 0;
  for (auto *item : programTree_->selectedItems()) {
    if (isConvertibleItem(item)) ++count;
  }
  convertBtn_->setEnabled(count > 0);
  convertBtn_->setText(count > 1 ? QString("CONVERT %1 SELECTED PROGRAMS").arg(count)
                                  : "CONVERT SELECTED PROGRAM");
}

void MainWindow::onBrowseOutputDir() {
  QString dir = QFileDialog::getExistingDirectory(this, "Output Directory");
  if (!dir.isEmpty()) outputDirEdit_->setText(dir);
}

akai2sfz::ConvertResult MainWindow::convertSelectedRoland(QTreeWidgetItem *patchItem,
                                                           const QString &outDir) {
  if (!rolandDisk_) {
    ConvertResult r;
    r.error = "no Roland disk open";
    return r;
  }
  QString patchName = patchItem->data(0, kRoleFileName).toString();
  return convert_roland_patch(*rolandDisk_, patchName.toStdString(), outDir.toStdString());
}

akai2sfz::ConvertResult MainWindow::convertSelectedEmu(QTreeWidgetItem *presetItem,
                                                        const QString &outDir) {
  QTreeWidgetItem *bankItem = presetItem->parent();
  if (!bankItem || !emuDisk_) {
    ConvertResult r;
    r.error = "invalid E-mu selection";
    return r;
  }
  QString bankName = bankItem->data(0, kRoleFileName).toString();
  QString presetName = presetItem->data(0, kRolePresetName).toString();
  return convert_emu_preset(*emuDisk_, currentFolderName_, bankName.toStdString(),
                             presetName.toStdString(), outDir.toStdString());
}

akai2sfz::ConvertResult MainWindow::convertSelectedKurzweil(QTreeWidgetItem *programItem,
                                                             const QString &outDir) {
  QTreeWidgetItem *krzFileItem = programItem->parent();
  if (!krzFileItem || !kurzweilDisk_) {
    ConvertResult r;
    r.error = "invalid Kurzweil selection";
    return r;
  }
  QString krzFileName = krzFileItem->data(0, kRoleFileName).toString();
  QString programName = programItem->data(0, kRolePresetName).toString();
  return convert_krz_program(*kurzweilDisk_, krzFileName.toStdString(),
                              programName.toStdString(), outDir.toStdString());
}

akai2sfz::ConvertResult MainWindow::convertSelectedAkai(QTreeWidgetItem *programItem,
                                                         const QString &outDir) {
  if (!partition_) {
    ConvertResult r;
    r.error = "no partition open";
    return r;
  }
  QString volName = QString::fromStdString(partition_->volume_name(currentVolumeIndex_));
  QString fileName = programItem->data(0, kRoleFileName).toString();
  return convert_program(*partition_, volName.toStdString(), fileName.toStdString(),
                          outDir.toStdString());
}

akai2sfz::ConvertResult MainWindow::convertItem(QTreeWidgetItem *item, const QString &outDir) {
  if (isEmu_) return convertSelectedEmu(item, outDir);
  if (isKurzweil_) return convertSelectedKurzweil(item, outDir);
  if (isRoland_) return convertSelectedRoland(item, outDir);
  return convertSelectedAkai(item, outDir);
}

// Converte todos os itens convertiveis selecionados (1 ou varios -- Cmd/
// Shift-click habilita lote). Cada um vai pra sua PROPRIA subpasta dentro
// do diretorio escolhido, nomeada com o nome do program/patch/preset
// sanitizado -- assim SFZs/WAVs de presets diferentes nunca colidem. Um so
// dialogo de resumo no final, mesmo em lote (dialogo por item seria
// inviavel com varias dezenas selecionadas).
void MainWindow::onConvertSelected() {
  QList<QTreeWidgetItem *> targets;
  for (auto *item : programTree_->selectedItems()) {
    if (isConvertibleItem(item)) targets.push_back(item);
  }
  if (targets.isEmpty()) return;

  QString baseOutDir = outputDirEdit_->text();
  if (baseOutDir.isEmpty()) {
    QMessageBox::warning(this, "Output Directory",
                          "Choose an output directory before converting.");
    return;
  }

  int successCount = 0;
  int totalWav = 0;
  QStringList failedNames;

  for (auto *item : targets) {
    QString name = itemConvertName(item);
    QString subDir = baseOutDir + "/" + QString::fromStdString(sanitize_filename(name.toStdString()));

    log(QString("Converting '%1' -> %2 ...").arg(name, subDir));
    setStatus(QString("Converting (%1/%2)...")
                  .arg(successCount + failedNames.size() + 1)
                  .arg(targets.size()),
              StatusKind::Warn);

    ConvertResult result = convertItem(item, subDir);

    for (const auto &w : result.warnings) {
      log("warning: " + QString::fromStdString(w));
    }

    if (!result.success) {
      log("error: " + QString::fromStdString(result.error));
      failedNames << name;
      continue;
    }

    log(QString("OK: %1 (%2 WAV)")
            .arg(QString::fromStdString(result.sfz_path))
            .arg(result.wav_paths.size()));
    ++successCount;
    totalWav += static_cast<int>(result.wav_paths.size());
  }

  QString summary = targets.size() == 1
      ? QString("%1/%2 converted (%3 WAV file(s)).").arg(successCount).arg(targets.size()).arg(totalWav)
      : QString("%1/%2 presets converted successfully (%3 WAV files total).")
            .arg(successCount)
            .arg(targets.size())
            .arg(totalWav);
  setStatus(summary, failedNames.isEmpty() ? StatusKind::Ok : StatusKind::Error);

  if (failedNames.isEmpty()) {
    QMessageBox::information(this, "Conversion complete", summary);
  } else {
    QMessageBox::warning(this, "Conversion finished with errors",
                          summary + "\n\nFailed: " + failedNames.join(", "));
  }
}
