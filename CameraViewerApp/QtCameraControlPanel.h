#pragma once
#include <QWidget>
#include <QPointer>
#include <QTabWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QVector>
#include <QString>

class GraphWidget;
class MetricController;

#pragma once
#include <QString>
#include <QVector>
#include <QGroupBox>

class QLabel;

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
    explicit CameraControlPanel(CameraConnectionManager* mgr, QWidget* parent = nullptr);
    void setSelectedSerial(unsigned serial) { selected_serial = serial; }
    MetricController* getFocusMetricsController() const { return focusMetricsController; }
	
signals:
    void showWarning(const QString& title, const QString& message);
    // Toggle edge-detect overlay in the viewer (does not change camera codec beyond selecting grayscale)
    void edgeDetectToggled(bool enabled);

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


public slots:
    void onSetTab0Visibility();
    void onSetTab1Visibility();
    void onSetTab2Visibility();
	void onSetTab3Visibility();

private slots:
    void onSetExposure();
    void onSetFps();
    void onSetGain();
    void onSetGamma();
    void onSetCompression();
    void onSetVideoMode(int modeEnum);
    bool isEdgeDetectCompatible(int mode); 
};

