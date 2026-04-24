#pragma once

#include "FocusResultLabel.h"
#include "FocusScoreLabel.h"
#include "LensResultLabel.h"
#include "MetricsManager.h"
#include <QCheckBox>
#include <QDir>
#include <QLineEdit>
#include <QString>
#include <QWidget>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>

#ifdef HAVE_FFMPEG
#include "videodecoder.h"
#endif

// Forward Declarations
class QLabel;
class QStackedLayout;
class CameraPicker;
class CameraControlPanel;
class VideoWidget;
class QComboBox;
class QPushButton;
class CameraConnectionManager;
namespace CameraLibrary {
class Camera;
}
namespace CameraHelper {
class FrameRateCalculator;
}

class QtCameraViewer : public QWidget {
  Q_OBJECT
public:
  QtCameraViewer(CameraConnectionManager *mgr, std::mutex &camMutex,
                 std::shared_ptr<CameraLibrary::Camera> &currentCamera,
                 std::atomic<uint64_t> &switchEpoch,
                 std::atomic<unsigned> &activeSerial,
                 FocusResultLabel *focus_result, FocusScoreLabel *focus_score,
                 LensResultLabel *lens_result, MetricsManager &mMgr,
                 QWidget *parent = nullptr);

  static void ApplyAppStyle();

  QWidget *videoContainer() const { return viewer_container; }
  VideoWidget *videoWidget() const { return gl_viewer_window; }
  CameraControlPanel *getControlPanel() const { return camera_controls; }
  // float focus_score{ 0.0f };
  void retranslateUi();
  QString currentLanguage() const;

public slots:
  void setViewerZoomValue(float val);

signals:
  void languageChanged(const QString &locale);

private:
  CameraPicker *camera_picker{nullptr};
  CameraControlPanel *camera_controls{nullptr};
  QWidget *focus_bar{nullptr};
  QWidget *toggle_tabs_bar{nullptr};
  QLabel *toggle_label{nullptr};
  QLabel *empty_label{nullptr};
  QLabel *language_label{nullptr};
  QComboBox *language_combo{nullptr};
  QPushButton *tab0_visibility_button{nullptr};
  QPushButton *tab1_visibility_button{nullptr};
  QPushButton *tab2_visibility_button{nullptr};
  QPushButton *tab3_visibility_button{nullptr};
  QPushButton *tab4_visibility_button{nullptr};
  QPushButton *tab5_visibility_button{nullptr};
  QWidget *center_widget{nullptr};
  QStackedLayout *stacked_layout{nullptr};
  QWidget *empty_pane{nullptr};
  QWidget *viewer_container{nullptr};
  VideoWidget *gl_viewer_window{nullptr};
  QLabel *focus_result_label{nullptr};
  QLabel *lens_result_label{nullptr};
  QLabel *focus_score_label{nullptr};
  QLabel *focus_score_display{nullptr};

  CameraConnectionManager *camera_manager{nullptr};
  std::mutex &camera_mutex;
  std::shared_ptr<CameraLibrary::Camera> &current_camera;
  std::atomic<uint64_t> &switch_epoch;
  std::atomic<unsigned> &active_serial;
  FocusResultLabel *focus_result;
  FocusScoreLabel *focus_score;
  LensResultLabel *lens_result;
  MetricsManager &metrics_manager;

  void buildUi();
  void wireSignals();
  void setEmptyState(bool anyCamerasPresent);
  void handleSerialSelected(std::optional<unsigned> serialOpt);
  void onSetFocusHUDVisibility(bool toggle);

signals:
  void exportMetricsRequested();
};
