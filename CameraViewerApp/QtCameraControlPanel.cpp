#include "QtCameraControlPanel.h"
#include "CameraHelpers.h"
#include "MetricsManager.h"
#include "QtCameraConnectionManager.h"
#include "QtCameraViewer.h"
#include "QtVideoWidget.h"
#include "metricscontroller.h"
#include "widgets/graphwidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <qguiapplication.h>

using namespace CameraLibrary;

// Specialized collection of widgets for camera controls

CameraControlPanel::CameraControlPanel(CameraConnectionManager *mgr,
                                       MetricsManager &mMgr, QWidget *parent)
    : QWidget(parent), camera_manager(mgr), metrics_manager(mMgr) {
  buildUi();
  connect(this, &CameraControlPanel::showWarning, this,
          [](const QString &t, const QString &m) {
            QMessageBox::warning(nullptr, t, m);
          });
}

bool CameraControlPanel::currentSerialValid() const {
  return selected_serial != 0;
}

bool CameraControlPanel::isMarkerZoomPossible() const {
  return markerZoomPossible;
}

void CameraControlPanel::buildUi() {
  // auto* root = new QVBoxLayout(this);
  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(6);

  rightTabWidget = new QTabWidget(this);
  // use the 'underline' tab style (sleek blue underline for active tab)
  if (rightTabWidget->tabBar()) {
    rightTabWidget->tabBar()->setProperty("underline", true);
    // give the left tab bar a stable object name so CSS can target it precisely
    rightTabWidget->tabBar()->setObjectName("leftControlTabs");
  }
  rightTabWidget->setMinimumWidth(450);
  rightTabWidget->setMaximumWidth(450);

  /*
  ********** Tab: General operator workflow ***************
  */

  auto *tabGeneral = new QWidget;
  auto *scrollAreaGeneral = new QScrollArea;
  scrollAreaGeneral->setWidget(tabGeneral);
  scrollAreaGeneral->setWidgetResizable(true);
  scrollAreaGeneral->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollAreaGeneral->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  auto *vGeneral = new QVBoxLayout(tabGeneral);
  vGeneral->setSpacing(10);

  auto *generalExposureWidget = new QWidget(tabGeneral);
  auto *generalExposureLayout = new QVBoxLayout(generalExposureWidget);
  generalExposureLayout->setContentsMargins(6, 6, 6, 6);
  generalExposureLayout->setSpacing(6);
  general_exposure_title_label = new QLabel(generalExposureWidget);
  generalExposureLayout->addWidget(general_exposure_title_label, 0,
                                   Qt::AlignLeft);

  general_exposure_slider = new QSlider(Qt::Horizontal, generalExposureWidget);
  general_exposure_slider->setRange(1, 200);
  general_exposure_slider->setValue(50);
  general_exposure_slider->setSizePolicy(QSizePolicy::Expanding,
                                         QSizePolicy::Fixed);
  general_exposure_slider->setToolTip("Drag slider to adjust exposure");

  general_exposure_label = new QLabel(generalExposureWidget);
  general_exposure_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  general_exposure_label->setMinimumWidth(90);
  general_exposure_label->setMaximumWidth(90);

  auto *generalExposureRow = new QWidget(generalExposureWidget);
  auto *generalExposureRowLayout = new QHBoxLayout(generalExposureRow);
  generalExposureRowLayout->setContentsMargins(0, 0, 0, 0);
  generalExposureRowLayout->setSpacing(8);
  generalExposureRowLayout->addWidget(general_exposure_slider);
  generalExposureRowLayout->addWidget(general_exposure_label);
  generalExposureLayout->addWidget(generalExposureRow);
  vGeneral->addWidget(generalExposureWidget);

  auto *generalZoomModeWidget = new QWidget(tabGeneral);
  auto *generalZoomModeLayout = new QVBoxLayout(generalZoomModeWidget);
  generalZoomModeLayout->setContentsMargins(6, 0, 6, 0);
  generalZoomModeLayout->setSpacing(6);
  general_lens_inspection_mode_label = new QLabel(generalZoomModeWidget);
  generalZoomModeLayout->addWidget(general_lens_inspection_mode_label, 0,
                                   Qt::AlignLeft);

  general_lens_inspection_mode_combo = new QComboBox(generalZoomModeWidget);
  repopulateLensInspectionModes();
  generalZoomModeLayout->addWidget(general_lens_inspection_mode_combo);
  vGeneral->addWidget(generalZoomModeWidget);

  auto *generalZoomWidget = new QWidget(tabGeneral);
  auto *generalZoomLayout = new QVBoxLayout(generalZoomWidget);
  generalZoomLayout->setContentsMargins(6, 0, 6, 0);
  generalZoomLayout->setSpacing(6);
  general_zoom_title_label = new QLabel(generalZoomWidget);
  generalZoomLayout->addWidget(general_zoom_title_label, 0, Qt::AlignLeft);

  general_zoom_slider = new QSlider(Qt::Horizontal, generalZoomWidget);
  general_zoom_slider->setRange(10, 200);
  general_zoom_slider->setValue(10);
  general_zoom_slider->setSingleStep(1);
  general_zoom_slider->setPageStep(5);
  general_zoom_slider->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Fixed);
  general_zoom_slider->setToolTip(
      "Drag slider to adjust zoom (1.0x - 20.0x in 0.1x steps)");

  general_zoom_label = new QLabel("1.0x", generalZoomWidget);
  general_zoom_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  general_zoom_label->setMinimumWidth(70);
  general_zoom_label->setMaximumWidth(70);

  auto *generalZoomRow = new QWidget(generalZoomWidget);
  auto *generalZoomRowLayout = new QHBoxLayout(generalZoomRow);
  generalZoomRowLayout->setContentsMargins(0, 0, 0, 0);
  generalZoomRowLayout->setSpacing(8);
  generalZoomRowLayout->addWidget(general_zoom_slider);
  generalZoomRowLayout->addWidget(general_zoom_label);
  generalZoomLayout->addWidget(generalZoomRow);
  vGeneral->addWidget(generalZoomWidget);

  QVector<QString> generalFocusLabels = {"FocusQuality"};
  QVector<QString> generalFocusDescriptions = {""};
  QVector<bool> generalFocusGraphs = {true};
  general_focus_metrics_widgets = createCompactMetricWidgets(
      QString(), QString(), generalFocusLabels, generalFocusDescriptions,
      generalFocusGraphs, 88);
  general_focus_metrics_widgets->passingThreshold = 0.65;
  if (!general_focus_metrics_widgets->dataLabels.isEmpty()) {
    QLabel *focusScoreDataLabel =
        general_focus_metrics_widgets->dataLabels.first();
    QFont focusScoreFont = focusScoreDataLabel->font();
    focusScoreFont.setPixelSize(58);
    focusScoreFont.setBold(true);
    focusScoreDataLabel->setFont(focusScoreFont);
    focusScoreDataLabel->setProperty("metricFontSizePx", 58);
    focusScoreDataLabel->setFixedWidth(165);
  }
  vGeneral->addWidget(general_focus_metrics_widgets->groupBox);

  QVector<QString> generalLensLabels = {"LensHealth"};
  QVector<QString> generalLensDescriptions = {""};
  QVector<bool> generalLensGraphs = {true};
  general_lens_metrics_widgets = createCompactMetricWidgets(
      QString(), QString(), generalLensLabels, generalLensDescriptions,
      generalLensGraphs, 88);
  general_lens_metrics_widgets->passingThreshold = 0.90;
  vGeneral->addWidget(general_lens_metrics_widgets->groupBox);
  vGeneral->addStretch();

  rightTabWidget->addTab(scrollAreaGeneral, QString());

  /*
  ********** Tab: Camera Controls and Video Modes ***************
  */

  auto *tab0 = new QWidget;
  auto *scrollArea0 = new QScrollArea;
  scrollArea0->setWidget(tab0);
  scrollArea0->setWidgetResizable(true);
  scrollArea0->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea0->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  auto *v0 = new QVBoxLayout(tab0);

  // Group: Camera Controls (exposure, fps, gain)

  cam_group = new QGroupBox(tab0);
  auto *camLayout = new QVBoxLayout();
  camLayout->setContentsMargins(6, 6, 6, 6);
  cam_group->setLayout(camLayout);

  // Exposure: slider from 1 to 200
  exposure_slider = new QSlider(Qt::Horizontal, cam_group);
  exposure_slider->setRange(1, 200);
  exposure_slider->setValue(50);
  exposure_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  exposure_slider->setToolTip("Drag slider to adjust exposure");
  exposure_edit = new QLineEdit(cam_group);
  exposure_edit->setValidator(new QIntValidator(1, 200, exposure_edit));
  exposure_edit->setMaximumWidth(64);
  exposure_edit->setToolTip("Enter an exposure value here");
  exposure_label = new QLabel(cam_group);
  exposure_label->setMaximumWidth(75);
  exposure_label->setMinimumWidth(75);
  connect(exposure_slider, QOverload<int>::of(&QSlider::valueChanged), this,
          [this](int val) { updateSliderLabels(); });
  connect(exposure_slider, &QAbstractSlider::sliderReleased, this,
          [this]() { onSetExposure(); });
  connect(exposure_edit, &QLineEdit::textEdited, this,
          [this](const QString &text) {
            bool ok = false;
            const int v = text.toInt(&ok);
            if (!ok)
              return;
            exposure_slider->setValue(qBound(1, v, 200));
            onSetExposure();
          });
  connect(exposure_edit, &QLineEdit::editingFinished, this, [this]() {
    bool ok = false;
    if (!exposure_edit->text().toInt(&ok) || !ok)
      exposure_edit->setText(QString::number(exposure_slider->value()));
  });
  if (general_exposure_slider) {
    connect(exposure_slider, &QSlider::valueChanged, general_exposure_slider,
            &QSlider::setValue);
    connect(general_exposure_slider, &QSlider::valueChanged, exposure_slider,
            &QSlider::setValue);
    connect(general_exposure_slider, &QSlider::valueChanged, this,
            &CameraControlPanel::updateGeneralExposureLabel);
    connect(general_exposure_slider, &QAbstractSlider::sliderReleased, this,
            [this]() { onSetExposure(); });
  }
  // Frame Rate: slider from 1 to 1000
  fps_slider = new QSlider(Qt::Horizontal, cam_group);
  fps_slider->setRange(1, 1000);
  fps_slider->setValue(30);
  fps_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  fps_slider->setToolTip("Drag slider to adjust maximum framerate");
  fps_edit = new QLineEdit(cam_group);
  fps_edit->setValidator(new QIntValidator(1, 1000, fps_edit));
  fps_edit->setMaximumWidth(64);
  fps_edit->setToolTip("Enter a new framerate here");
  fps_label = new QLabel(cam_group);
  fps_label->setMaximumWidth(80);
  fps_label->setMinimumWidth(80);
  connect(fps_slider, QOverload<int>::of(&QSlider::valueChanged), this,
          [this](int val) {
            Q_UNUSED(val);
            updateSliderLabels();
          });
  connect(fps_slider, &QAbstractSlider::sliderReleased, this,
          [this]() { onSetFps(); });
  connect(fps_edit, &QLineEdit::textEdited, this, [this](const QString &text) {
    bool ok = false;
    const int v = text.toInt(&ok);
    if (!ok)
      return;
    fps_slider->setValue(qBound(1, v, 1000));
    onSetFps();
  });
  connect(fps_edit, &QLineEdit::editingFinished, this, [this]() {
    bool ok = false;
    if (!fps_edit->text().toInt(&ok) || !ok)
      fps_edit->setText(QString::number(fps_slider->value()));
  });
  // Gain: slider from 0 to 7
  gain_slider = new QSlider(Qt::Horizontal, cam_group);
  gain_slider->setRange(0, 7);
  gain_slider->setValue(0);
  gain_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  gain_slider->setToolTip("Drag slider to adjust gain");
  gain_edit = new QLineEdit(cam_group);
  gain_edit->setValidator(new QIntValidator(0, 7, gain_edit));
  gain_edit->setMaximumWidth(64);
  gain_edit->setToolTip("Enter new gain here");
  gain_label = new QLabel(cam_group);
  gain_label->setMaximumWidth(60);
  gain_label->setMinimumWidth(60);
  connect(gain_slider, QOverload<int>::of(&QSlider::valueChanged), this,
          [this](int val) {
            Q_UNUSED(val);
            updateSliderLabels();
          });
  connect(gain_slider, &QAbstractSlider::sliderReleased, this,
          [this]() { onSetGain(); });
  connect(gain_edit, &QLineEdit::textEdited, this, [this](const QString &text) {
    bool ok = false;
    const int v = text.toInt(&ok);
    if (!ok)
      return;
    gain_slider->setValue(qBound(0, v, 7));
    onSetGain();
  });
  connect(gain_edit, &QLineEdit::editingFinished, this, [this]() {
    bool ok = false;
    if (!gain_edit->text().toInt(&ok) || !ok)
      gain_edit->setText(QString::number(gain_slider->value()));
  });
  // Build compact horizontal widgets for each camera control
  auto *exposureWidget = new QWidget(cam_group);
  auto *expLayout = new QVBoxLayout(exposureWidget);
  expLayout->setContentsMargins(0, 0, 0, 0);
  expLayout->setSpacing(8);
  exposure_title_label = new QLabel(exposureWidget);
  exposure_title_label->setMinimumWidth(80);
  exposure_title_label->setMaximumWidth(80);
  expLayout->addWidget(exposure_title_label, 0, Qt::AlignLeft);
  auto *exposureRow = new QWidget(exposureWidget);
  auto *exposureRowLayout = new QHBoxLayout(exposureRow);
  exposureRowLayout->setContentsMargins(0, 0, 0, 0);
  exposureRowLayout->setSpacing(6);
  exposureRowLayout->addWidget(exposure_slider);
  exposureRowLayout->addWidget(exposure_edit);
  expLayout->addWidget(exposureRow);
  expLayout->addWidget(exposure_label, 0, Qt::AlignLeft);

  auto *fpsWidget = new QWidget(cam_group);
  auto *fpsLayoutW = new QVBoxLayout(fpsWidget);
  fpsLayoutW->setContentsMargins(0, 0, 0, 0);
  fpsLayoutW->setSpacing(8);
  fps_title_label = new QLabel(fpsWidget);
  fps_title_label->setMinimumWidth(80);
  fps_title_label->setMaximumWidth(80);
  fpsLayoutW->addWidget(fps_title_label, 0, Qt::AlignLeft);
  auto *fpsRow = new QWidget(fpsWidget);
  auto *fpsRowLayout = new QHBoxLayout(fpsRow);
  fpsRowLayout->setContentsMargins(0, 0, 0, 0);
  fpsRowLayout->setSpacing(6);
  fpsRowLayout->addWidget(fps_slider);
  fpsRowLayout->addWidget(fps_edit);
  fpsLayoutW->addWidget(fpsRow);
  fpsLayoutW->addWidget(fps_label, 0, Qt::AlignLeft);

  auto *gainWidget = new QWidget(cam_group);
  auto *gainLayoutW = new QVBoxLayout(gainWidget);
  gainLayoutW->setContentsMargins(0, 0, 0, 0);
  gainLayoutW->setSpacing(8);
  gain_title_label = new QLabel(gainWidget);
  gain_title_label->setMaximumWidth(80);
  gain_title_label->setMinimumWidth(80);
  gainLayoutW->addWidget(gain_title_label, 0, Qt::AlignLeft);
  auto *gainRow = new QWidget(gainWidget);
  auto *gainRowLayout = new QHBoxLayout(gainRow);
  gainRowLayout->setContentsMargins(0, 0, 0, 0);
  gainRowLayout->setSpacing(6);
  gainRowLayout->addWidget(gain_slider);
  gainRowLayout->addWidget(gain_edit);
  gainLayoutW->addWidget(gainRow);
  gainLayoutW->addWidget(gain_label, 0, Qt::AlignLeft);

  camLayout->addWidget(exposureWidget);
  camLayout->addWidget(fpsWidget);
  camLayout->addWidget(gainWidget);

  // Edge Detect mode: behave like Grayscale but enable an edge-overlay in the
  // viewer Add a Video Mode dropdown next to existing controls so modes appear
  // with other controls
  video_mode_combo = new QComboBox(tab0);
  video_mode_combo->setToolTip("Click to select a new video mode");
  repopulateVideoModes();

  // Selecting any regular mode should disable Edge Detect if it was enabled
  connect(video_mode_combo,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
          [this](int idx) {
            emit resetFocusStats(); // reset focus data on mode change

            if (!currentSerialValid()) {
              emit showWarning(tr("No Camera"),
                               tr("No camera is currently selected."));
              return;
            }

            // find mode value from current data and request it
            QVariant itemData = video_mode_combo->itemData(idx);
            int mode;

            // Check if data is a list (Grayscale modes) or a single int (other
            // modes)
            if (itemData.canConvert<QVariantList>()) {
              QVariantList dataList = itemData.toList();
              mode = dataList[0].toInt();
              setMarkerZoomPossible(true);
            } else {
              mode = itemData.toInt();
              setMarkerZoomPossible(false);
              onMarkerZoomToggled(false);
            }

            onSetVideoMode(mode);

            // Disable edge button and Zoom controls for incompatible modes
            // (Segment, Object, Duplex)
            const bool isCompatible = isEdgeDetectCompatible(mode);
            edge_button->setEnabled(isCompatible);

            // // Disable ROI Zoom option in combo box
            // QStandardItemModel *model =
            //     qobject_cast<QStandardItemModel
            //     *>(lens_inspection_mode_combo->model());
            // Q_ASSERT(model != nullptr);
            // bool disabled = true;
            // QStandardItem *item = model->item(1);
            // item->setFlags(disabled ? item->flags() & ~Qt::ItemIsEnabled
            //                         : item->flags() | Qt::ItemIsEnabled);
            // lens_inspection_mode_combo->show();

            // handle ROI-Zoom UI behavior
            bool possible = isMarkerZoomPossible();
            updateMarkerZoomControlsEnabled(possible);
            if (!possible) {
              qDebug("marker zoom not possible");
              zoom_slider->setValue(10);
              setLensInspectionModeIndex(0);
            } else {
              qDebug("marker zoom is possible");
              // lens_inspection_mode_combo->setItemData(1, true, Qt::UserRole
              // -1);
            }

            if (!isCompatible && edge_button->isChecked()) {
              edge_button->setChecked(false);
              emit edgeDetectToggled(false);
            }

            // Handle ROI marker zoom case with grayscale mode
            emit onMarkerZoomPossible(possible);
          });

  // Group: Video Modes (dropdown + Edge Detect toggle)
  video_group = new QGroupBox(tab0);
  auto *video_layout = new QVBoxLayout(video_group);
  video_layout->setContentsMargins(6, 6, 6, 6);
  video_group->setLayout(video_layout);
  video_layout->addWidget(video_mode_combo);
  edge_button = new QPushButton(video_group);
  edge_button->setCheckable(true);
  edge_button->setProperty("secondary", true);

  // Set initial state based on first item in combo (Segment mode)
  const int initialMode = video_mode_combo->itemData(0).toInt();
  edge_button->setEnabled(isEdgeDetectCompatible(initialMode));

  connect(edge_button, &QPushButton::toggled, this, [this](bool checked) {
    if (checked) {
      if (!currentSerialValid()) {
        emit showWarning(tr("No Camera"),
                         tr("No camera is currently selected."));
        edge_button->setChecked(false);
        return;
      }
    }
    emit edgeDetectToggled(checked);
  });
  edge_button->setToolTip("Click to enable edge detection overlay");

  rightTabWidget->addTab(scrollArea0, QString());

  // add camera controls and video modes to tab
  v0->addWidget(cam_group);
  v0->addWidget(video_group);
  v0->addStretch();
  video_layout->addWidget(edge_button);

  /*
  ********** Tab: Focus Tool and Lens Inspection ***************
  */

  // Row: Video modes - convert previous buttons into a single dropdown embedded
  // with other controls
  auto *tab1 = new QWidget;
  auto *scrollArea1 = new QScrollArea;
  scrollArea1->setWidget(tab1);
  scrollArea1->setWidgetResizable(true);
  scrollArea1->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea1->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  auto *v1 = new QVBoxLayout(tab1);

  // Group: Focus Tool

  auto *focusToolGroup = new QGroupBox("Focus Tool", tab1);
  auto *focusToolLayout = new QVBoxLayout();
  focusToolLayout->setContentsMargins(6, 6, 6, 6);
  focusToolGroup->setLayout(focusToolLayout);

  // Focus Tool enable/disable checkbox
  focus_button = new QCheckBox(focusToolGroup);
  focus_button->setText("Focus Enabled");
  focus_button->setChecked(true);
  focus_button->setToolTip("When checked, focus assist tool is enabled");
  connect(focus_button, &QCheckBox::clicked, this, [this]() {
    focusState = !focusState;
    if (focusState) {
      focus_button->setText("Focus Enabled");
      emit focusToolToggled(true);
    } else {
      focus_button->setText("Focus Disabled");
      emit focusToolToggled(false);
    }
  });

  // Focus HUD enable/disable checkbox
  focusHUD_button = new QCheckBox(focusToolGroup);
  focusHUD_button->setText("Focus HUD Enabled");
  focusHUD_button->setChecked(true);
  focusHUD_button->setToolTip(
      "When checked, focus and lens grading HUD is enabled");
  connect(focusHUD_button, &QCheckBox::clicked, this, [this]() {
    focusHUDState = !focusHUDState;
    if (focusHUDState) {
      focusHUD_button->setText("Focus HUD Enabled");
      emit focusHUDToggled(true);
    } else {
      focusHUD_button->setText("Focus HUD Disabled");
      emit focusHUDToggled(false);
    }
  });

  focusToolLayout->addWidget(focus_button);
  focusToolLayout->addWidget(focusHUD_button);

  rightTabWidget->addTab(scrollArea1, QString());
  v1->addWidget(focusToolGroup);

  // Group: Lens Inspection

  lens_inspection_group = new QGroupBox(tr("Lens Inspection"));
  auto *lensInspectionLayout = new QVBoxLayout(lens_inspection_group);
  lensInspectionLayout->setContentsMargins(6, 6, 6, 6);

  lens_inspection_mode_label = new QLabel(tr("Mode:"), lens_inspection_group);
  lens_inspection_mode_label->setMaximumWidth(60);
  lens_inspection_mode_label->setMinimumWidth(60);
  lens_inspection_mode_combo = new QComboBox(tab1);
  repopulateLensInspectionModes();

  connect(lens_inspection_mode_combo,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
          &CameraControlPanel::applyLensInspectionModeSelection);
  if (general_lens_inspection_mode_combo) {
    connect(general_lens_inspection_mode_combo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            &CameraControlPanel::applyLensInspectionModeSelection);
  }

  // Zoom Slider: 1.0x to 20.0x in 0.1x steps
  // The slider displays tenths-of-zoom (range 10–200), divided by 10.0f to get
  // the true value.
  zoom_slider = new QSlider(Qt::Horizontal, lens_inspection_group);
  zoom_slider->setRange(10, 200);
  zoom_slider->setValue(10);
  zoom_slider->setSingleStep(1); // 0.1x per arrow-key tick
  zoom_slider->setPageStep(5);   // 0.5x per page-up/down
  zoom_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  zoom_slider->setToolTip(
      "Drag slider to adjust zoom (1.0x – 20.0x in 0.1x steps)");
  zoom_label = new QLabel("1.0x", lens_inspection_group);
  zoom_label->setMaximumWidth(60);
  zoom_label->setMinimumWidth(60);

  connect(zoom_slider, QOverload<int>::of(&QSlider::valueChanged), this,
          [this](int val) {
            // Convert tenths back to the real zoom value for display and
            // emission.
            float zoom = val / 10.0f;
            zoom_label->setText(QString::number(zoom, 'f', 1) + "x");
            onSetZoom(false);
          });

  zoom_button = new QPushButton(tr("Reset"), lens_inspection_group);
  zoom_button->setProperty("secondary", true);
  zoom_button->setToolTip("Click to reset zoom to default");
  connect(zoom_button, &QPushButton::clicked, this, [this]() {
    zoom_slider->setValue(10); // 10*0.1 = 1.0x initial zoom
  });
  zoom_button->setEnabled(false);
  zoom_slider->setEnabled(false);
  if (general_zoom_slider) {
    connect(zoom_slider, &QSlider::valueChanged, general_zoom_slider,
            &QSlider::setValue);
    connect(general_zoom_slider, &QSlider::valueChanged, zoom_slider,
            &QSlider::setValue);
    connect(general_zoom_slider, &QSlider::valueChanged, this,
            &CameraControlPanel::updateGeneralZoomLabel);
  }

  auto *zoomWidget = new QWidget(lens_inspection_group);
  zoomWidget->setToolTip("Zooms into captured image. Available only in "
                         "Grayscale + ROI Zoom mode.");
  auto *zoomLayoutW = new QVBoxLayout(zoomWidget);
  zoomLayoutW->setContentsMargins(0, 0, 0, 0);
  zoomLayoutW->setSpacing(8);
  zoom_title_label = new QLabel(tr("Zoom:"), zoomWidget);
  zoom_title_label->setMinimumWidth(80);
  zoom_title_label->setMaximumWidth(80);
  zoomLayoutW->addWidget(zoom_title_label, 0, Qt::AlignLeft);
  zoomLayoutW->addWidget(zoom_slider);
  zoomLayoutW->addWidget(zoom_label, 0, Qt::AlignLeft);
  zoomLayoutW->addWidget(zoom_button);

  lensInspectionLayout->addWidget(lens_inspection_mode_label);
  lensInspectionLayout->addWidget(lens_inspection_mode_combo);
  lensInspectionLayout->addWidget(zoomWidget);

  // Hough Circle Detection
  circle_detect_button =
      new QPushButton(tr("Enable Circle Detection"), lens_inspection_group);
  circle_detect_button->setCheckable(true);
  circle_detect_button->setChecked(false);
  circle_detect_button->setProperty("secondary", true);
  circle_detect_button->setToolTip("Click to enable circle marker detection");
  connect(circle_detect_button, &QPushButton::clicked, this,
          [this](bool checked) { emit circleDetectionToggled(checked); });

  circle_count_label =
      new QLabel(tr("Circles Detected: %1").arg(0), lens_inspection_group);

  // Circle detection param2 (accumulator threshold)
  circle_param2_slider = new QSlider(Qt::Horizontal, lens_inspection_group);
  circle_param2_slider->setRange(1, 100);
  circle_param2_slider->setValue(10);
  circle_param2_slider->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Fixed);
  circle_param2_slider->setToolTip(
      "Click to adjust accumulator threshold parameter value for circle marker "
      "detection");

  circle_param2_edit = new QLineEdit(lens_inspection_group);
  circle_param2_edit->setText("10");
  circle_param2_edit->setMaximumWidth(60);
  circle_param2_edit->setToolTip(
      "Enter a new accumulator threshold value here");
  connect(circle_param2_edit, &QLineEdit::textChanged, this,
          &CameraControlPanel::onCircleParam2Changed);
  connect(
      circle_param2_slider, QOverload<int>::of(&QSlider::valueChanged), this,
      [this](int val) { circle_param2_edit->setText(QString::number(val)); });

  // Build circle detection controls widget
  auto *circleCtrlsWidget = new QWidget(lens_inspection_group);
  auto *circleCtrlsLayout = new QVBoxLayout(circleCtrlsWidget);
  circleCtrlsLayout->setContentsMargins(0, 0, 0, 0);
  circleCtrlsLayout->setSpacing(8);

  circleCtrlsLayout->addWidget(circle_detect_button);
  circleCtrlsLayout->addWidget(circle_count_label, 0, Qt::AlignLeft);

  auto *param2LblLayout = new QHBoxLayout();
  circle_param2_title_label =
      new QLabel(tr("Param2 (Threshold):"), circleCtrlsWidget);
  circle_param2_title_label->setMinimumWidth(120);
  circle_param2_title_label->setMaximumWidth(120);
  param2LblLayout->addWidget(circle_param2_title_label, 0, Qt::AlignLeft);
  param2LblLayout->addWidget(circle_param2_slider);
  param2LblLayout->addWidget(circle_param2_edit);
  param2LblLayout->addStretch();
  circleCtrlsLayout->addLayout(param2LblLayout);

  lensInspectionLayout->addSpacing(12);
  hough_circle_header_label =
      new QLabel(QStringLiteral("<b>%1</b>").arg(tr("Hough Circle Detection")),
                 lens_inspection_group);
  lensInspectionLayout->addWidget(hough_circle_header_label);
  lensInspectionLayout->addWidget(circleCtrlsWidget);

  v1->addWidget(lens_inspection_group);
  v1->addStretch();

  /*
  *************** Tab: Color compression / gamma ***************
  */

  auto *tab2 = new QWidget;
  auto *scrollArea2 = new QScrollArea;
  scrollArea2->setWidget(tab2);
  scrollArea2->setWidgetResizable(true);
  scrollArea2->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea2->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  auto *v2 = new QVBoxLayout(tab2);

  // Group: Color Compression (quality, bitrate, mode dropdown)
  compression_group = new QGroupBox(tab2);
  auto *compLayout = new QVBoxLayout(compression_group);
  compLayout->setContentsMargins(6, 6, 6, 6);
  compression_group->setLayout(compLayout);

  // Quality slider (0.0 - 1.0, scaled to 0-100 for slider)
  quality_slider = new QSlider(Qt::Horizontal, compression_group);
  quality_slider->setRange(0, 100);
  quality_slider->setValue(75);
  quality_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  quality_slider->setToolTip("Drag slider to adjust quality");
  quality_edit = new QLineEdit(compression_group);
  quality_edit->setValidator(new QDoubleValidator(0.0, 1.0, 2, quality_edit));
  quality_edit->setMaximumWidth(64);
  quality_edit->setToolTip("Enter a new quality value here");
  quality_label = new QLabel(compression_group);
  quality_label->setMaximumWidth(50);
  quality_label->setMinimumWidth(50);
  connect(quality_slider, QOverload<int>::of(&QSlider::valueChanged), this,
          [this](int val) {
            quality_label->setText(QString::number(val / 100.0, 'f', 2));
            if (quality_edit)
              quality_edit->setText(QString::number(val / 100.0, 'f', 2));
          });
  connect(quality_slider, &QAbstractSlider::sliderReleased, this,
          [this]() { onSetCompression(); });
  connect(quality_edit, &QLineEdit::textEdited, this,
          [this](const QString &text) {
            bool ok = false;
            const double v = text.toDouble(&ok);
            if (!ok)
              return;
            quality_slider->setValue(
                static_cast<int>(qBound(0.0, v, 1.0) * 100.0 + 0.5));
            onSetCompression();
          });
  connect(quality_edit, &QLineEdit::editingFinished, this, [this]() {
    bool ok = false;
    if (!quality_edit->text().toDouble(&ok) && !ok)
      quality_edit->setText(
          QString::number(quality_slider->value() / 100.0, 'f', 2));
  });

  // Bitrate slider (0 - 10000 Mbps)
  bitrate_slider = new QSlider(Qt::Horizontal, compression_group);
  bitrate_slider->setRange(0, 200);
  bitrate_slider->setValue(50);
  bitrate_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  bitrate_slider->setToolTip("Drag slider to adjust bitrate");
  bitrate_edit = new QLineEdit(compression_group);
  bitrate_edit->setValidator(new QDoubleValidator(0.0, 2.0, 2, bitrate_edit));
  bitrate_edit->setMaximumWidth(64);
  bitrate_edit->setToolTip("Enter a new bitrate value here");
  bitrate_label = new QLabel(compression_group);
  bitrate_label->setMaximumWidth(60);
  bitrate_label->setMinimumWidth(60);
  connect(bitrate_slider, QOverload<int>::of(&QSlider::valueChanged), this,
          [this](int val) {
            bitrate_label->setText(QString::number(val / 100.0, 'f', 2));
            if (bitrate_edit)
              bitrate_edit->setText(QString::number(val / 100.0, 'f', 2));
          });
  connect(bitrate_slider, &QAbstractSlider::sliderReleased, this,
          [this]() { onSetCompression(); });
  connect(bitrate_edit, &QLineEdit::textEdited, this,
          [this](const QString &text) {
            bool ok = false;
            const double v = text.toDouble(&ok);
            if (!ok)
              return;
            bitrate_slider->setValue(
                static_cast<int>(qBound(0.0, v, 2.0) * 100.0 + 0.5));
            onSetCompression();
          });
  connect(bitrate_edit, &QLineEdit::editingFinished, this, [this]() {
    bool ok = false;
    if (!bitrate_edit->text().toDouble(&ok) && !ok)
      bitrate_edit->setText(
          QString::number(bitrate_slider->value() / 100.0, 'f', 2));
  });

  mode_combo = new QComboBox(compression_group);
  mode_combo->setToolTip("Click to select a new video compression mode");
  repopulateCompressionModes();

  // Build compression controls widget
  auto *compressionCtrlsWidget = new QWidget(compression_group);
  auto *compressionCtrlsLayout = new QVBoxLayout(compressionCtrlsWidget);
  compressionCtrlsLayout->setContentsMargins(0, 0, 0, 0);
  compressionCtrlsLayout->setSpacing(8);
  quality_title_label = new QLabel(compressionCtrlsWidget);
  compressionCtrlsLayout->addWidget(quality_title_label, 0, Qt::AlignLeft);
  auto *qualityRow = new QWidget(compressionCtrlsWidget);
  auto *qualityRowLayout = new QHBoxLayout(qualityRow);
  qualityRowLayout->setContentsMargins(0, 0, 0, 0);
  qualityRowLayout->setSpacing(6);
  qualityRowLayout->addWidget(quality_slider);
  qualityRowLayout->addWidget(quality_edit);
  compressionCtrlsLayout->addWidget(qualityRow);
  compressionCtrlsLayout->addWidget(quality_label, 0, Qt::AlignLeft);

  bitrate_title_label = new QLabel(compressionCtrlsWidget);
  compressionCtrlsLayout->addWidget(bitrate_title_label, 0, Qt::AlignLeft);
  auto *bitrateRow = new QWidget(compressionCtrlsWidget);
  auto *bitrateRowLayout = new QHBoxLayout(bitrateRow);
  bitrateRowLayout->setContentsMargins(0, 0, 0, 0);
  bitrateRowLayout->setSpacing(6);
  bitrateRowLayout->addWidget(bitrate_slider);
  bitrateRowLayout->addWidget(bitrate_edit);
  compressionCtrlsLayout->addWidget(bitrateRow);
  compressionCtrlsLayout->addWidget(bitrate_label, 0, Qt::AlignLeft);

  compressionCtrlsLayout->addWidget(mode_combo);

  // Group: Color Compression (quality, bitrate, mode dropdown)
  gamma_group = new QGroupBox(tab2);
  auto *gammaLayout = new QVBoxLayout(gamma_group);
  gammaLayout->setContentsMargins(6, 6, 6, 6);
  gamma_group->setLayout(gammaLayout);

  // Gamma slider (0.1 - 1.0)
  gamma_slider = new QSlider(Qt::Horizontal, gamma_group);
  gamma_slider->setRange(1, 10);
  gamma_slider->setValue(10);
  gamma_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  gamma_slider->setToolTip("Drag slider to adjust gamma");
  gamma_edit = new QLineEdit(gamma_group);
  gamma_edit->setValidator(new QDoubleValidator(0.1, 1.0, 1, gamma_edit));
  gamma_edit->setMaximumWidth(64);
  gamma_edit->setToolTip("Enter a new gamma value here");
  gamma_label = new QLabel(gamma_group);
  gamma_label->setMaximumWidth(40);
  gamma_label->setMinimumWidth(40);
  connect(gamma_slider, QOverload<int>::of(&QSlider::valueChanged), this,
          [this](int val) {
            gamma_label->setText(QString::number(val / 10.0, 'f', 1));
            if (gamma_edit)
              gamma_edit->setText(QString::number(val / 10.0, 'f', 1));
          });
  connect(gamma_slider, &QAbstractSlider::sliderReleased, this,
          [this]() { onSetGamma(); });
  connect(gamma_edit, &QLineEdit::textEdited, this,
          [this](const QString &text) {
            bool ok = false;
            const double v = text.toDouble(&ok);
            if (!ok)
              return;
            gamma_slider->setValue(
                static_cast<int>(qBound(0.1, v, 1.0) * 10.0 + 0.5));
            onSetGamma();
          });
  connect(gamma_edit, &QLineEdit::editingFinished, this, [this]() {
    bool ok = false;
    if (!gamma_edit->text().toDouble(&ok) && !ok)
      gamma_edit->setText(
          QString::number(gamma_slider->value() / 10.0, 'f', 1));
  });

  auto *gammaCtrlsWidget = new QWidget(gamma_group);
  auto *gammaCtrlsLayout = new QVBoxLayout(gammaCtrlsWidget);
  gammaCtrlsLayout->setContentsMargins(0, 0, 0, 0);
  gammaCtrlsLayout->setSpacing(8);
  gamma_title_label = new QLabel(gammaCtrlsWidget);
  gammaCtrlsLayout->addWidget(gamma_title_label, 0, Qt::AlignLeft);
  auto *gammaRow = new QWidget(gammaCtrlsWidget);
  auto *gammaRowLayout = new QHBoxLayout(gammaRow);
  gammaRowLayout->setContentsMargins(0, 0, 0, 0);
  gammaRowLayout->setSpacing(6);
  gammaRowLayout->addWidget(gamma_slider);
  gammaRowLayout->addWidget(gamma_edit);
  gammaCtrlsLayout->addWidget(gammaRow);
  gammaCtrlsLayout->addWidget(gamma_label, 0, Qt::AlignLeft);

  compLayout->addWidget(compressionCtrlsWidget);
  gammaLayout->addWidget(gammaCtrlsWidget);

  rightTabWidget->addTab(scrollArea2, QString());
  v2->addWidget(compression_group);
  v2->addWidget(gamma_group);
  v2->addStretch();

  /*
   ********** Tab for statistics graphs with MetricController integration
   * ***************
   */

  auto *tabStats = new QWidget;
  auto *scrollAreaStats = new QScrollArea;
  scrollAreaStats->setWidget(tabStats);
  scrollAreaStats->setWidgetResizable(true);
  scrollAreaStats->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollAreaStats->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  auto *vStats = new QVBoxLayout(tabStats);

  // Create Focus Metrics with controller
  QVector<QString> focusLabels = {"FocusQuality"};
  QVector<QString> focusDescriptions = {""};
  QVector<bool> focusGraphs = {true};
  focus_metrics_widgets = createMetricWidgets(QString(), QString(), focusLabels,
                                              focusDescriptions, focusGraphs);
  focus_metrics_widgets->passingThreshold = 0.65;
  focusMetricsController = new MetricController(focus_metrics_widgets);
  focusMetricsController->addMetricWidgets(general_focus_metrics_widgets);
  vStats->addWidget(focus_metrics_widgets->groupBox);

  // Create Lens Metrics with controller
  QVector<QString> lensLabels = {"LensHealth"};
  QVector<QString> lensDescriptions = {""};
  QVector<bool> lensGraphs = {true};
  lens_metrics_widgets = createMetricWidgets(QString(), QString(), lensLabels,
                                             lensDescriptions, lensGraphs);
  lens_metrics_widgets->passingThreshold = 0.90;
  lensMetricsController = new MetricController(lens_metrics_widgets);
  lensMetricsController->addMetricWidgets(general_lens_metrics_widgets);
  vStats->addWidget(lens_metrics_widgets->groupBox);
  vStats->addStretch();

  rightTabWidget->addTab(scrollAreaStats, "Statistics");

  /*
   *************** Tab for Exporter ***************
   */

  auto *tabExpo = new QWidget;
  auto *scrollAreaExpo = new QScrollArea;
  scrollAreaExpo->setWidget(tabExpo);
  scrollAreaExpo->setWidgetResizable(true);
  scrollAreaExpo->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollAreaExpo->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  auto *vExpo = new QVBoxLayout(tabExpo);

  exporter_group = new QGroupBox(tabExpo);
  auto *exporterLayout = new QVBoxLayout(exporter_group);
  exporterLayout->setContentsMargins(6, 6, 6, 6);
  exporter_group->setLayout(exporterLayout);

  // Serial number input
  serial_input = new QLineEdit(exporter_group);
  serial_input->setToolTip(
      "Enter the currently installed lens serial number here");
  connect(serial_input, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            metrics_manager.setLensSerial(text.toStdString());
          });
  exporterLayout->addWidget(serial_input);

  // Browse button for screenshot directory
  auto *browseDirLayout = new QHBoxLayout();
  browse_label = new QLabel(exporter_group);
  browse_label->setWordWrap(true);
  browse_button = new QPushButton(exporter_group);
  browse_button->setProperty("secondary", true);
  browse_button->setToolTip("Click to select a destination folder for export");
  connect(browse_button, &QPushButton::clicked, this, [this]() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Screenshot Directory"), screenshotDirectory);
    if (!dir.isEmpty()) {
      screenshotDirectory = dir;
      browse_label->setText(tr("Screenshot Dir: %1").arg(screenshotDirectory));
    }
  });
  browseDirLayout->addWidget(browse_label);
  browseDirLayout->addWidget(browse_button);
  browseDirLayout->addStretch();
  exporterLayout->addLayout(browseDirLayout);

  // Screenshot button
  screenshot_button = new QPushButton(exporter_group);
  screenshot_button->setProperty("secondary", true);
  screenshot_button->setToolTip("Click to capture screenshot of window");
  connect(screenshot_button, &QPushButton::clicked, this,
          [this]() { takeScreenshot(); });
  exporterLayout->addWidget(screenshot_button);

  screenshot_status_label = new QLabel(exporter_group);
  screenshot_status_label->setWordWrap(true);
  screenshot_status_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  screenshot_status_label->setMinimumHeight(40);
  screenshot_status_label->setVisible(false);
  exporterLayout->addWidget(screenshot_status_label);

  // Export metrics button
  metrics_export_button = new QPushButton(exporter_group);
  metrics_export_button->setProperty("secondary", true);
  metrics_export_button->setToolTip("Click to export current lens metrics");
  connect(metrics_export_button, &QPushButton::clicked, this,
          [this]() { emit exportMetricsRequested(); });
  exporterLayout->addWidget(metrics_export_button);

  // Overlay enable/disable checkbox
  overlay_button = new QCheckBox(exporter_group);
  overlay_button->setChecked(true);
  overlay_button->setProperty("secondary", true);
  overlay_button->setToolTip("When checked, the overlay is enabled");
  connect(overlay_button, &QCheckBox::clicked, this, [this]() {
    overlayState = overlay_button->isChecked();
    updateOverlayButtonText();
  });
  exporterLayout->addWidget(overlay_button);

  vExpo->addWidget(exporter_group);
  vExpo->addStretch();

  rightTabWidget->addTab(scrollAreaExpo, QString());
  rightTabWidget->setCurrentIndex(0);

  // Start at 1 to skip the first tab and end at count - 1 to skip last tab (exporter)
  for (int i = 1; i < rightTabWidget->count() - 1; ++i)
    rightTabWidget->setTabVisible(i, false);

  updateMarkerZoomControlsEnabled(false);
  setLensInspectionModeIndex(0);

  root->addWidget(rightTabWidget);

  retranslateUi();
}

