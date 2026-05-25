#pragma once
#include "MetricsManager.h"
#include <QCheckBox>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QSlider>
#include <QString>
#include <QTabWidget>
#include <QVector>
#include <QWidget>

class GraphWidget;
class MetricController;
class QtCameraViewer;

#pragma once
#include <QGroupBox>
#include <QString>
#include <QVector>

class QLabel;
class VideoWidget;

struct MetricWidgets {
  QString name;
  QString units;
  qreal passingThreshold{1.0};
  QGroupBox *groupBox{nullptr};
  QVector<QLabel *> dataLabels;
  QVector<QLabel *> descriptionLabels;
  QVector<GraphWidget *> metricGraphs;

  // Optional constructor to initialize the group box with a parent
  explicit MetricWidgets(QWidget *parent = nullptr) {
    groupBox = new QGroupBox(parent);
  }
};

class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QSlider;

class CameraConnectionManager;

class CameraControlPanel : public QWidget {
  Q_OBJECT
public:
  explicit CameraControlPanel(CameraConnectionManager *mgr,
                              MetricsManager &mMgr, QWidget *parent = nullptr);
  void setSelectedSerial(unsigned serial);
  void setMarkerZoomPossible(bool tf) { markerZoomPossible = tf; }
  MetricController *getFocusMetricsController() const {
    return focusMetricsController;
  }
  MetricController *getLensMetricsController() const {
    return lensMetricsController;
  }

  /// @brief Update circle detection count display
  void updateCircleCount(int count);
  bool const returnFocusToolState() { return focusState; }
  bool const returnOverlayState() { return overlayState; }
  void setVideoWidget(VideoWidget *widget) { gl_viewer_window = widget; }
  VideoWidget *videoWidget() const { return gl_viewer_window; }
  void retranslateUi();
  void setExportLanguage(MetricsManager::OutputLanguage lang);
signals:
  void showWarning(const QString &title, const QString &message);
  // Toggle circle detection
  void circleDetectionToggled(bool enabled);
  // Update circle detection param2 (accumulator threshold)
  void circleParam2Changed(double param2);
  void edgeDetectToggled(bool enabled);
  void clearLocksRequested();
  void onMarkerZoomPossible(bool enabled);
  void onMarkerZoomToggled(bool enabled);
  void focusToolToggled(bool enabled);
  void resetFocusStats();
  void zoomValueChanged(float val);
  void focusHUDToggled(bool enabled);
  void exportMetricsRequested();
  void worstCircleMarkersNChanged(int n);

private:
  void buildUi();
  bool currentSerialValid() const;
  bool isMarkerZoomPossible() const;
  MetricWidgets *createMetricWidgets(const QString name, const QString units,
                                     QVector<QString> labels,
                                     QVector<QString> descriptions,
                                     QVector<bool> graphs);
  MetricWidgets *
  createCompactMetricWidgets(const QString &name, const QString &units,
                             const QVector<QString> &labels,
                             const QVector<QString> &descriptions,
                             const QVector<bool> &graphs, int graphHeight = 72);
  void updateFocusButtonText();
  void updateFocusHudButtonText();
  void updateOverlayButtonText();
  void updateSliderLabels();
  void updateGeneralExposureLabel(int value);
  void updateGeneralZoomLabel(int value);
  void updateMarkerZoomControlsEnabled(bool enabled);
  void setLensInspectionModeIndex(int index);
  void applyLensInspectionModeSelection(int index);
  void repopulateVideoModes();
  void repopulateLensInspectionModes();
  void repopulateCompressionModes();
  void toggleTabVisibility(int index);

  QPointer<CameraConnectionManager> camera_manager;
  unsigned selected_serial{0};
  bool markerZoomPossible;

  QTabWidget *rightTabWidget{nullptr};
  QGroupBox *cam_group{nullptr};
  QGroupBox *focus_tool_group{nullptr};
  QGroupBox *video_group{nullptr};
  QGroupBox *lens_inspection_group{nullptr};
  QGroupBox *compression_group{nullptr};
  QGroupBox *gamma_group{nullptr};
  QGroupBox *exporter_group{nullptr};
  MetricWidgets *focus_metrics_widgets{nullptr};
  MetricWidgets *lens_metrics_widgets{nullptr};
  MetricWidgets *general_focus_metrics_widgets{nullptr};
  MetricWidgets *general_lens_metrics_widgets{nullptr};

