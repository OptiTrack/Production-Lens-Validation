#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QIntValidator>
#include <QDoubleValidator>

#include "QtCameraControlPanel.h"
#include "QtCameraConnectionManager.h"
#include "CameraHelpers.h"
#include "cameralibrary.h"

using namespace CameraLibrary;

// Specialized collection of widgets for camera controls

CameraControlPanel::CameraControlPanel(CameraConnectionManager* mgr, QWidget* parent)
    : QWidget(parent), camera_manager(mgr) {
    buildUi();
    connect(this, &CameraControlPanel::showWarning, this, [](const QString& t, const QString& m){
        QMessageBox::warning(nullptr, t, m);
    });
}

bool CameraControlPanel::currentSerialValid() const {
    return selected_serial != 0;
}

void CameraControlPanel::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(6);

    // Row: Exposure / FPS / Gain
    auto* row1 = new QWidget(this);
    auto* h1   = new QHBoxLayout(row1); h1->setContentsMargins(0,0,0,0);

    // Group: Camera Controls (exposure, fps, gain)
    auto* camGroup = new QGroupBox("Camera Controls", row1);
    auto* camLayout = new QHBoxLayout(camGroup); camLayout->setContentsMargins(6,6,6,6);
    
    // Exposure: slider from 1 to 200
    exposure_slider = new QSlider(Qt::Horizontal, camGroup);
    exposure_slider->setRange(1, 200);
    exposure_slider->setValue(50);
    exposure_slider->setMaximumWidth(150);
    exposure_label = new QLabel("50", camGroup);
    exposure_label->setMaximumWidth(50);
    connect(exposure_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        exposure_label->setText(QString::number(val));
    });
    exposure_button = new QPushButton("Apply", camGroup);
    connect(exposure_button, &QPushButton::clicked, this, [this](){
        onSetExposure();
    });

    // Frame Rate: slider from 1 to 1000
    fps_slider = new QSlider(Qt::Horizontal, camGroup);
    fps_slider->setRange(1, 1000);
    fps_slider->setValue(30);
    fps_slider->setMaximumWidth(150);
    fps_label = new QLabel("30", camGroup);
    fps_label->setMaximumWidth(50);
    connect(fps_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        fps_label->setText(QString::number(val));
    });
    fps_button  = new QPushButton("Apply", camGroup);
    connect(fps_button, &QPushButton::clicked, this, [this](){
        onSetFps();
    });

    // Gain: slider from 0 to 7
    gain_slider = new QSlider(Qt::Horizontal, camGroup);
    gain_slider->setRange(0, 7);
    gain_slider->setValue(0);
    gain_slider->setMaximumWidth(100);
    gain_label = new QLabel("0", camGroup);
    gain_label->setMaximumWidth(30);
    connect(gain_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        gain_label->setText(QString::number(val));
    });
    gain_button  = new QPushButton("Apply", camGroup);
    connect(gain_button, &QPushButton::clicked, this, [this](){
        onSetGain();
    });

    // Build compact horizontal widgets for each camera control
    auto* exposureWidget = new QWidget(camGroup);
    auto* expLayout = new QHBoxLayout(exposureWidget); expLayout->setContentsMargins(0,0,0,0); expLayout->setSpacing(4);
    auto* expLabel = new QLabel("Exposure:", exposureWidget);
    expLayout->addWidget(expLabel, 0, Qt::AlignRight);
    expLayout->addWidget(exposure_slider);
    expLayout->addWidget(exposure_label, 0, Qt::AlignRight);
    expLayout->addWidget(exposure_button);

    auto* fpsWidget = new QWidget(camGroup);
    auto* fpsLayoutW = new QHBoxLayout(fpsWidget); fpsLayoutW->setContentsMargins(0,0,0,0); fpsLayoutW->setSpacing(4);
    auto* fpsLbl = new QLabel("FPS:", fpsWidget);
    fpsLayoutW->addWidget(fpsLbl, 0, Qt::AlignRight);
    fpsLayoutW->addWidget(fps_slider);
    fpsLayoutW->addWidget(fps_label, 0, Qt::AlignRight);
    fpsLayoutW->addWidget(fps_button);

    auto* gainWidget = new QWidget(camGroup);
    auto* gainLayoutW = new QHBoxLayout(gainWidget); gainLayoutW->setContentsMargins(0,0,0,0); gainLayoutW->setSpacing(4);
    auto* gainLbl = new QLabel("Gain:", gainWidget);
    gainLayoutW->addWidget(gainLbl, 0, Qt::AlignRight);
    gainLayoutW->addWidget(gain_slider);
    gainLayoutW->addWidget(gain_label, 0, Qt::AlignRight);
    gainLayoutW->addWidget(gain_button);

    camLayout->addWidget(exposureWidget);
    camLayout->addSpacing(8);
    camLayout->addWidget(fpsWidget);
    camLayout->addWidget(gainWidget);
    h1->addWidget(camGroup);
    
    root->addWidget(row1);

    root->addWidget(row1);

    // Row: Video modes - convert previous buttons into a single dropdown embedded with other controls
    // Edge Detect mode: behave like Grayscale but enable an edge-overlay in the viewer
    // Add a Video Mode dropdown next to existing controls so modes appear with other controls
    auto* modeLbl = new QLabel("Video Mode:", row1);
    video_mode_combo = new QComboBox(row1);
    video_mode_combo->addItem("Segment", QVariant(static_cast<int>(Core::SegmentMode)));
    video_mode_combo->setItemData(video_mode_combo->count()-1, "5-segment view (center+corners)", Qt::ToolTipRole);
    video_mode_combo->addItem("Grayscale", QVariant(static_cast<int>(Core::GrayscaleMode)));
    video_mode_combo->setItemData(video_mode_combo->count()-1, "8bpp camera preview", Qt::ToolTipRole);
    video_mode_combo->addItem("Object", QVariant(static_cast<int>(Core::ObjectMode)));
    video_mode_combo->setItemData(video_mode_combo->count()-1, "Object mode: runs detection pipeline", Qt::ToolTipRole);
    video_mode_combo->addItem("Precision", QVariant(static_cast<int>(Core::PrecisionMode)));
    video_mode_combo->setItemData(video_mode_combo->count()-1, "Precision view: tighter quality metrics", Qt::ToolTipRole);
    video_mode_combo->addItem("MJPEG", QVariant(static_cast<int>(Core::MJPEGMode)));
    video_mode_combo->setItemData(video_mode_combo->count()-1, "MJPEG streaming mode", Qt::ToolTipRole);
    video_mode_combo->addItem("Duplex", QVariant(static_cast<int>(Core::DuplexMode)));
    video_mode_combo->setItemData(video_mode_combo->count()-1, "Duplex: two-stream capture", Qt::ToolTipRole);

    // Selecting any regular mode should disable Edge Detect if it was enabled
    connect(video_mode_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [this](int idx){
        if (!currentSerialValid()) { emit showWarning("No Camera", "No camera is currently selected."); return; }
        // find mode value from current data and request it
        const int mode = video_mode_combo->itemData(idx).toInt();
        onSetVideoMode(mode);
        if (edge_button && edge_button->isChecked()) { edge_button->setChecked(false); emit edgeDetectToggled(false); }
    });

    // Group: Video Modes (dropdown + Edge Detect toggle)
    auto* videoGroup = new QGroupBox("Video Mode", row1);
    auto* videoLayout = new QHBoxLayout(videoGroup); videoLayout->setContentsMargins(6,6,6,6);
    videoLayout->addWidget(modeLbl);
    videoLayout->addWidget(video_mode_combo);
    edge_button = new QPushButton("Edge Detect", videoGroup);
    edge_button->setCheckable(true);
    connect(edge_button, &QPushButton::toggled, this, [this](bool checked){
        // If enabling, force camera into Grayscale mode so frames are 8bpp
        if (checked) {
            if (!currentSerialValid()) { emit showWarning("No Camera", "No camera is currently selected."); edge_button->setChecked(false); return; }
            // request grayscale video on camera
            onSetVideoMode(static_cast<int>(Core::GrayscaleMode));
            // Update dropdown so it reflects the camera's requested grayscale mode
            if (video_mode_combo) {
                const int gi = video_mode_combo->findData(QVariant(static_cast<int>(Core::GrayscaleMode)));
                if (gi >= 0) video_mode_combo->setCurrentIndex(gi);
            }
        }
        emit edgeDetectToggled(checked);
    });
    edge_button->setToolTip("Enable edge overlay in viewer (forces Grayscale video mode)");
    videoLayout->addWidget(edge_button);
    h1->addWidget(videoGroup);

    // Row: Color compression / gamma
    auto* row2 = new QWidget(this);
    auto* h2   = new QHBoxLayout(row2); h2->setContentsMargins(0,0,0,0); h2->setSpacing(6);

    // Group: Color Compression (quality, bitrate, mode dropdown)
    auto* compGroup = new QGroupBox("Color Compression", row2);
    auto* compLayout = new QHBoxLayout(compGroup); compLayout->setContentsMargins(6,6,6,6);

    // Quality slider (0.0 - 1.0, scaled to 0-100 for slider)
    quality_slider = new QSlider(Qt::Horizontal, compGroup);
    quality_slider->setRange(0, 100);
    quality_slider->setValue(75);
    quality_slider->setMaximumWidth(120);
    quality_label = new QLabel("0.75", compGroup);
    quality_label->setMaximumWidth(50);
    connect(quality_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        quality_label->setText(QString::number(val / 100.0, 'f', 2));
    });

    // Bitrate slider (0 - 10000 Mbps)
    bitrate_slider = new QSlider(Qt::Horizontal, compGroup);
    bitrate_slider->setRange(0, 200);
    bitrate_slider->setValue(50);
    bitrate_slider->setMaximumWidth(120);
    bitrate_label = new QLabel("50.00", compGroup);
    bitrate_label->setMaximumWidth(50);
    connect(bitrate_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        bitrate_label->setText(QString::number(val / 100.0, 'f', 2));
    });

    mode_combo = new QComboBox(compGroup);
    mode_combo->addItem("Variable Bitrate", QVariant(0));
    mode_combo->addItem("Constant Bitrate", QVariant(1));

    set_compression_button = new QPushButton("Apply", compGroup);
    connect(set_compression_button, &QPushButton::clicked, this, &CameraControlPanel::onSetCompression);

    // Gamma slider (0.1 - 1.0)
    gamma_slider = new QSlider(Qt::Horizontal, compGroup);
    gamma_slider->setRange(1, 10);
    gamma_slider->setValue(10);
    gamma_slider->setMaximumWidth(100);
    gamma_label = new QLabel("1.0", compGroup);
    gamma_label->setMaximumWidth(40);
    connect(gamma_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        gamma_label->setText(QString::number(val / 10.0, 'f', 1));
    });

    gamma_button = new QPushButton("Apply", compGroup);
    connect(gamma_button, &QPushButton::clicked, this, &CameraControlPanel::onSetGamma);

    // Build compression controls widget
    auto* compressionCtrlsWidget = new QWidget(compGroup);
    auto* compressionCtrlsLayout = new QHBoxLayout(compressionCtrlsWidget); compressionCtrlsLayout->setContentsMargins(0,0,0,0); compressionCtrlsLayout->setSpacing(4);
    auto* qualityLbl = new QLabel("Quality:", compressionCtrlsWidget);
    compressionCtrlsLayout->addWidget(qualityLbl, 0, Qt::AlignRight);
    compressionCtrlsLayout->addWidget(quality_slider);
    compressionCtrlsLayout->addWidget(quality_label, 0, Qt::AlignRight);

    auto* bitrateLbl = new QLabel("Bitrate:", compressionCtrlsWidget);
    compressionCtrlsLayout->addWidget(bitrateLbl, 0, Qt::AlignRight);
    compressionCtrlsLayout->addWidget(bitrate_slider);
    compressionCtrlsLayout->addWidget(bitrate_label, 0, Qt::AlignRight);

    compressionCtrlsLayout->addWidget(mode_combo);
    compressionCtrlsLayout->addWidget(set_compression_button);

    auto* gammaCtrlsWidget = new QWidget(compGroup);
    auto* gammaCtrlsLayout = new QHBoxLayout(gammaCtrlsWidget); gammaCtrlsLayout->setContentsMargins(0,0,0,0); gammaCtrlsLayout->setSpacing(4);
    auto* gammaLbl = new QLabel("Gamma:", gammaCtrlsWidget);
    gammaCtrlsLayout->addWidget(gammaLbl, 0, Qt::AlignRight);
    gammaCtrlsLayout->addWidget(gamma_slider);
    gammaCtrlsLayout->addWidget(gamma_label, 0, Qt::AlignRight);
    gammaCtrlsLayout->addWidget(gamma_button);

    compLayout->addWidget(compressionCtrlsWidget);
    compLayout->addWidget(gammaCtrlsWidget);

    h2->addWidget(compGroup);
    root->addWidget(row2);
}