MetricWidgets *CameraControlPanel::createMetricWidgets(
    const QString name, const QString units, QVector<QString> labels,
    QVector<QString> descriptions, QVector<bool> graphs) {

  // Create new metricWidgets structure & layout
  MetricWidgets *metricWidgets = new MetricWidgets();
  QVBoxLayout *layout = new QVBoxLayout();

  // Set name & units
  metricWidgets->name = name;
  metricWidgets->units = units;

  // Set groupBox Settings
  metricWidgets->groupBox->setTitle(name);
  metricWidgets->groupBox->setCheckable(true);

  for (int i = 0; i < labels.count(); ++i) {
    QLabel *dataLabel = new QLabel();
    dataLabel->setObjectName(labels[i] + "DataLabel");
    dataLabel->setText("- " + units);
    dataLabel->setAlignment(Qt::AlignRight);
    QFont labelFont = dataLabel->font();
    labelFont.setPointSize(20);
    labelFont.setBold(true);
    dataLabel->setFont(labelFont);

    layout->addWidget(dataLabel);
    metricWidgets->dataLabels.append(dataLabel);

    QString description = descriptions.value(i);
    if (!description.isEmpty()) {
      QLabel *descriptionLabel = new QLabel();
      descriptionLabel->setObjectName(labels[i] + "DescriptionLabel");
      descriptionLabel->setText(description);
      descriptionLabel->setAlignment(Qt::AlignRight);

      layout->addWidget(descriptionLabel);
      metricWidgets->descriptionLabels.append(descriptionLabel);
    }

    if (graphs[i]) {
      GraphWidget *metricGraph = new GraphWidget(metricWidgets->groupBox);
      metricGraph->setObjectName(labels[i] + "Graph");
      metricGraph->setMinimumHeight(100);

      layout->addWidget(metricGraph);
      metricWidgets->metricGraphs.append(metricGraph);
    } else {
      metricWidgets->metricGraphs.append(nullptr);
    }
  }

  metricWidgets->groupBox->setLayout(layout);

  return metricWidgets;
}

