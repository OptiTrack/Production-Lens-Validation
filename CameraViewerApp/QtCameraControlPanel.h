#pragma once
#include <QWidget>
#include <QPointer>

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

signals:
    void showWarning(const QString& title, const QString& message);
    // Toggle edge-detect overlay in the viewer (does not change camera codec beyond selecting grayscale)
    void edgeDetectToggled(bool enabled);

private:
    void buildUi();
    bool currentSerialValid() const;

    QPointer<CameraConnectionManager> camera_manager;
    unsigned selected_serial{0};

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

private slots:
    void onSetExposure();
    void onSetFps();
    void onSetGain();
    void onSetGamma();
    void onSetCompression();
    void onSetVideoMode(int modeEnum);
};

