#include "QtCameraViewer.h"

#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QStackedLayout>
#include <QStyleFactory>
#include <QTimer>
#include <QVBoxLayout>

#include "./widgets/graphwidget.h"
#include "CameraHelpers.h"
#include "MetricsManager.h"
#include "QtCameraConnectionManager.h"
#include "QtCameraControlPanel.h"
#include "QtCameraPicker.h"
#include "QtVideoWidget.h"

// Main Collection of Widgets and layouts for the application

using namespace CameraLibrary;

void QtCameraViewer::ApplyAppStyle() {
  QApplication::setStyle(QStyleFactory::create("Fusion"));
  QPalette dark;
  dark.setColor(QPalette::Window, QColor(53, 53, 53));
  dark.setColor(QPalette::WindowText, Qt::white);
  dark.setColor(QPalette::Base, QColor(35, 35, 35));
  dark.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
  dark.setColor(QPalette::ToolTipBase, Qt::white);
  dark.setColor(QPalette::ToolTipText, Qt::white);
  dark.setColor(QPalette::Text, Qt::white);
  dark.setColor(QPalette::Button, QColor(53, 53, 53));
  dark.setColor(QPalette::ButtonText, Qt::white);
  dark.setColor(QPalette::BrightText, Qt::red);
  dark.setColor(QPalette::Link, QColor(42, 130, 218));
  dark.setColor(QPalette::Highlight, QColor(42, 130, 218));
  dark.setColor(QPalette::HighlightedText, Qt::black);
  dark.setColor(QPalette::PlaceholderText, QColor(160, 160, 160));
  QApplication::setPalette(dark);
}

QtCameraViewer::QtCameraViewer(
    CameraConnectionManager *mgr, std::mutex &camMutex,
    std::shared_ptr<Camera> &currentCamera, std::atomic<uint64_t> &switchEpoch,
    std::atomic<unsigned> &activeSerial,
    FocusResultLabel *focusResult, FocusScoreLabel *focusScore, 
    LensResultLabel *lensResult, MetricsManager &MetricsManager,
    QWidget *parent)
    : QWidget(parent), camera_manager(mgr), camera_mutex(camMutex),
      current_camera(currentCamera), switch_epoch(switchEpoch),
      active_serial(activeSerial), focus_result(focusResult),
      focus_score(focusScore), lens_result(lensResult),
      metrics_manager(MetricsManager) {
  buildUi();
  wireSignals();
}