  // Metrics controllers for Statistics tab
  MetricController *focusMetricsController{nullptr};
  MetricController *lensMetricsController{nullptr};

  QSlider *general_exposure_slider{nullptr};
  QLabel *general_exposure_label{nullptr};
  QLabel *general_exposure_title_label{nullptr};
  QLineEdit *general_exposure_edit{nullptr};

  QLineEdit *exposure_edit{nullptr};
  QSlider *exposure_slider{nullptr};
  QLabel *exposure_label{nullptr};
  QLabel *exposure_title_label{nullptr};

  QLineEdit *gain_edit{nullptr};
  QSlider *gain_slider{nullptr};
  QLabel *gain_label{nullptr};
  QLabel *gain_title_label{nullptr};

  QLineEdit *zoom_edit{nullptr};
  QSlider *zoom_slider{nullptr};
  QLabel *zoom_label{nullptr};
  QLabel *zoom_title_label{nullptr};
  QPushButton *zoom_button{nullptr};

  QCheckBox *focus_button{nullptr};
  bool focusState{true};
  QCheckBox *focusHUD_button{nullptr};
  bool focusHUDState{true};

  QLabel *lens_inspection_mode_label{nullptr};
  QComboBox *lens_inspection_mode_combo{nullptr};
  QPushButton* lens_inspection_clear_lock_button{nullptr};
  QLabel *general_lens_inspection_mode_label{nullptr};
  QComboBox *general_lens_inspection_mode_combo{nullptr};
  QPushButton* general_clear_lock_button{ nullptr };

  QSlider *general_zoom_slider{nullptr};
  QLabel *general_zoom_label{nullptr};
  QLabel *general_zoom_title_label{nullptr};
  QWidget *zoom_widget{nullptr};

  QLineEdit *quality_edit{nullptr};
  QSlider *quality_slider{nullptr};
  QLabel *quality_label{nullptr};
  QLabel *quality_title_label{nullptr};
  QLineEdit *bitrate_edit{nullptr};
  QSlider *bitrate_slider{nullptr};
  QLabel *bitrate_label{nullptr};
  QLabel *bitrate_title_label{nullptr};
  QComboBox *mode_combo{nullptr};

  QLineEdit *gamma_edit{nullptr};
  QSlider *gamma_slider{nullptr};
  QLabel *gamma_label{nullptr};
  QLabel *gamma_title_label{nullptr};

  // Droplist for selecting the video mode (replaces multiple mode buttons)
  QComboBox *video_mode_combo{nullptr};
  QPushButton *edge_button{nullptr};

  // Hough Circle detection controls
  QPushButton *circle_detect_button{nullptr};
  QLabel *circle_count_label{nullptr};
  QLabel *hough_circle_header_label{nullptr};
  QLabel *circle_param2_title_label{nullptr};
  QLineEdit *circle_param2_edit{nullptr};
  QSlider *circle_param2_slider{nullptr};
  QLabel   *circle_worst_n_label{nullptr};
  QLineEdit *circle_worst_n_edit{nullptr};
  QSlider *circle_worst_n_slider{nullptr};
  int circle_detected_count{0};

  // Exporter Tab
  QLineEdit *serial_input{nullptr};
  MetricsManager &metrics_manager;
  VideoWidget *gl_viewer_window{nullptr};
  CameraControlPanel *camera_controls{nullptr};
  bool overlayState{true};
  QString screenshotDirectory = QDir::currentPath();
  QLabel *browse_label{nullptr};
  QPushButton *browse_button{nullptr};
  QPushButton *screenshot_button{nullptr};
  QLabel *screenshot_status_label{nullptr};
  QPushButton *metrics_export_button{nullptr};
  QCheckBox *overlay_button{nullptr};

  QString exposure_unit_ms;
  QString gain_unit_db;

public slots:
  void onSetTab0Visibility();
  void onSetTab1Visibility();
  void onSetTab2Visibility();
  void onSetTab3Visibility();
  void onSetTab4Visibility();
  void onSetTab5Visibility();

private slots:
  void onSetExposure();
  void onSetGain();
  void onSetZoom(bool reset);
  void onSetGamma();
  void onSetCompression();
  void onClearROILocks();
  void onSetVideoMode(int modeEnum);
  bool isEdgeDetectCompatible(int mode);
  void onCircleParam2Changed();
  void takeScreenshot();
  void onWorstMarkersNChanged();
};