MetricWidgets *CameraControlPanel::createCompactMetricWidgets(
    const QString &name, const QString &units, const QVector<QString> &labels,
    const QVector<QString> &descriptions, const QVector<bool> &graphs,
    int graphHeight) {

  MetricWidgets *metricWidgets = new MetricWidgets();
  auto *layout = new QVBoxLayout(metricWidgets->groupBox);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  metricWidgets->name = name;
  metricWidgets->units = units;
  metricWidgets->groupBox->setTitle(name);

  for (int i = 0; i < labels.count(); ++i) {
    auto *rowWidget = new QWidget(metricWidgets->groupBox);
    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(14);

    GraphWidget *metricGraph = nullptr;
    if (graphs.value(i)) {
      metricGraph = new GraphWidget(rowWidget);
      metricGraph->setObjectName(labels[i] + "Graph");
      metricGraph->setMinimumHeight(graphHeight);
      metricGraph->setMaximumHeight(graphHeight);
      metricGraph->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    QLabel *dataLabel = new QLabel(rowWidget);
    dataLabel->setObjectName(labels[i] + "DataLabel");
    dataLabel->setText(QStringLiteral("-"));
    dataLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    dataLabel->setMinimumHeight(graphHeight);
    dataLabel->setMaximumHeight(graphHeight);
    dataLabel->setFixedWidth(qMax(170, graphHeight * 2));
    dataLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QFont labelFont = dataLabel->font();
    const int labelFontPx = qMax(54, graphHeight - 10);
    labelFont.setPixelSize(labelFontPx);
    labelFont.setBold(true);
    dataLabel->setFont(labelFont);
    dataLabel->setProperty("metricFontSizePx", labelFontPx);
    dataLabel->setStyleSheet("color: #ddd; font-weight: 700;");
    rowLayout->addWidget(dataLabel);

    if (metricGraph) {
      rowLayout->addWidget(metricGraph, 1);
    } else {
      rowLayout->addStretch(1);
    }

    layout->addWidget(rowWidget);
    metricWidgets->dataLabels.append(dataLabel);
    metricWidgets->metricGraphs.append(metricGraph);

    const QString description = descriptions.value(i);
    if (!description.isEmpty()) {
      QLabel *descriptionLabel =
          new QLabel(description, metricWidgets->groupBox);
      descriptionLabel->setObjectName(labels[i] + "DescriptionLabel");
      descriptionLabel->setAlignment(Qt::AlignRight);
      layout->addWidget(descriptionLabel);
      metricWidgets->descriptionLabels.append(descriptionLabel);
    }
  }

  return metricWidgets;
}

void CameraControlPanel::setExportLanguage(
    MetricsManager::OutputLanguage lang) {
  metrics_manager.setLanguage(lang);
}

void CameraControlPanel::updateFocusButtonText() {
  if (!focus_button)
    return;
  focus_button->setText(focusState ? tr("Focus Enabled")
                                   : tr("Focus Disabled"));
}

void CameraControlPanel::updateFocusHudButtonText() {
  if (!focusHUD_button)
    return;
  focusHUD_button->setText(focusHUDState ? tr("Focus HUD Enabled")
                                         : tr("Focus HUD Disabled"));
}

void CameraControlPanel::updateOverlayButtonText() {
  if (!overlay_button)
    return;
  overlay_button->setText(overlayState ? tr("Overlay Enabled")
                                       : tr("Overlay Disabled"));
}

void CameraControlPanel::updateSliderLabels() {
  if (exposure_slider && exposure_label) {
    exposure_label->setText(QString::number(exposure_slider->value()) + " " +
                            exposure_unit_ms);
    if (exposure_edit)
      exposure_edit->setText(QString::number(exposure_slider->value()));
    updateGeneralExposureLabel(exposure_slider->value());
  }
  if (fps_slider && fps_label) {
    fps_label->setText(QString::number(fps_slider->value()) + " " + fps_unit);
    if (fps_edit)
      fps_edit->setText(QString::number(fps_slider->value()));
  }
  if (gain_slider && gain_label) {
    gain_label->setText(QString::number(gain_slider->value()) + " " +
                        gain_unit_db);
    if (gain_edit)
      gain_edit->setText(QString::number(gain_slider->value()));
  }
}

void CameraControlPanel::updateGeneralExposureLabel(int value) {
  if (!general_exposure_label) {
    return;
  }
  const QString units =
      exposure_unit_ms.isEmpty() ? QStringLiteral("ms") : exposure_unit_ms;
  general_exposure_label->setText(
      QStringLiteral("%1 %2").arg(value).arg(units));
}

void CameraControlPanel::updateGeneralZoomLabel(int value) {
  if (!general_zoom_label) {
    return;
  }
  general_zoom_label->setText(QString::number(value / 10.0f, 'f', 1) +
                              QStringLiteral("x"));
}

void CameraControlPanel::updateMarkerZoomControlsEnabled(bool enabled) {
  if (lens_inspection_mode_combo) {
    lens_inspection_mode_combo->setEnabled(enabled);
  }
  if (zoom_button) {
    zoom_button->setEnabled(enabled);
  }
  if (zoom_slider) {
    zoom_slider->setEnabled(enabled);
  }
  if (general_lens_inspection_mode_combo) {
    general_lens_inspection_mode_combo->setEnabled(enabled);
  }
  if (general_zoom_slider) {
    general_zoom_slider->setEnabled(enabled);
  }
}

void CameraControlPanel::setLensInspectionModeIndex(int index) {
  if (lens_inspection_mode_combo) {
    const QSignalBlocker blocker(lens_inspection_mode_combo);
    lens_inspection_mode_combo->setCurrentIndex(index);
  }
  if (general_lens_inspection_mode_combo) {
    const QSignalBlocker blocker(general_lens_inspection_mode_combo);
    general_lens_inspection_mode_combo->setCurrentIndex(index);
  }
}

void CameraControlPanel::applyLensInspectionModeSelection(int index) {
  if (!currentSerialValid()) {
    emit showWarning(tr("No Camera"), tr("No camera is currently selected."));
    setLensInspectionModeIndex(0);
    return;
  }
  if (!isMarkerZoomPossible()) {
    emit showWarning(
        tr("Wrong Video Mode"),
        tr("Must turn on Grayscale Mode to use the zoom feature."));
    setLensInspectionModeIndex(0);
    return;
  }

  setLensInspectionModeIndex(index);
  bool markerZoomToggled = false;
  if (lens_inspection_mode_combo) {
    markerZoomToggled = lens_inspection_mode_combo->itemData(index).toBool();
  }
  qDebug("[dbg] markerZoomToggled = %d", markerZoomToggled);
  emit onMarkerZoomToggled(markerZoomToggled);
}

static bool videoModeDataEqual(const QVariant &a, const QVariant &b) {
  if (a.canConvert<QVariantList>() && b.canConvert<QVariantList>()) {
    const QVariantList al = a.toList();
    const QVariantList bl = b.toList();
    return al.size() == bl.size() && al.value(0) == bl.value(0) &&
           al.value(1) == bl.value(1);
  }
  return a == b;
}

void CameraControlPanel::repopulateVideoModes() {
  if (!video_mode_combo)
    return;
  const QVariant currentData = video_mode_combo->currentData();
  video_mode_combo->blockSignals(true);
  video_mode_combo->clear();

  video_mode_combo->addItem(tr("Segment"),
                            QVariant(static_cast<int>(Core::SegmentMode)));
  video_mode_combo->setItemData(video_mode_combo->count() - 1,
                                tr("5-segment view (center+corners)"),
                                Qt::ToolTipRole);

  video_mode_combo->addItem(
      tr("Grayscale"),
      QVariantList{static_cast<int>(Core::GrayscaleMode), false});
  video_mode_combo->setItemData(video_mode_combo->count() - 1,
                                tr("8bpp camera preview"), Qt::ToolTipRole);

  video_mode_combo->addItem(tr("Object"),
                            QVariant(static_cast<int>(Core::ObjectMode)));
  video_mode_combo->setItemData(video_mode_combo->count() - 1,
                                tr("Object mode: runs detection pipeline"),
                                Qt::ToolTipRole);
  video_mode_combo->addItem(tr("Precision"),
                            QVariant(static_cast<int>(Core::PrecisionMode)));
  video_mode_combo->setItemData(video_mode_combo->count() - 1,
                                tr("Precision view: tighter quality metrics"),
                                Qt::ToolTipRole);
  video_mode_combo->addItem(tr("MJPEG"),
                            QVariant(static_cast<int>(Core::MJPEGMode)));
  video_mode_combo->setItemData(video_mode_combo->count() - 1,
                                tr("MJPEG streaming mode"), Qt::ToolTipRole);
  video_mode_combo->addItem(tr("Duplex"),
                            QVariant(static_cast<int>(Core::DuplexMode)));
  video_mode_combo->setItemData(video_mode_combo->count() - 1,
                                tr("Duplex: two-stream capture"),
                                Qt::ToolTipRole);

  int targetIdx = -1;
  for (int i = 0; i < video_mode_combo->count(); ++i) {
    if (videoModeDataEqual(video_mode_combo->itemData(i), currentData)) {
      targetIdx = i;
      break;
    }
  }
  if (targetIdx < 0 && video_mode_combo->count() > 0) {
    targetIdx = 0;
  }
  video_mode_combo->setCurrentIndex(targetIdx);
  video_mode_combo->blockSignals(false);
}

void CameraControlPanel::repopulateLensInspectionModes() {
  auto populateLensInspectionCombo = [this](QComboBox *lensInspectionCombo) {
    if (!lensInspectionCombo) {
      return;
    }

    const QVariant currentData = lensInspectionCombo->currentData();
    lensInspectionCombo->blockSignals(true);
    lensInspectionCombo->clear();

    lensInspectionCombo->addItem(tr("No Zoom"), QVariant(false));
    lensInspectionCombo->setItemData(lensInspectionCombo->count() - 1,
                                     tr("8bpp camera preview"),
                                     Qt::ToolTipRole);

    lensInspectionCombo->addItem(tr("ROI Zoom"), QVariant(true));
    lensInspectionCombo->setItemData(
        lensInspectionCombo->count() - 1,
        tr("8bpp camera preview with center/edge marker focus"),
        Qt::ToolTipRole);

    int targetIdx = -1;
    for (int i = 0; i < lensInspectionCombo->count(); ++i) {
      if (videoModeDataEqual(lensInspectionCombo->itemData(i), currentData)) {
        targetIdx = i;
        break;
      }
    }
    if (targetIdx < 0 && lensInspectionCombo->count() > 0) {
      targetIdx = 0;
    }
    lensInspectionCombo->setCurrentIndex(targetIdx);
    lensInspectionCombo->blockSignals(false);
  };

  populateLensInspectionCombo(lens_inspection_mode_combo);
  populateLensInspectionCombo(general_lens_inspection_mode_combo);
}

void CameraControlPanel::repopulateCompressionModes() {
  if (!mode_combo)
    return;
  const QVariant currentData = mode_combo->currentData();
  mode_combo->blockSignals(true);
  mode_combo->clear();

  mode_combo->addItem(tr("Variable Bitrate"), QVariant(0));
  mode_combo->addItem(tr("Constant Bitrate"), QVariant(1));

  int targetIdx = -1;
  for (int i = 0; i < mode_combo->count(); ++i) {
    if (mode_combo->itemData(i) == currentData) {
      targetIdx = i;
      break;
    }
  }
  if (targetIdx < 0 && mode_combo->count() > 0) {
    targetIdx = 0;
  }
  mode_combo->setCurrentIndex(targetIdx);
  mode_combo->blockSignals(false);
}

void CameraControlPanel::retranslateUi() {
  if (cam_group)
    cam_group->setTitle(tr("General Camera Controls"));
  if (focus_tool_group)
    focus_tool_group->setTitle(tr("Focus Tool"));
  if (video_group)
    video_group->setTitle(tr("Video Mode"));
  if (lens_inspection_group)
    lens_inspection_group->setTitle(tr("Lens Inspection"));
  if (compression_group)
    compression_group->setTitle(tr("Color Compression"));
  if (gamma_group)
    gamma_group->setTitle(tr("Gamma"));
  if (exporter_group)
    exporter_group->setTitle(tr("Exporter"));

  if (exposure_title_label)
    exposure_title_label->setText(tr("Exposure:"));
  if (general_exposure_title_label)
    general_exposure_title_label->setText(tr("Exposure:"));
  if (fps_title_label)
    fps_title_label->setText(tr("FPS:"));
  if (gain_title_label)
    gain_title_label->setText(tr("Gain:"));
  if (quality_title_label)
    quality_title_label->setText(tr("Quality:"));
  if (bitrate_title_label)
    bitrate_title_label->setText(tr("Bitrate:"));
  if (gamma_title_label)
    gamma_title_label->setText(tr("Gamma:"));

  exposure_unit_ms = tr("ms");
  fps_unit = tr("fps");
  gain_unit_db = tr("dB");
  updateSliderLabels();
  if (quality_slider && quality_label) {
    quality_label->setText(
        QString::number(quality_slider->value() / 100.0, 'f', 2));
  }
  if (quality_slider && quality_edit) {
    quality_edit->setText(
        QString::number(quality_slider->value() / 100.0, 'f', 2));
  }
  if (bitrate_slider && bitrate_label) {
    bitrate_label->setText(
        QString::number(bitrate_slider->value() / 100.0, 'f', 2));
  }
  if (bitrate_slider && bitrate_edit) {
    bitrate_edit->setText(
        QString::number(bitrate_slider->value() / 100.0, 'f', 2));
  }
  if (gamma_slider && gamma_label) {
    gamma_label->setText(QString::number(gamma_slider->value() / 10.0, 'f', 1));
  }
  if (gamma_slider && gamma_edit) {
    gamma_edit->setText(QString::number(gamma_slider->value() / 10.0, 'f', 1));
  }

  updateFocusButtonText();
  updateFocusHudButtonText();
  updateOverlayButtonText();

  if (lens_inspection_mode_label) {
    lens_inspection_mode_label->setText(tr("Mode:"));
  }
  if (general_lens_inspection_mode_label) {
    general_lens_inspection_mode_label->setText(tr("Zoom Mode:"));
  }
  if (zoom_title_label) {
    zoom_title_label->setText(tr("Zoom:"));
  }
  if (general_zoom_title_label) {
    general_zoom_title_label->setText(tr("Zoom:"));
  }
  if (zoom_button) {
    zoom_button->setText(tr("Reset"));
  }
  if (circle_detect_button) {
    circle_detect_button->setText(tr("Enable Circle Detection"));
  }
  if (circle_count_label) {
    circle_count_label->setText(
        tr("Circles Detected: %1").arg(circle_detected_count));
  }
  if (circle_param2_title_label) {
    circle_param2_title_label->setText(tr("Param2 (Threshold):"));
  }
  if (hough_circle_header_label) {
    hough_circle_header_label->setText(
        QStringLiteral("<b>%1</b>").arg(tr("Hough Circle Detection")));
  }

  if (edge_button) {
    edge_button->setText(tr("Edge Detect"));
    edge_button->setToolTip(tr("Enable edge overlay in viewer: Works on "
                               "Grayscale, Precision, and MJPEG modes"));
  }

  repopulateVideoModes();
  repopulateLensInspectionModes();
  repopulateCompressionModes();
  if (video_mode_combo && edge_button) {
    const QVariant itemData = video_mode_combo->currentData();
    int mode = 0;
    if (itemData.canConvert<QVariantList>()) {
      const QVariantList dataList = itemData.toList();
      mode = dataList.value(0).toInt();
    } else {
      mode = itemData.toInt();
    }
    edge_button->setEnabled(isEdgeDetectCompatible(mode));
  }

  if (rightTabWidget) {
    rightTabWidget->setTabText(0, tr("General"));
    rightTabWidget->setTabText(1, tr("Controls"));
    rightTabWidget->setTabText(2, tr("Lens"));
    rightTabWidget->setTabText(3, tr("Color"));
    rightTabWidget->setTabText(4, tr("Statistics"));
    rightTabWidget->setTabText(5, tr("Exporter"));
  }

  if (general_focus_metrics_widgets &&
      general_focus_metrics_widgets->groupBox) {
    general_focus_metrics_widgets->name = tr("Focus Score");
    general_focus_metrics_widgets->groupBox->setTitle(
        general_focus_metrics_widgets->name);
  }
  if (general_lens_metrics_widgets && general_lens_metrics_widgets->groupBox) {
    general_lens_metrics_widgets->name = tr("Lens Score");
    general_lens_metrics_widgets->groupBox->setTitle(
        general_lens_metrics_widgets->name);
  }

  if (focus_metrics_widgets && focus_metrics_widgets->groupBox) {
    focus_metrics_widgets->name = tr("Focus Statistics");
    focus_metrics_widgets->groupBox->setTitle(focus_metrics_widgets->name);
  }
  if (lens_metrics_widgets && lens_metrics_widgets->groupBox) {
    lens_metrics_widgets->name = tr("Lens Statistics");
    lens_metrics_widgets->groupBox->setTitle(lens_metrics_widgets->name);
  }

  if (serial_input)
    serial_input->setPlaceholderText(tr("Serial #"));
  if (browse_label)
    browse_label->setText(tr("Screenshot Dir: %1").arg(screenshotDirectory));
  if (browse_button)
    browse_button->setText(tr("Browse..."));
  if (screenshot_button) {
    screenshot_button->setText(tr("Screenshot"));
    screenshot_button->setToolTip(tr("Take Screenshot"));
  }
  if (screenshot_status_label) {
    screenshot_status_label->clear();
    screenshot_status_label->setVisible(false);
  }
  if (metrics_export_button) {
    metrics_export_button->setText(tr("Export Data"));
    metrics_export_button->setToolTip(
        tr("Click to export information about the currently installed lens."));
  }

  if (general_zoom_slider) {
    updateGeneralZoomLabel(general_zoom_slider->value());
  }
}

void CameraControlPanel::onSetExposure() {
  if (!currentSerialValid()) {
    emit showWarning(tr("No Camera"), tr("No camera is currently selected."));
    return;
  }
  const int v = exposure_slider->value();
  if (!camera_manager->SetExposure(selected_serial, v)) {
    emit showWarning(tr("Failed"),
                     tr("Could not set exposure on the selected camera."));
  }
}

void CameraControlPanel::onSetFps() {
  if (!currentSerialValid()) {
    emit showWarning(tr("No Camera"), tr("No camera is currently selected."));
    return;
  }
  const int v = fps_slider->value();
  if (!camera_manager->SetFrameRate(selected_serial, v)) {
    emit showWarning(tr("Failed"),
                     tr("Could not set frame rate on the selected camera."));
  }
}

void CameraControlPanel::onSetGain() {
  if (!currentSerialValid()) {
    emit showWarning(tr("No Camera"), tr("No camera is currently selected."));
    return;
  }
  const int v = gain_slider->value();
  if (!camera_manager->SetImagerGain(selected_serial, v)) {
    emit showWarning(tr("Failed"),
                     tr("Could not set imager gain on the selected camera."));
  }
}

void CameraControlPanel::onSetZoom(bool reset) {
  if (!currentSerialValid()) {
    emit showWarning("No Camera", "No camera is currently selected.");
    return;
  }
  if (reset) {
    zoom_slider->setValue(10); // 10 tenths = 1.0x
    return; // setValue fires valueChanged which calls onSetZoom(false), so
            // return here
  }
  // Slider stores tenths-of-zoom; convert to the float value VideoWidget
  // expects.
  float zoom = zoom_slider->value() / 10.0f;
  emit zoomValueChanged(zoom);
}

void CameraControlPanel::onSetGamma() {
  if (!currentSerialValid()) {
    emit showWarning(tr("No Camera"), tr("No camera is currently selected."));
    return;
  }
  const float g = gamma_slider->value() / 10.0f;
  if (!camera_manager->SetColorGamma(selected_serial, g)) {
    emit showWarning(
        tr("Unsupported Camera"),
        tr("Color gamma is only supported on Prime Color cameras."));
  }
}

void CameraControlPanel::onSetCompression() {
  if (!currentSerialValid()) {
    emit showWarning(tr("No Camera"), tr("No camera is currently selected."));
    return;
  }
  const float quality = quality_slider->value() / 100.0f;
  const float mbps = bitrate_slider->value() / 100.0f;

  const float bitrateScaled = CameraHelper::MbpsToNormalized(mbps);
  const int mode = mode_combo->currentData().toInt();
  if (!camera_manager->SetColorCompression(selected_serial, mode, quality,
                                           bitrateScaled)) {
    emit showWarning(
        tr("Unsupported Camera"),
        tr("Color compression is only supported on Prime Color cameras."));
  }
}

void CameraControlPanel::onSetVideoMode(int modeEnum) {
  if (!currentSerialValid())
    return;
  QString err;
  if (!camera_manager->SetVideoType(
          selected_serial, static_cast<Core::eVideoMode>(modeEnum), &err)) {
    if (!err.isEmpty())
      emit showWarning(tr("Unsupported Mode"), err);
  }
}

void CameraControlPanel::toggleTabVisibility(int index) {
  if (!rightTabWidget || index < 0 || index >= rightTabWidget->count()) {
    return;
  }

  const bool currentlyVisible = rightTabWidget->isTabVisible(index);
  rightTabWidget->setTabVisible(index, !currentlyVisible);

  bool anyVisible = false;
  for (int i = 0; i < rightTabWidget->count(); ++i) {
    if (rightTabWidget->isTabVisible(i)) {
      anyVisible = true;
      break;
    }
  }
  rightTabWidget->setVisible(anyVisible);
}

void CameraControlPanel::onSetTab0Visibility() { toggleTabVisibility(0); }

void CameraControlPanel::onSetTab1Visibility() { toggleTabVisibility(1); }

void CameraControlPanel::onSetTab2Visibility() { toggleTabVisibility(2); }

void CameraControlPanel::onSetTab3Visibility() { toggleTabVisibility(3); }

void CameraControlPanel::onSetTab4Visibility() { toggleTabVisibility(4); }

void CameraControlPanel::onSetTab5Visibility() { toggleTabVisibility(5); }

bool CameraControlPanel::isEdgeDetectCompatible(int mode) {
  switch (mode) {
  case Core::SegmentMode:
  case Core::ObjectMode:
  case Core::DuplexMode:
    return false;
  default:
    return true;
  }
}

void CameraControlPanel::updateCircleCount(int count) {
  circle_detected_count = count;
  if (circle_count_label) {
    circle_count_label->setText(tr("Circles Detected: %1").arg(count));
  }
}

void CameraControlPanel::onCircleParam2Changed() {
  bool ok;
  double param2 = circle_param2_edit->text().toDouble(&ok);
  if (ok && param2 >= 5.0 && param2 <= 100.0) {
    emit circleParam2Changed(param2);
  }
}

void CameraControlPanel::takeScreenshot() {
  // Check if loaded screen
  QScreen *screen = QGuiApplication::primaryScreen();
  if (!screen) {
    emit showWarning(tr("Screenshot"), tr("No screen is currently available."));
    return;
  }
  // Add Serial number of lens if possible, else put #
  QString serial = serial_input && !serial_input->text().isEmpty()
                       ? serial_input->text()
                       : "#";
  // Get the window image
  QPixmap pix;
  if (overlayState)
    // Capture the entire top-level window (the whole application)
    pix = screen->grabWindow(this->window()->winId());
  else
    // Capture just the video widget if overlay is disabled
    pix = screen->grabWindow(gl_viewer_window->winId());
  // Assign the time and day, with the serial number for file name
  QString filename = QDateTime::currentDateTime()
                         .toString("'screenshot_%1_'yyyyMMdd_HHmmss'.png'")
                         .arg(serial);
  QString fileLocation = screenshotDirectory.isEmpty()
                             ? filename
                             : QDir(screenshotDirectory).filePath(filename);

  if (!pix.save(fileLocation)) {
    emit showWarning(tr("Screenshot"), tr("Failed to save screenshot."));
    return;
  }

  if (screenshot_status_label) {
    screenshot_status_label->setStyleSheet("color: #1f8f3a;");
    screenshot_status_label->setText(
        tr("Screenshot Saved: %1").arg(QFileInfo(fileLocation).fileName()));
    screenshot_status_label->setVisible(true);
  }
}