void QtCameraViewer::buildUi() {
  auto *mainLayout =
      new QVBoxLayout(this);    // 'this' places layout onto the main widget
  auto *v = new QVBoxLayout();  // no parent
  auto *h2 = new QHBoxLayout(); // no parent

  // Row 1: Camera and language pickers (go above everything else)
  status_bar = new QWidget(this);
  auto box = new QHBoxLayout(status_bar);
  box->setContentsMargins(6, 0, 6, 0);

  camera_picker = new CameraPicker(camera_manager, status_bar);
  box->addWidget(camera_picker);

  language_label = new QLabel(status_bar);
  language_combo = new QComboBox(status_bar);
  language_combo->addItem(QStringLiteral("English"), QStringLiteral("en"));
  language_combo->addItem(QStringLiteral("Simplified Chinese"),
                          QStringLiteral("zh_CN"));
  connect(language_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),

          this, [this](int idx) {
            if (!language_combo || idx < 0)
              return;

            emit languageChanged(language_combo->itemData(idx).toString());
          });

  box->addWidget(language_label);
  box->addWidget(language_combo);

  mainLayout->addWidget(status_bar);


  // Controls panel that later comes in Row 5
  camera_controls =
      new CameraControlPanel(camera_manager, metrics_manager, this);

  // Row 2: Status bar with focus eval result and lens grade
  focus_bar = new QWidget(this);
  auto *sh = new QHBoxLayout(focus_bar);
  sh->setContentsMargins(6, 0, 6, 0);

  // set font for using horizontalAdvance()
  // font itself based on what's defined in motive.css
  QFont focus_font("Lato", 12);
  fm = new QFontMetricsF(focus_font);

  focus_result_label = new QLabel("Focus Result:", focus_bar);
  focus_result_label->setStyleSheet("color:#ddd; font-weight:600;");

  focus_score_label = new QLabel("Focus Score:", focus_bar);
  focus_score_label->setStyleSheet("color:#ddd; font-weight:600;");

  lens_result_label = new QLabel("Lens Grade:", focus_bar);
  lens_result_label->setStyleSheet("color:#ddd; font-weight:600;");

  focus_result->setStyleSheet("color:CadetBlue; font-weight:600;");
  double focus_result_width = fm->horizontalAdvance(focus_result->wide_angle_success);
  focus_result->setMinimumWidth(focus_result_width);
  focus_result->setMaximumWidth(focus_result_width);
  focus_result->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  focus_score->setStyleSheet("color:CadetBlue; font-weight:600;");
  focus_score->setMinimumWidth(200);
  focus_score->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

  lens_result->setStyleSheet("color:CadetBlue; font-weight:600;");
  lens_result->setMinimumWidth(70);
  lens_result->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  sh->addWidget(focus_result_label);
  sh->addWidget(focus_result);
  sh->addWidget(focus_score_label);
  sh->addWidget(focus_score);
  sh->addWidget(lens_result_label);
  sh->addWidget(lens_result);
  // sh->addStretch(1);
  
  v->addWidget(focus_bar);

  // change visibility of focus tool HUD
  connect(camera_controls, &CameraControlPanel::focusHUDToggled, this,
          &QtCameraViewer::onSetFocusHUDVisibility);

  // Row 5: Contains toggle buttons for the tabs' visibility
  toggle_tabs_bar = new QWidget(this);
  auto *toggle_tabs_box = new QHBoxLayout(toggle_tabs_bar);
  toggle_tabs_box->setContentsMargins(6, 0, 6, 0);
  toggle_label = new QLabel("Tabs:", toggle_tabs_bar);
  const QString tabToggleStyle =
      "QPushButton:checked { color: cyan; border-color: cyan; } ";

  tab0_visibility_button = new QPushButton("General", toggle_tabs_bar);
  tab0_visibility_button->setCheckable(true);
  tab0_visibility_button->setChecked(true);
  tab0_visibility_button->setStyleSheet(tabToggleStyle);
  connect(tab0_visibility_button, &QPushButton::clicked, camera_controls,
          &CameraControlPanel::onSetTab0Visibility);

  tab1_visibility_button = new QPushButton("Controls", toggle_tabs_bar);
  tab1_visibility_button->setCheckable(true);
  tab1_visibility_button->setChecked(false);
  tab1_visibility_button->setStyleSheet(tabToggleStyle);
  connect(tab1_visibility_button, &QPushButton::clicked, camera_controls,
          &CameraControlPanel::onSetTab1Visibility);

  tab2_visibility_button = new QPushButton("Lens", toggle_tabs_bar);
  tab2_visibility_button->setCheckable(true);
  tab2_visibility_button->setChecked(false);
  tab2_visibility_button->setStyleSheet(tabToggleStyle);
  connect(tab2_visibility_button, &QPushButton::clicked, camera_controls,
          &CameraControlPanel::onSetTab2Visibility);

  tab3_visibility_button = new QPushButton("Color", toggle_tabs_bar);
  tab3_visibility_button->setCheckable(true);
  tab3_visibility_button->setChecked(false);
  tab3_visibility_button->setStyleSheet(tabToggleStyle);
  connect(tab3_visibility_button, &QPushButton::clicked, camera_controls,
          &CameraControlPanel::onSetTab3Visibility);

  tab4_visibility_button = new QPushButton("Statistics", toggle_tabs_bar);
  tab4_visibility_button->setCheckable(true);
  tab4_visibility_button->setChecked(false);
  tab4_visibility_button->setStyleSheet(tabToggleStyle);
  connect(tab4_visibility_button, &QPushButton::clicked, camera_controls,
          &CameraControlPanel::onSetTab4Visibility);

  tab5_visibility_button = new QPushButton("Exporter", toggle_tabs_bar);
  tab5_visibility_button->setCheckable(true);
  tab5_visibility_button->setChecked(true);
  tab5_visibility_button->setStyleSheet(tabToggleStyle);
  connect(tab5_visibility_button, &QPushButton::clicked, camera_controls,
          &CameraControlPanel::onSetTab5Visibility);

  toggle_tabs_box->addWidget(toggle_label);
  toggle_tabs_box->addWidget(tab0_visibility_button);
  toggle_tabs_box->addWidget(tab1_visibility_button);
  toggle_tabs_box->addWidget(tab2_visibility_button);
  toggle_tabs_box->addWidget(tab3_visibility_button);
  toggle_tabs_box->addWidget(tab4_visibility_button);
  toggle_tabs_box->addWidget(tab5_visibility_button);
  toggle_tabs_box->addStretch(1);

  v->addWidget(toggle_tabs_bar);

  // only add camera_controls after all of the other things (camera picker,
  // etc.)

  // Center stacked layout
  center_widget = new QWidget(this);
  stacked_layout = new QStackedLayout(center_widget);

  // Empty pane
  empty_pane = new QWidget(center_widget);
  auto *emptyLayout = new QVBoxLayout(empty_pane);
  emptyLayout->setAlignment(Qt::AlignCenter);
  empty_label = new QLabel(empty_pane);
  QFont f = empty_label->font();
  f.setPointSize(f.pointSize() + 6);
  f.setBold(true);
  empty_label->setFont(f);
  empty_label->setAlignment(Qt::AlignCenter);
  emptyLayout->addWidget(empty_label);

  // Video pane
  gl_viewer_window = new VideoWidget();
  gl_viewer_window->setNewZoomValue(2.f);

  viewer_container =
      QWidget::createWindowContainer(gl_viewer_window, center_widget);
  viewer_container->setFocusPolicy(Qt::StrongFocus);

  // Set the video widget in the control panel so it can capture it for
  // screenshots
  camera_controls->setVideoWidget(gl_viewer_window);

  stacked_layout->addWidget(empty_pane);
  stacked_layout->addWidget(viewer_container);
  setEmptyState(camera_picker->combo() && camera_picker->combo()->count() > 0);

  // video pane on left, camera controls and stats on right
  v->addWidget(center_widget);
  h2->addLayout(v, 20);
  h2->addWidget(camera_controls);

  mainLayout->addLayout(h2);

  retranslateUi();
}