void CameraControlPanel::onSetExposure() {
    if (!currentSerialValid()) { emit showWarning("No Camera", "No camera is currently selected."); return; }
    const int v = exposure_slider->value();
    if (!camera_manager->SetExposure(selected_serial, v)) {
        emit showWarning("Failed", "Could not set exposure on the selected camera.");
    }
}

void CameraControlPanel::onSetFps() {
    if (!currentSerialValid()) { emit showWarning("No Camera", "No camera is currently selected."); return; }
    const int v = fps_slider->value();
    if (!camera_manager->SetFrameRate(selected_serial, v)) {
        emit showWarning("Failed", "Could not set frame rate on the selected camera.");
    }
}

void CameraControlPanel::onSetGain() {
    if (!currentSerialValid()) { emit showWarning("No Camera", "No camera is currently selected."); return; }
    const int v = gain_slider->value();
    if (!camera_manager->SetImagerGain(selected_serial, v)) {
        emit showWarning("Failed", "Could not set imager gain on the selected camera.");
    }
}

void CameraControlPanel::onSetGamma() {
    if (!currentSerialValid()) { emit showWarning("No Camera", "No camera is currently selected."); return; }
    const float g = gamma_slider->value() / 10.0f;
    if (!camera_manager->SetColorGamma(selected_serial, g)) {
        emit showWarning("Unsupported Camera", "Color gamma is only supported on Prime Color cameras.");
    }
}

void CameraControlPanel::onSetCompression() {
    if (!currentSerialValid()) { emit showWarning("No Camera", "No camera is currently selected."); return; }
    const float quality = quality_slider->value() / 100.0f;
    const float mbps = bitrate_slider->value() / 100.0f;
    
    const float bitrateScaled = CameraHelper::MbpsToNormalized(mbps);
    const int mode = mode_combo->currentData().toInt();
    if (!camera_manager->SetColorCompression(selected_serial, mode, quality, bitrateScaled)) {
        emit showWarning("Unsupported Camera", "Color compression is only supported on Prime Color cameras.");
    }
}

void CameraControlPanel::onSetVideoMode(int modeEnum) {
    if (!currentSerialValid()) return;
    QString err;
    if (!camera_manager->SetVideoType(selected_serial, static_cast<Core::eVideoMode>(modeEnum), &err)) {
        if (!err.isEmpty()) emit showWarning("Unsupported Mode", err);
    }
}