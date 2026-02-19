#pragma once
#include <QWidget>
#include <QPointer>
#include <QTabWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QVector>
#include <QString>
#include <QLineEdit>
#include <QSlider>
#include "MetricsExporter.h"
#include <QDir>

class GraphWidget;
class MetricController;

#pragma once
#include <QString>
#include <QVector>
#include <QGroupBox>

class QLabel;
class VideoWidget;

struct MetricWidgets {
    QString name;
    QString units;
    QGroupBox* groupBox{nullptr};
    QVector<QLabel*> dataLabels;
    QVector<QLabel*> descriptionLabels;
    QVector<GraphWidget*> metricGraphs;

    // Optional constructor to initialize the group box with a parent
    explicit MetricWidgets(QWidget* parent = nullptr) {
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
    explicit CameraControlPanel(CameraConnectionManager* mgr, MetricsExporter& metricsExporter, QWidget* parent = nullptr);
    void setSelectedSerial(unsigned serial) { selected_serial = serial; }
    MetricController* getFocusMetricsController() const { return focusMetricsController; }
    
    /// @brief Update circle detection count display
    void updateCircleCount(int count);
    bool const returnFocusToolState() { return focusState; }
    bool const returnOverlayState() { return overlayState; }
    void setVideoWidget(VideoWidget* widget) { gl_viewer_window = widget; }
	VideoWidget* videoWidget()    const { return gl_viewer_window; }
signals:
    void showWarning(const QString& title, const QString& message);
    // Toggle circle detection
    void circleDetectionToggled(bool enabled);
    // Update circle detection param2 (accumulator threshold)
    void circleParam2Changed(double param2);
    void edgeDetectToggled(bool enabled);
    void onMarkerZoomToggled(bool enabled);
    void focusToolToggled(bool enabled);
    void zoomValueChanged(float val);
    void focusHUDToggled(bool enabled);
    void exportMetricsRequested();

private:
    void buildUi();
    bool currentSerialValid() const;
    MetricWidgets* createMetricWidgets(const QString name, const QString units, QVector<QString> labels, QVector<QString> descriptions, QVector<bool> graphs);

    QPointer<CameraConnectionManager> camera_manager;
    unsigned selected_serial{0};

    QTabWidget* leftTabWidget{nullptr};

    // Metrics controllers for Statistics tab
    MetricController* focusMetricsController{nullptr};
    MetricController* lensMetricsController{nullptr};

    QLineEdit* exposure_edit{nullptr};
    QSlider* exposure_slider{nullptr};
    QLabel* exposure_label{nullptr};
    QPushButton* exposure_button{nullptr};

    QLineEdit* fps_edit{nullptr};
    QSlider* fps_slider{nullptr};
    QLabel* fps_label{nullptr};
    QPushButton* fps_button{nullptr};

    QLineEdit* gain_edit{nullptr};
    QSlider* gain_slider{nullptr};
    QLabel* gain_label{nullptr};
    QPushButton* gain_button{nullptr};

    QLineEdit* zoom_edit{ nullptr };
    QSlider* zoom_slider{ nullptr };
    QLabel* zoom_label{ nullptr };
    QPushButton* zoom_button{ nullptr };

    QCheckBox*        focus_button{nullptr};
    bool              focusState{true};
    QCheckBox*        focusHUD_button{nullptr};
    bool              focusHUDState{true};

    QLineEdit* quality_edit{nullptr};
    QSlider* quality_slider{nullptr};
    QLabel* quality_label{nullptr};
    QLineEdit* bitrate_edit{nullptr};
    QSlider* bitrate_slider{nullptr};
    QLabel* bitrate_label{nullptr};
    QComboBox* mode_combo{nullptr};
    QPushButton* set_compression_button{nullptr};

    QLineEdit* gamma_edit{nullptr};
    QSlider* gamma_slider{nullptr};
    QLabel* gamma_label{nullptr};
    QPushButton* gamma_button{nullptr};

    // Droplist for selecting the video mode (replaces multiple mode buttons)
    QComboBox* video_mode_combo{nullptr};
    QPushButton* edge_button{nullptr};
    
    // Hough Circle detection controls
    QPushButton* circle_detect_button{nullptr};
    QLabel* circle_count_label{nullptr};
    QLineEdit* circle_param2_edit{nullptr};
    QSlider* circle_param2_slider{nullptr};

    // Exporter Tab
    QLineEdit*        serial_input{nullptr};
    MetricsExporter&  metrics_exporter;
    VideoWidget*      gl_viewer_window{nullptr};
    CameraControlPanel* camera_controls{nullptr};
    bool              overlayState{ true };
    QString           screenshotDirectory = QDir::currentPath();
    QLabel*           browse_label{nullptr};
    QPushButton*      browse_button{nullptr};
    QPushButton*      metrics_export_button{nullptr};
    QCheckBox*        overlay_button{nullptr};

public slots:
    void onSetTab0Visibility();
    void onSetTab1Visibility();
    void onSetTab2Visibility();
	void onSetTab3Visibility();
    void onSetTab4Visibility();

private slots:
    void onSetExposure();
    void onSetFps();
    void onSetGain();
	void onSetZoom(bool reset);
    void onSetGamma();
    void onSetCompression();
    void onSetVideoMode(int modeEnum);
    bool isEdgeDetectCompatible(int mode);
    void onCircleParam2Changed(); 
    void takeScreenshot();
};