void QtCameraViewer::wireSignals() {
  // Empty-state follows camera presence
  connect(camera_picker, &CameraPicker::camerasPresentChanged, this,
          &QtCameraViewer::setEmptyState);

  // Selection changes update shared state and control panel
  connect(camera_picker, &CameraPicker::serialChanged, this,
          &QtCameraViewer::handleSerialSelected);

  // Update MetricsManager with the active camera's resolution on selection
  // change
  connect(camera_picker, &CameraPicker::serialChanged, this,
          [this](std::optional<unsigned> serialOpt) {
            if (!serialOpt)
              return;
            auto res = camera_picker->getResolutionBySerial(
                static_cast<qulonglong>(*serialOpt));
            if (res)
              metrics_manager.setActiveResolution(*res);
          });

  // Forward Edge Detect toggle from control panel to the video widget
  if (camera_controls) {
    connect(camera_controls, &CameraControlPanel::edgeDetectToggled, this,
            [this](bool enabled) {
              if (gl_viewer_window)
                gl_viewer_window->setEdgeDetectEnabled(enabled);
            });
    connect(camera_controls, &CameraControlPanel::onMarkerZoomToggled, this,
            [this](bool enabled) {
              if (gl_viewer_window)
                gl_viewer_window->setRoiZoomEnabled(enabled);
            });

  }
}

void QtCameraViewer::setEmptyState(bool anyCamerasPresent) {
  stacked_layout->setCurrentWidget(anyCamerasPresent ? viewer_container
                                                     : empty_pane);
}

void QtCameraViewer::handleSerialSelected(std::optional<unsigned> serialOpt) {
  if (!serialOpt) {
    std::lock_guard<std::mutex> lk(camera_mutex);
    current_camera.reset();
    camera_controls->setSelectedSerial(0);
    setEmptyState(false);
    return;
  }

  const auto serial = static_cast<qulonglong>(*serialOpt);
  active_serial.store(static_cast<unsigned>(serial), std::memory_order_release);
  switch_epoch.fetch_add(1, std::memory_order_acq_rel);

  auto cams = camera_manager->GetCameras();
  for (auto &c : cams) {
    if (static_cast<qulonglong>(c->Serial()) == serial) {
      c->SetTextOverlay(false);
      {
        std::lock_guard<std::mutex> lk(camera_mutex);
        current_camera = c;
        active_serial.store(c->Serial(), std::memory_order_release);
      }

#ifdef HAVE_FFMPEG
      const QString camName = QString::fromUtf8(c->Name());
      const bool isColor = camName.startsWith(QStringLiteral("Prime Color"));
      if (isColor) {
        c->AttachModule(new cModuleVideoDecompressorLibav());
        c->SetLateDecompression(true);
        c->SetVideoType(Core::VideoMode);
        c->SetColorCompression(1, 0.4F, 0.30F);
        c->SetExposure(3000);
        c->SetImagerGain(static_cast<eImagerGain>(3));
        switch_epoch.fetch_add(1, std::memory_order_acq_rel);
      }
#endif

      // Tell the controls which serial to drive
      camera_controls->setSelectedSerial(static_cast<unsigned>(serial));
      setEmptyState(true);
      break;
    }
  }
}

void QtCameraViewer::setViewerZoomValue(float val) {
  if (gl_viewer_window) {
    gl_viewer_window->setNewZoomValue(val);
  }
}

void QtCameraViewer::onSetFocusHUDVisibility(bool toggle) {
  this->focus_bar->setVisible(toggle);
}

void QtCameraViewer::retranslateUi() {
  if (focus_result_label) {
    focus_result_label->setText(
        QCoreApplication::translate("QtCameraViewer", "Focus Result:"));
    fr_label_width = fm->horizontalAdvance(focus_result_label->text());
    focus_result_label->setMinimumWidth(fr_label_width);
    focus_result_label->setMaximumWidth(fr_label_width);
  }
  if (focus_score_label) {
    focus_score_label->setText(
        QCoreApplication::translate("QtCameraViewer", "Focus Score:"));
    fs_label_width = fm->horizontalAdvance(focus_score_label->text());
    focus_score_label->setMinimumWidth(fs_label_width);
    focus_score_label->setMaximumWidth(fs_label_width);
  }
  if (lens_result_label) {
    lens_result_label->setText(
        QCoreApplication::translate("QtCameraViewer", "Lens Grade:"));
    lr_label_width = fm->horizontalAdvance(lens_result_label->text());
    lens_result_label->setMinimumWidth(lr_label_width);
    lens_result_label->setMaximumWidth(lr_label_width);
  }
  if (toggle_label) {
    toggle_label->setText(
        QCoreApplication::translate("QtCameraViewer", "Tabs:"));
  }
  if (tab0_visibility_button) {
    tab0_visibility_button->setText(
        QCoreApplication::translate("QtCameraViewer", "General"));
  }
  if (tab1_visibility_button) {
    tab1_visibility_button->setText(
        QCoreApplication::translate("QtCameraViewer", "Controls"));
  }
  if (tab2_visibility_button) {
    tab2_visibility_button->setText(
        QCoreApplication::translate("QtCameraViewer", "Lens"));
  }
  if (tab3_visibility_button) {
    tab3_visibility_button->setText(
        QCoreApplication::translate("QtCameraViewer", "Color"));
  }
  if (tab4_visibility_button) {
    tab4_visibility_button->setText(
        QCoreApplication::translate("QtCameraViewer", "Statistics"));
  }
  if (tab5_visibility_button) {
    tab5_visibility_button->setText(
        QCoreApplication::translate("QtCameraViewer", "Exporter"));
  }
  if (empty_label) {
    empty_label->setText(
        QCoreApplication::translate("QtCameraViewer", "No Cameras Connected"));
  }
  if (language_label) {
    language_label->setText(
        QCoreApplication::translate("QtCameraViewer", "Language:"));
  }
  if (language_combo && language_combo->count() >= 2) {
    language_combo->setItemText(0, QStringLiteral("English"));
    language_combo->setItemText(1, QStringLiteral("Simplified Chinese"));
  }
  if (camera_picker) {
    camera_picker->retranslateUi();
  }
  if (camera_controls) {
    const QString locale = currentLanguage();
    camera_controls->setExportLanguage(locale == QLatin1String("zh_CN")
                                           ? MetricsManager::Chinese
                                           : MetricsManager::English);
    camera_controls->retranslateUi();
  }
}

QString QtCameraViewer::currentLanguage() const {
  if (!language_combo || language_combo->currentIndex() < 0) {
    return QStringLiteral("en");
  }
  return language_combo->itemData(language_combo->currentIndex()).toString();
}
