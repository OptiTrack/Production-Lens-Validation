#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QScreen>
#include <QGroupBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QScrollArea>
#include <QDateTime>
#include <QCoreApplication>
#include <QGuiApplication.h>
#include "widgets/graphwidget.h"
#include "metricscontroller.h"
#include <QGuiApplication.h>
#include "QtCameraControlPanel.h"
#include "QtCameraConnectionManager.h"
#include "QtCameraViewer.h"
#include "QtVideoWidget.h"
#include "CameraHelpers.h"
#include "MetricsManager.h"

using namespace CameraLibrary;

// Specialized collection of widgets for camera controls

CameraControlPanel::CameraControlPanel(CameraConnectionManager* mgr, MetricsManager& mMgr, QWidget* parent)
    : QWidget(parent), camera_manager(mgr), metrics_manager(mMgr) {
    buildUi();
    connect(this, &CameraControlPanel::showWarning, this, [](const QString& t, const QString& m){
        QMessageBox::warning(nullptr, t, m);
    });
}

bool CameraControlPanel::currentSerialValid() const {
    return selected_serial != 0;
}

void CameraControlPanel::buildUi() {
    // auto* root = new QVBoxLayout(this);
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(6);

    leftTabWidget = new QTabWidget(this);
    // use the 'underline' tab style (sleek blue underline for active tab)
    if (leftTabWidget->tabBar()) {
        leftTabWidget->tabBar()->setProperty("underline", true);
        // give the left tab bar a stable object name so CSS can target it precisely
        leftTabWidget->tabBar()->setObjectName("leftControlTabs");
    }
    leftTabWidget->setMinimumWidth(300);
    leftTabWidget->setMaximumWidth(300);

    auto* row1 = new QWidget(this);
    auto* h1   = new QHBoxLayout(row1); h1->setContentsMargins(0,0,0,0);


    /*
    ********** Tab: Camera Controls and Video Modes ***************
    */

    auto* tab0 = new QWidget(this);
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidget(tab0);
    auto* v0 = new QVBoxLayout(tab0);

    // Group: Camera Controls (exposure, fps, gain)

    cam_group = new QGroupBox(this);
    auto* camLayout = new QVBoxLayout(this); camLayout->setContentsMargins(6,6,6,6);
    cam_group->setLayout(camLayout);
    
    // Exposure: slider from 1 to 200
    exposure_slider = new QSlider(Qt::Horizontal, this);
    exposure_slider->setRange(1, 200);
    exposure_slider->setValue(50);
    exposure_slider->setMaximumWidth(150);
    exposure_edit = new QLineEdit(cam_group);
    exposure_edit->setValidator(new QIntValidator(1, 200, exposure_edit));
    exposure_edit->setMaximumWidth(64);
    exposure_label = new QLabel(cam_group);
    exposure_label->setMaximumWidth(75);
    exposure_label->setMinimumWidth(75);
    connect(exposure_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        Q_UNUSED(val);
        updateSliderLabels();
    });
    connect(exposure_edit, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        int v = exposure_edit->text().toInt(&ok);
        if (!ok) {
            exposure_edit->setText(QString::number(exposure_slider->value()));
            return;
        }
        exposure_slider->setValue(qBound(1, v, 200));
    });
    exposure_button = new QPushButton(cam_group);
    exposure_button->setProperty("secondary", true);
    connect(exposure_button, &QPushButton::clicked, this, [this](){
        onSetExposure();
    });

    // Frame Rate: slider from 1 to 1000
    fps_slider = new QSlider(Qt::Horizontal, cam_group);
    fps_slider->setRange(1, 1000);
    fps_slider->setValue(30);
    fps_slider->setMaximumWidth(150);
    fps_edit = new QLineEdit(cam_group);
    fps_edit->setValidator(new QIntValidator(1, 1000, fps_edit));
    fps_edit->setMaximumWidth(64);
    fps_label = new QLabel(cam_group);
    fps_label->setMaximumWidth(80);
    fps_label->setMinimumWidth(80);
    connect(fps_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        Q_UNUSED(val);
        updateSliderLabels();
    });
    connect(fps_edit, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        int v = fps_edit->text().toInt(&ok);
        if (!ok) {
            fps_edit->setText(QString::number(fps_slider->value()));
            return;
        }
        fps_slider->setValue(qBound(1, v, 1000));
    });
    fps_button  = new QPushButton(cam_group);
    fps_button->setProperty("secondary", true);
    connect(fps_button, &QPushButton::clicked, this, [this](){
        onSetFps();
    });

    // Gain: slider from 0 to 7
    gain_slider = new QSlider(Qt::Horizontal, cam_group);
    gain_slider->setRange(0, 7);
    gain_slider->setValue(0);
    gain_slider->setMaximumWidth(100);
    gain_edit = new QLineEdit(cam_group);
    gain_edit->setValidator(new QIntValidator(0, 7, gain_edit));
    gain_edit->setMaximumWidth(64);
    gain_label = new QLabel(cam_group);
    gain_label->setMaximumWidth(60);
    gain_label->setMinimumWidth(60);
    connect(gain_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        Q_UNUSED(val);
        updateSliderLabels();
    });
    connect(gain_edit, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        int v = gain_edit->text().toInt(&ok);
        if (!ok) {
            gain_edit->setText(QString::number(gain_slider->value()));
            return;
        }
        gain_slider->setValue(qBound(0, v, 7));
    });
    gain_button  = new QPushButton(cam_group);
    gain_button->setProperty("secondary", true);
    connect(gain_button, &QPushButton::clicked, this, [this](){
        onSetGain();
    });

    // Build compact horizontal widgets for each camera control
    auto* exposureWidget = new QWidget(cam_group);
    auto* expLayout = new QVBoxLayout(exposureWidget); expLayout->setContentsMargins(0,0,0,0); expLayout->setSpacing(8);
    exposure_title_label = new QLabel(exposureWidget);
    exposure_title_label->setMinimumWidth(80);
    exposure_title_label->setMaximumWidth(80);
    expLayout->addWidget(exposure_title_label, 0, Qt::AlignLeft);
    auto* exposureRow = new QWidget(exposureWidget);
    auto* exposureRowLayout = new QHBoxLayout(exposureRow); exposureRowLayout->setContentsMargins(0,0,0,0); exposureRowLayout->setSpacing(6);
    exposureRowLayout->addWidget(exposure_slider);
    exposureRowLayout->addWidget(exposure_edit);
    expLayout->addWidget(exposureRow);
    expLayout->addWidget(exposure_label, 0, Qt::AlignLeft);
    expLayout->addWidget(exposure_button);

    auto* fpsWidget = new QWidget(cam_group);
    auto* fpsLayoutW = new QVBoxLayout(fpsWidget); fpsLayoutW->setContentsMargins(0,0,0,0); fpsLayoutW->setSpacing(8);
    fps_title_label = new QLabel(fpsWidget);
    fps_title_label->setMinimumWidth(80);
    fps_title_label->setMaximumWidth(80);
    fpsLayoutW->addWidget(fps_title_label, 0, Qt::AlignLeft);
    auto* fpsRow = new QWidget(fpsWidget);
    auto* fpsRowLayout = new QHBoxLayout(fpsRow); fpsRowLayout->setContentsMargins(0,0,0,0); fpsRowLayout->setSpacing(6);
    fpsRowLayout->addWidget(fps_slider);
    fpsRowLayout->addWidget(fps_edit);
    fpsLayoutW->addWidget(fpsRow);
    fpsLayoutW->addWidget(fps_label, 0, Qt::AlignLeft);
    fpsLayoutW->addWidget(fps_button);

    auto* gainWidget = new QWidget(cam_group);
    auto* gainLayoutW = new QVBoxLayout(gainWidget); gainLayoutW->setContentsMargins(0,0,0,0); gainLayoutW->setSpacing(8);
    gain_title_label = new QLabel(gainWidget);
    gain_title_label->setMaximumWidth(80);
    gain_title_label->setMinimumWidth(80);
    gainLayoutW->addWidget(gain_title_label, 0, Qt::AlignLeft);
    auto* gainRow = new QWidget(gainWidget);
    auto* gainRowLayout = new QHBoxLayout(gainRow); gainRowLayout->setContentsMargins(0,0,0,0); gainRowLayout->setSpacing(6);
    gainRowLayout->addWidget(gain_slider);
    gainRowLayout->addWidget(gain_edit);
    gainLayoutW->addWidget(gainRow);
    gainLayoutW->addWidget(gain_label, 0, Qt::AlignLeft);
    gainLayoutW->addWidget(gain_button);
    

    camLayout->addWidget(exposureWidget);
    camLayout->addWidget(fpsWidget);
    camLayout->addWidget(gainWidget);


    // Edge Detect mode: behave like Grayscale but enable an edge-overlay in the viewer
    // Add a Video Mode dropdown next to existing controls so modes appear with other controls
    video_mode_combo = new QComboBox(tab0);
    repopulateVideoModes();

    // Selecting any regular mode should disable Edge Detect if it was enabled
    connect(video_mode_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [this](int idx){
        if (!currentSerialValid()) {
            emit showWarning(tr("No Camera"), tr("No camera is currently selected."));
            return;
        }

        // find mode value from current data and request it
        QVariant itemData = video_mode_combo->itemData(idx);
        int mode;
        bool markerZoom = false;

        // Check if data is a list (Grayscale modes) or a single int (other modes)
        if (itemData.canConvert<QVariantList>()) {
            QVariantList dataList = itemData.toList();
            mode = dataList[0].toInt();
            markerZoom = dataList[1].toBool();
        } else {
            mode = itemData.toInt();
        }

        onSetVideoMode(mode);

        // Disable edge button and Zoom controls for incompatible modes (Segment, Object, Duplex)
        const bool isCompatible = isEdgeDetectCompatible(mode);
        edge_button->setEnabled(isCompatible);

        // handle ROI-Zoom UI behavior
        zoom_button->setEnabled(markerZoom);
        zoom_slider->setEnabled(markerZoom);
        if (!markerZoom) {
            zoom_slider->setValue(1.0);
        }

        if (!isCompatible && edge_button->isChecked()) {
            edge_button->setChecked(false);
            emit edgeDetectToggled(false);
        }

        // Handle ROI marker zoom case with grayscale mode
        emit onMarkerZoomToggled(markerZoom);
    });

    // Group: Video Modes (dropdown + Edge Detect toggle)
    video_group = new QGroupBox(this);
    auto* video_layout = new QVBoxLayout(video_group); video_layout->setContentsMargins(6,6,6,6);
    video_group->setLayout(video_layout);
    video_layout->addWidget(video_mode_combo);
    edge_button = new QPushButton(video_group);
    edge_button->setCheckable(true);
    edge_button->setProperty("secondary", true);
    
    // Set initial state based on first item in combo (Segment mode)
    const int initialMode = video_mode_combo->itemData(0).toInt();
    edge_button->setEnabled(isEdgeDetectCompatible(initialMode));
    
    connect(edge_button, &QPushButton::toggled, this, [this](bool checked){
        if (checked) {
            if (!currentSerialValid()) { emit showWarning(tr("No Camera"), tr("No camera is currently selected.")); edge_button->setChecked(false); return; }
        }
        emit edgeDetectToggled(checked);
    });
    edge_button->setToolTip(QString());


    leftTabWidget->addTab(tab0, QString());

    // add camera controls and video modes to tab
    v0->addWidget(cam_group);
    v0->addWidget(video_group);
    v0->addStretch();
    video_layout->addWidget(edge_button);
    h1->addWidget(leftTabWidget);
    

    /*
    ********** Tab: Focus Tool and Lens Inspection ***************
    */

    // Row: Video modes - convert previous buttons into a single dropdown embedded with other controls
    auto* tab1 = new QWidget(this);
    auto* v1 = new QVBoxLayout(tab1);

    // Group: Focus Tool

    auto* focusToolGroup = new QGroupBox("Focus Tool");
    auto* focusToolLayout = new QVBoxLayout(this); focusToolLayout->setContentsMargins(6,6,6,6);
    focusToolGroup->setLayout(focusToolLayout);

    // Focus Tool enable/disable checkbox
    focus_button = new QCheckBox(focusToolGroup);
    focus_button->setText("Focus Enabled");
    focus_button->setChecked(true);
    connect(focus_button, &QCheckBox::clicked, this, [this]() {
        focusState = !focusState;
        if (focusState) {
            focus_button->setText("Focus Enabled");
            emit focusToolToggled(true);
        }
        else {
            focus_button->setText("Focus Disabled" );
            emit focusToolToggled(false);
        }
    });

    // Focus HUD enable/disable checkbox
    focusHUD_button = new QCheckBox(focusToolGroup);
    focusHUD_button->setText("Focus HUD Enabled");
    focusHUD_button->setChecked(true);
    connect(focusHUD_button, &QCheckBox::clicked, this, [this]() { 
        focusHUDState = !focusHUDState;
        if (focusHUDState) {
            focusHUD_button->setText("Focus HUD Enabled");
            emit focusHUDToggled(true);
        }
        else {
            focusHUD_button->setText("Focus HUD Disabled" );
            emit focusHUDToggled(false);
        }
    });

    focusToolLayout->addWidget(focus_button);
    focusToolLayout->addWidget(focusHUD_button);

    leftTabWidget->addTab(tab1, QString());
    v1->addWidget(focusToolGroup);

    // Group: Lens Inspection

    auto* lensInspectionGroup = new QGroupBox("Lens Inspection");
    auto* lensInspectionLayout = new QVBoxLayout(lensInspectionGroup); lensInspectionLayout->setContentsMargins(6,6,6,6);
    // Zoom Slider (1x to 20x)
    zoom_slider = new QSlider(Qt::Horizontal, lensInspectionGroup);
    zoom_slider->setRange(1, 20);
    zoom_slider->setValue(1);
    zoom_slider->setMaximumWidth(100);
    zoom_label = new QLabel("1x", lensInspectionGroup);
    zoom_label->setMaximumWidth(60);
    zoom_label->setMinimumWidth(60);

    // Sliders output an int, but the implicit conversion to float is safe.
    connect(zoom_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val) {
        zoom_label->setText(QString::number(val) + "x");
        onSetZoom(false);
        });
    zoom_button = new QPushButton("Reset", lensInspectionGroup);
    zoom_button->setProperty("primary", true);
    connect(zoom_button, &QPushButton::clicked, this, [this]() {
        zoom_slider->setValue(1.0);
    });
    zoom_button->setEnabled(false);
	zoom_slider->setEnabled(false);

    auto* zoomWidget = new QWidget(lensInspectionGroup);
    zoomWidget->setToolTip("Zooms into captured image. Available only in Grayscale + ROI Zoom mode.");
    auto* zoomLayoutW = new QVBoxLayout(zoomWidget); zoomLayoutW->setContentsMargins(0, 0, 0, 0); zoomLayoutW->setSpacing(8);
    auto* zoomLbl = new QLabel("Zoom:", zoomWidget);
    zoomLbl->setMinimumWidth(80);
    zoomLbl->setMaximumWidth(80);
    zoomLayoutW->addWidget(zoomLbl, 0, Qt::AlignLeft);
    zoomLayoutW->addWidget(zoom_slider);
    zoomLayoutW->addWidget(zoom_label, 0, Qt::AlignLeft);
    zoomLayoutW->addWidget(zoom_button);

    lensInspectionLayout->addWidget(zoomWidget);

    // Hough Circle Detection
    circle_detect_button = new QPushButton("Enable Circle Detection", lensInspectionGroup);
    circle_detect_button->setCheckable(true);
    circle_detect_button->setChecked(false);
    circle_detect_button->setProperty("secondary", true);
    connect(circle_detect_button, &QPushButton::clicked, this, [this](bool checked) {
        emit circleDetectionToggled(checked);
    });

    circle_count_label = new QLabel("Circles Detected: 0", lensInspectionGroup);

    // Circle detection param2 (accumulator threshold)
    circle_param2_slider = new QSlider(Qt::Horizontal, lensInspectionGroup);
    circle_param2_slider->setRange(1, 100);
    circle_param2_slider->setValue(10);
    circle_param2_slider->setMaximumWidth(100);
    
    circle_param2_edit = new QLineEdit(lensInspectionGroup);
    circle_param2_edit->setText("10");
    circle_param2_edit->setMaximumWidth(60);
    connect(circle_param2_edit, &QLineEdit::textChanged, this, &CameraControlPanel::onCircleParam2Changed);
    connect(circle_param2_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val) {
        circle_param2_edit->setText(QString::number(val));
    });

    // Build circle detection controls widget
    auto* circleCtrlsWidget = new QWidget(lensInspectionGroup);
    auto* circleCtrlsLayout = new QVBoxLayout(circleCtrlsWidget); 
    circleCtrlsLayout->setContentsMargins(0, 0, 0, 0); 
    circleCtrlsLayout->setSpacing(8);
    
    circleCtrlsLayout->addWidget(circle_detect_button);
    circleCtrlsLayout->addWidget(circle_count_label, 0, Qt::AlignLeft);
    
    auto* param2LblLayout = new QHBoxLayout();
    auto* param2Lbl = new QLabel("Param2 (Threshold):", circleCtrlsWidget);
    param2Lbl->setMinimumWidth(120);
    param2Lbl->setMaximumWidth(120);
    param2LblLayout->addWidget(param2Lbl, 0, Qt::AlignLeft);
    param2LblLayout->addWidget(circle_param2_slider);
    param2LblLayout->addWidget(circle_param2_edit);
    param2LblLayout->addStretch();
    circleCtrlsLayout->addLayout(param2LblLayout);

    lensInspectionLayout->addSpacing(12);
    lensInspectionLayout->addWidget(new QLabel("<b>Hough Circle Detection</b>", lensInspectionGroup));
    lensInspectionLayout->addWidget(circleCtrlsWidget);

    v1->addWidget(lensInspectionGroup);
    v1->addStretch();


    /*
    *************** Tab: Color compression / gamma ***************
    */

    auto* tab2 = new QWidget(this);
    auto* v2   = new QVBoxLayout(tab2);

    // Group: Color Compression (quality, bitrate, mode dropdown)
    compression_group = new QGroupBox(this);
    auto* compLayout = new QVBoxLayout(compression_group); compLayout->setContentsMargins(6,6,6,6);
    compression_group->setLayout(compLayout);

    // Quality slider (0.0 - 1.0, scaled to 0-100 for slider)
    quality_slider = new QSlider(Qt::Horizontal, compression_group);
    quality_slider->setRange(0, 100);
    quality_slider->setValue(75);
    quality_slider->setMaximumWidth(120);
    quality_edit = new QLineEdit(compression_group);
    quality_edit->setValidator(new QDoubleValidator(0.0, 1.0, 2, quality_edit));
    quality_edit->setMaximumWidth(64);
    quality_label = new QLabel(compression_group);
    quality_label->setMaximumWidth(50);
    quality_label->setMinimumWidth(50);
    connect(quality_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        quality_label->setText(QString::number(val / 100.0, 'f', 2));
        if (quality_edit) quality_edit->setText(QString::number(val / 100.0, 'f', 2));
    });
    connect(quality_edit, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        const double v = quality_edit->text().toDouble(&ok);
        if (!ok) {
            quality_edit->setText(QString::number(quality_slider->value() / 100.0, 'f', 2));
            return;
        }
        const double bounded = qBound(0.0, v, 1.0);
        quality_slider->setValue(static_cast<int>(bounded * 100.0 + 0.5));
    });

    // Bitrate slider (0 - 10000 Mbps)
    bitrate_slider = new QSlider(Qt::Horizontal, compression_group);
    bitrate_slider->setRange(0, 200);
    bitrate_slider->setValue(50);
    bitrate_slider->setMaximumWidth(120);
    bitrate_edit = new QLineEdit(compression_group);
    bitrate_edit->setValidator(new QDoubleValidator(0.0, 2.0, 2, bitrate_edit));
    bitrate_edit->setMaximumWidth(64);
    bitrate_label = new QLabel(compression_group);
    bitrate_label->setMaximumWidth(60);
    bitrate_label->setMinimumWidth(60);
    connect(bitrate_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        bitrate_label->setText(QString::number(val / 100.0, 'f', 2));
        if (bitrate_edit) bitrate_edit->setText(QString::number(val / 100.0, 'f', 2));
    });
    connect(bitrate_edit, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        const double v = bitrate_edit->text().toDouble(&ok);
        if (!ok) {
            bitrate_edit->setText(QString::number(bitrate_slider->value() / 100.0, 'f', 2));
            return;
        }
        const double bounded = qBound(0.0, v, 2.0);
        bitrate_slider->setValue(static_cast<int>(bounded * 100.0 + 0.5));
    });

    mode_combo = new QComboBox(compression_group);
    repopulateCompressionModes();

    set_compression_button = new QPushButton(compression_group);
    set_compression_button->setProperty("secondary", true);
    connect(set_compression_button, &QPushButton::clicked, this, &CameraControlPanel::onSetCompression);

    // Build compression controls widget
    auto* compressionCtrlsWidget = new QWidget(compression_group);
    auto* compressionCtrlsLayout = new QVBoxLayout(compressionCtrlsWidget); compressionCtrlsLayout->setContentsMargins(0,0,0,0); compressionCtrlsLayout->setSpacing(8);
    color_title_label = new QLabel(compressionCtrlsWidget);
    compressionCtrlsLayout->addWidget(color_title_label, 0, Qt::AlignLeft);
    auto* qualityRow = new QWidget(compressionCtrlsWidget);
    auto* qualityRowLayout = new QHBoxLayout(qualityRow); qualityRowLayout->setContentsMargins(0,0,0,0); qualityRowLayout->setSpacing(6);
    qualityRowLayout->addWidget(quality_slider);
    qualityRowLayout->addWidget(quality_edit);
    compressionCtrlsLayout->addWidget(qualityRow);
    compressionCtrlsLayout->addWidget(quality_label, 0, Qt::AlignLeft);

    bitrate_title_label = new QLabel(compressionCtrlsWidget);
    compressionCtrlsLayout->addWidget(bitrate_title_label, 0, Qt::AlignLeft);
    auto* bitrateRow = new QWidget(compressionCtrlsWidget);
    auto* bitrateRowLayout = new QHBoxLayout(bitrateRow); bitrateRowLayout->setContentsMargins(0,0,0,0); bitrateRowLayout->setSpacing(6);
    bitrateRowLayout->addWidget(bitrate_slider);
    bitrateRowLayout->addWidget(bitrate_edit);
    compressionCtrlsLayout->addWidget(bitrateRow);
    compressionCtrlsLayout->addWidget(bitrate_label, 0, Qt::AlignLeft);

    compressionCtrlsLayout->addWidget(mode_combo);
    compressionCtrlsLayout->addWidget(set_compression_button);

    // Group: Color Compression (quality, bitrate, mode dropdown)
    gamma_group = new QGroupBox(this);
    auto* gammaLayout = new QVBoxLayout(gamma_group); gammaLayout->setContentsMargins(6,6,6,6);
    gamma_group->setLayout(gammaLayout);

    // Gamma slider (0.1 - 1.0)
    gamma_slider = new QSlider(Qt::Horizontal, compression_group);
    gamma_slider->setRange(1, 10);
    gamma_slider->setValue(10);
    gamma_slider->setMaximumWidth(100);
    gamma_edit = new QLineEdit(compression_group);
    gamma_edit->setValidator(new QDoubleValidator(0.1, 1.0, 1, gamma_edit));
    gamma_edit->setMaximumWidth(64);
    gamma_label = new QLabel(compression_group);
    gamma_label->setMaximumWidth(40);
    gamma_label->setMinimumWidth(40);
    connect(gamma_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        gamma_label->setText(QString::number(val / 10.0, 'f', 1));
        if (gamma_edit) gamma_edit->setText(QString::number(val / 10.0, 'f', 1));
    });
    connect(gamma_edit, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        const double v = gamma_edit->text().toDouble(&ok);
        if (!ok) {
            gamma_edit->setText(QString::number(gamma_slider->value() / 10.0, 'f', 1));
            return;
        }
        const double bounded = qBound(0.1, v, 1.0);
        gamma_slider->setValue(static_cast<int>(bounded * 10.0 + 0.5));
    });

    gamma_button = new QPushButton(compression_group);
    gamma_button->setProperty("secondary", true);
    connect(gamma_button, &QPushButton::clicked, this, &CameraControlPanel::onSetGamma);

    auto* gammaCtrlsWidget = new QWidget(compression_group);
    auto* gammaCtrlsLayout = new QVBoxLayout(gammaCtrlsWidget); gammaCtrlsLayout->setContentsMargins(0,0,0,0); gammaCtrlsLayout->setSpacing(8);
    gamma_title_label = new QLabel(gammaCtrlsWidget);
    gammaCtrlsLayout->addWidget(gamma_title_label, 0, Qt::AlignLeft);
    auto* gammaRow = new QWidget(gammaCtrlsWidget);
    auto* gammaRowLayout = new QHBoxLayout(gammaRow); gammaRowLayout->setContentsMargins(0,0,0,0); gammaRowLayout->setSpacing(6);
    gammaRowLayout->addWidget(gamma_slider);
    gammaRowLayout->addWidget(gamma_edit);
    gammaCtrlsLayout->addWidget(gammaRow);
    gammaCtrlsLayout->addWidget(gamma_label, 0, Qt::AlignLeft);
    gammaCtrlsLayout->addWidget(gamma_button);

    compLayout->addWidget(compressionCtrlsWidget);
    gammaLayout->addWidget(gammaCtrlsWidget);

    leftTabWidget->addTab(tab2, QString());
    v2->addWidget(compression_group);
    v2->addWidget(gamma_group);
    v2->addStretch();


	/*
    ********** Tab for statistics graphs with MetricController integration ***************
    */

    auto* tabStats = new QWidget(this);
    auto* vStats = new QVBoxLayout(tabStats);

    // Create Focus Metrics with controller
    QVector<QString> focusLabels = {"FocusQuality"};
    QVector<QString> focusDescriptions = {""};
    QVector<bool> focusGraphs = {true};
    focus_metrics_widgets = createMetricWidgets(QString(), QString(), focusLabels, focusDescriptions, focusGraphs);
    focusMetricsController = new MetricController(focus_metrics_widgets);
    vStats->addWidget(focus_metrics_widgets->groupBox);

    // Create Lens Metrics with controller
    QVector<QString> lensLabels = {"LensHealth"};
    QVector<QString> lensDescriptions = {""};
    QVector<bool> lensGraphs = {true};
    lens_metrics_widgets = createMetricWidgets(QString(), QString(), lensLabels, lensDescriptions, lensGraphs);
    lensMetricsController = new MetricController(lens_metrics_widgets);
    vStats->addWidget(lens_metrics_widgets->groupBox);
    vStats->addStretch();

    leftTabWidget->addTab(tabStats, "Statistics");


	/*
    *************** Tab for Exporter ***************
    */
   
    auto* tabExpo = new QWidget(this);
    auto* vExpo = new QVBoxLayout(tabExpo);

    exporter_group = new QGroupBox(tabExpo);
    auto* exporterLayout = new QVBoxLayout(exporter_group); exporterLayout->setContentsMargins(6,6,6,6);
    exporter_group->setLayout(exporterLayout);

	// Serial number input
    serial_input = new QLineEdit(exporter_group);
	connect(serial_input, &QLineEdit::textChanged, this, [this](const QString& text) {
		metrics_manager.setLensSerial(text.toStdString());
	});
    exporterLayout->addWidget(serial_input);

	// Browse button for screenshot directory
	auto* browseDirLayout = new QHBoxLayout();
    browse_label = new QLabel(exporter_group);
    browse_button = new QPushButton(exporter_group);
    browse_button->setProperty("secondary", true);
	connect(browse_button, &QPushButton::clicked, this, [this]() {
		QString dir = QFileDialog::getExistingDirectory(
			this,
            tr("Select Screenshot Directory"),
			screenshotDirectory
		);
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
	connect(screenshot_button, &QPushButton::clicked, this, [this]() {
		takeScreenshot();
	});
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
	connect(metrics_export_button, &QPushButton::clicked, this, [this]() {
		emit exportMetricsRequested();
	});
    exporterLayout->addWidget(metrics_export_button);

	// Overlay enable/disable checkbox
    overlay_button = new QCheckBox(exporter_group);
	overlay_button->setChecked(true);
    overlay_button->setProperty("secondary", true);
	connect(overlay_button, &QCheckBox::clicked, this, [this]() {
		overlayState = overlay_button->isChecked();
        updateOverlayButtonText();
	});
    exporterLayout->addWidget(overlay_button);

    vExpo->addWidget(exporter_group);
	vExpo->addStretch();

    leftTabWidget->addTab(tabExpo, QString());

    root->addWidget(leftTabWidget);

	retranslateUi();
}

MetricWidgets* CameraControlPanel::createMetricWidgets(const QString name, const QString units, QVector<QString> labels, QVector<QString> descriptions, QVector<bool> graphs) {

    // Create new metricWidgets structure & layout
    MetricWidgets* metricWidgets = new MetricWidgets();
    QVBoxLayout* layout = new QVBoxLayout();

    // Set name & units
    metricWidgets->name = name;
    metricWidgets->units = units;

    // Set groupBox Settings
    metricWidgets->groupBox->setTitle(name);
    metricWidgets->groupBox->setCheckable(true);

    for (int i = 0; i < labels.count(); ++i) {
        QLabel* dataLabel = new QLabel();
        dataLabel->setObjectName(labels[i] + "DataLabel");
        dataLabel->setText("- " + units);
        dataLabel->setAlignment(Qt::AlignRight);

        layout->addWidget(dataLabel);
        metricWidgets->dataLabels.append(dataLabel);

        QString description = descriptions.value(i);
        if (!description.isEmpty()) {
            QLabel* descriptionLabel = new QLabel();
            descriptionLabel->setObjectName(labels[i] + "DescriptionLabel");
            descriptionLabel->setText(description);
            descriptionLabel->setAlignment(Qt::AlignRight);

            layout->addWidget(descriptionLabel);
            metricWidgets->descriptionLabels.append(descriptionLabel);
        }

        if (graphs[i]) {
            GraphWidget* metricGraph = new GraphWidget(metricWidgets->groupBox);
            metricGraph->setObjectName(labels[i] + "Graph");
            metricGraph->setMinimumHeight(100);

            layout->addWidget(metricGraph);
            metricWidgets->metricGraphs.append(metricGraph);
        }
        else {
            metricWidgets->metricGraphs.append(nullptr);
        }
    }

    metricWidgets->groupBox->setLayout(layout);

    return metricWidgets;
}

void CameraControlPanel::setExportLanguage(MetricsManager::OutputLanguage lang)
{
    metrics_manager.setLanguage(lang);
}

void CameraControlPanel::updateFocusButtonText()
{
    if (!focus_button) return;
    focus_button->setText(focusState
        ? tr("Focus Enabled")
        : tr("Focus Disabled"));
}

void CameraControlPanel::updateFocusHudButtonText()
{
    if (!focusHUD_button) return;
    focusHUD_button->setText(focusHUDState
        ? tr("Focus HUD Enabled")
        : tr("Focus HUD Disabled"));
}

void CameraControlPanel::updateOverlayButtonText()
{
    if (!overlay_button) return;
    overlay_button->setText(overlayState
        ? tr("Overlay Enabled")
        : tr("Overlay Disabled"));
}

void CameraControlPanel::updateSliderLabels()
{
    if (exposure_slider && exposure_label) {
        exposure_label->setText(QString::number(exposure_slider->value()) + " " + exposure_unit_ms);
        if (exposure_edit) exposure_edit->setText(QString::number(exposure_slider->value()));
    }
    if (fps_slider && fps_label) {
        fps_label->setText(QString::number(fps_slider->value()) + " " + fps_unit);
        if (fps_edit) fps_edit->setText(QString::number(fps_slider->value()));
    }
    if (gain_slider && gain_label) {
        gain_label->setText(QString::number(gain_slider->value()) + " " + gain_unit_db);
        if (gain_edit) gain_edit->setText(QString::number(gain_slider->value()));
    }
}

static bool videoModeDataEqual(const QVariant& a, const QVariant& b)
{
    if (a.canConvert<QVariantList>() && b.canConvert<QVariantList>()) {
        const QVariantList al = a.toList();
        const QVariantList bl = b.toList();
        return al.size() == bl.size() && al.value(0) == bl.value(0) && al.value(1) == bl.value(1);
    }
    return a == b;
}

void CameraControlPanel::repopulateVideoModes()
{
    if (!video_mode_combo) return;
    const QVariant currentData = video_mode_combo->currentData();
    video_mode_combo->blockSignals(true);
    video_mode_combo->clear();

    video_mode_combo->addItem(tr("Segment"), QVariant(static_cast<int>(Core::SegmentMode)));
    video_mode_combo->setItemData(video_mode_combo->count() - 1,
        tr("5-segment view (center+corners)"), Qt::ToolTipRole);

    video_mode_combo->addItem(tr("Grayscale"), QVariantList{
        static_cast<int>(Core::GrayscaleMode),
        false });
    video_mode_combo->setItemData(video_mode_combo->count() - 1,
        tr("8bpp camera preview"), Qt::ToolTipRole);

    video_mode_combo->addItem(tr("Grayscale with ROI Zoom"), QVariantList{
        static_cast<int>(Core::GrayscaleMode),
        true });
    video_mode_combo->setItemData(video_mode_combo->count() - 1,
        tr("8bpp camera preview with center/edge marker focus"), Qt::ToolTipRole);

    video_mode_combo->addItem(tr("Object"), QVariant(static_cast<int>(Core::ObjectMode)));
    video_mode_combo->setItemData(video_mode_combo->count() - 1,
        tr("Object mode: runs detection pipeline"), Qt::ToolTipRole);
    video_mode_combo->addItem(tr("Precision"), QVariant(static_cast<int>(Core::PrecisionMode)));
    video_mode_combo->setItemData(video_mode_combo->count() - 1,
        tr("Precision view: tighter quality metrics"), Qt::ToolTipRole);
    video_mode_combo->addItem(tr("MJPEG"), QVariant(static_cast<int>(Core::MJPEGMode)));
    video_mode_combo->setItemData(video_mode_combo->count() - 1,
        tr("MJPEG streaming mode"), Qt::ToolTipRole);
    video_mode_combo->addItem(tr("Duplex"), QVariant(static_cast<int>(Core::DuplexMode)));
    video_mode_combo->setItemData(video_mode_combo->count() - 1,
        tr("Duplex: two-stream capture"), Qt::ToolTipRole);

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

void CameraControlPanel::repopulateCompressionModes()
{
    if (!mode_combo) return;
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

void CameraControlPanel::retranslateUi()
{
    if (cam_group) cam_group->setTitle(tr("General Camera Controls"));
    if (focus_tool_group) focus_tool_group->setTitle(tr("Focus Tool"));
    if (video_group) video_group->setTitle(tr("Video Mode"));
    if (compression_group) compression_group->setTitle(tr("Color Compression"));
    if (gamma_group) gamma_group->setTitle(tr("Gamma"));
    if (exporter_group) exporter_group->setTitle(tr("Exporter"));

    if (exposure_title_label) exposure_title_label->setText(tr("Exposure:"));
    if (fps_title_label) fps_title_label->setText(tr("FPS:"));
    if (gain_title_label) gain_title_label->setText(tr("Gain:"));
    if (color_title_label) color_title_label->setText(tr("Color:"));
    if (bitrate_title_label) bitrate_title_label->setText(tr("Bitrate:"));
    if (gamma_title_label) gamma_title_label->setText(tr("Gamma:"));

    if (exposure_button) exposure_button->setText(tr("Apply"));
    if (fps_button) fps_button->setText(tr("Apply"));
    if (gain_button) gain_button->setText(tr("Apply"));
    if (set_compression_button) set_compression_button->setText(tr("Apply"));
    if (gamma_button) gamma_button->setText(tr("Apply"));

    exposure_unit_ms = tr("ms");
    fps_unit = tr("fps");
    gain_unit_db = tr("dB");
    updateSliderLabels();
    if (quality_slider && quality_label) {
        quality_label->setText(QString::number(quality_slider->value() / 100.0, 'f', 2));
    }
    if (quality_slider && quality_edit) {
        quality_edit->setText(QString::number(quality_slider->value() / 100.0, 'f', 2));
    }
    if (bitrate_slider && bitrate_label) {
        bitrate_label->setText(QString::number(bitrate_slider->value() / 100.0, 'f', 2));
    }
    if (bitrate_slider && bitrate_edit) {
        bitrate_edit->setText(QString::number(bitrate_slider->value() / 100.0, 'f', 2));
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

    if (edge_button) {
        edge_button->setText(tr("Edge Detect"));
        edge_button->setToolTip(tr("Enable edge overlay in viewer: Works on Grayscale, Precision, and MJPEG modes"));
    }

    repopulateVideoModes();
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

    if (leftTabWidget) {
        leftTabWidget->setTabText(0, tr("Controls"));
        leftTabWidget->setTabText(1, tr("Lens"));
        leftTabWidget->setTabText(2, tr("Color"));
        leftTabWidget->setTabText(3, tr("Statistics"));
        leftTabWidget->setTabText(4, tr("Exporter"));
    }

    if (focus_metrics_widgets && focus_metrics_widgets->groupBox) {
        focus_metrics_widgets->name = tr("Focus Statistics");
        focus_metrics_widgets->groupBox->setTitle(focus_metrics_widgets->name);
    }
    if (lens_metrics_widgets && lens_metrics_widgets->groupBox) {
        lens_metrics_widgets->name = tr("Lens Statistics");
        lens_metrics_widgets->groupBox->setTitle(lens_metrics_widgets->name);
    }

    if (serial_input) serial_input->setPlaceholderText(tr("Serial #"));
    if (browse_label) browse_label->setText(tr("Screenshot Dir: %1").arg(screenshotDirectory));
    if (browse_button) browse_button->setText(tr("Browse..."));
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
        metrics_export_button->setToolTip(tr("Click to export information about the currently installed lens."));
    }
}

void CameraControlPanel::onSetExposure() {
    if (!currentSerialValid()) { emit showWarning(tr("No Camera"), tr("No camera is currently selected.")); return; }
    const int v = exposure_slider->value();
    if (!camera_manager->SetExposure(selected_serial, v)) {
        emit showWarning(tr("Failed"), tr("Could not set exposure on the selected camera."));
    }
}

void CameraControlPanel::onSetFps() {
    if (!currentSerialValid()) { emit showWarning(tr("No Camera"), tr("No camera is currently selected.")); return; }
    const int v = fps_slider->value();
    if (!camera_manager->SetFrameRate(selected_serial, v)) {
        emit showWarning(tr("Failed"), tr("Could not set frame rate on the selected camera."));
    }
}

void CameraControlPanel::onSetGain() {
    if (!currentSerialValid()) { emit showWarning(tr("No Camera"), tr("No camera is currently selected.")); return; }
    const int v = gain_slider->value();
    if (!camera_manager->SetImagerGain(selected_serial, v)) {
        emit showWarning(tr("Failed"), tr("Could not set imager gain on the selected camera."));
    }
}

void CameraControlPanel::onSetZoom(bool reset) {
    if (!currentSerialValid()) { emit showWarning("No Camera", "No camera is currently selected."); return; }
    int v = zoom_slider->value();
    if (reset) {
        v = 1;
    }
    emit zoomValueChanged(v);
}

void CameraControlPanel::onSetGamma() {
    if (!currentSerialValid()) { emit showWarning(tr("No Camera"), tr("No camera is currently selected.")); return; }
    const float g = gamma_slider->value() / 10.0f;
    if (!camera_manager->SetColorGamma(selected_serial, g)) {
        emit showWarning(tr("Unsupported Camera"), tr("Color gamma is only supported on Prime Color cameras."));
    }
}

void CameraControlPanel::onSetCompression() {
    if (!currentSerialValid()) { emit showWarning(tr("No Camera"), tr("No camera is currently selected.")); return; }
    const float quality = quality_slider->value() / 100.0f;
    const float mbps = bitrate_slider->value() / 100.0f;
    
    const float bitrateScaled = CameraHelper::MbpsToNormalized(mbps);
    const int mode = mode_combo->currentData().toInt();
    if (!camera_manager->SetColorCompression(selected_serial, mode, quality, bitrateScaled)) {
        emit showWarning(tr("Unsupported Camera"), tr("Color compression is only supported on Prime Color cameras."));
    }
}

void CameraControlPanel::onSetVideoMode(int modeEnum) {
    if (!currentSerialValid()) return;
    QString err;
    if (!camera_manager->SetVideoType(selected_serial, static_cast<Core::eVideoMode>(modeEnum), &err)) {
        if (!err.isEmpty()) emit showWarning(tr("Unsupported Mode"), err);
    }
}

void CameraControlPanel::onSetTab0Visibility() {
    // check if first tab in widget is visible
    if (this->leftTabWidget->isTabVisible(0)) {
        this->leftTabWidget->setTabVisible(0, false);
        if (!(this->leftTabWidget->isTabVisible(1)) && !(this->leftTabWidget->isTabVisible(2) ) && !(this->leftTabWidget->isTabVisible(3)) && !(this->leftTabWidget->isTabVisible(4))) {
            this->leftTabWidget->setVisible(false);
        }
    }
    else {
        this->leftTabWidget->setTabVisible(0, true);
        this->leftTabWidget->setVisible(true);
    }
}

void CameraControlPanel::onSetTab1Visibility() {
    // check if first tab in widget is visible
    if (this->leftTabWidget->isTabVisible(1)) {
        this->leftTabWidget->setTabVisible(1, false);
        if (!(this->leftTabWidget->isTabVisible(0)) && !(this->leftTabWidget->isTabVisible(2)) && !(this->leftTabWidget->isTabVisible(3)) && !(this->leftTabWidget->isTabVisible(4))) {
            this->leftTabWidget->setVisible(false);
        }
    }
    else {
        this->leftTabWidget->setTabVisible(1, true);
        this->leftTabWidget->setVisible(true);
    }
}

void CameraControlPanel::onSetTab2Visibility() {
    // check if first tab in widget is visible
    if (this->leftTabWidget->isTabVisible(2)) {
        this->leftTabWidget->setTabVisible(2, false);
        if (!(this->leftTabWidget->isTabVisible(0)) && !(this->leftTabWidget->isTabVisible(1)) && !(this->leftTabWidget->isTabVisible(3)) && !(this->leftTabWidget->isTabVisible(4))) {
            this->leftTabWidget->setVisible(false);
        }
    }
    else {
        this->leftTabWidget->setTabVisible(2, true);
        this->leftTabWidget->setVisible(true);
    }
}

void CameraControlPanel::onSetTab3Visibility() {
    // check if first tab in widget is visible
    if (this->leftTabWidget->isTabVisible(3)) {
        this->leftTabWidget->setTabVisible(3, false);
        if (!(this->leftTabWidget->isTabVisible(0)) && !(this->leftTabWidget->isTabVisible(1)) && !(this->leftTabWidget->isTabVisible(2)) && !(this->leftTabWidget->isTabVisible(4))) {
            this->leftTabWidget->setVisible(false);
        }
    }
    else {
        this->leftTabWidget->setTabVisible(3, true);
        this->leftTabWidget->setVisible(true);
    }
}

void CameraControlPanel::onSetTab4Visibility() {
    // check if first tab in widget is visible
    if (this->leftTabWidget->isTabVisible(4)) {
        this->leftTabWidget->setTabVisible(4, false);
        if (!(this->leftTabWidget->isTabVisible(0)) && !(this->leftTabWidget->isTabVisible(1)) && !(this->leftTabWidget->isTabVisible(2)) && !(this->leftTabWidget->isTabVisible(3))) {
            this->leftTabWidget->setVisible(false);
        }
    }
    else {
        this->leftTabWidget->setTabVisible(4, true);
        this->leftTabWidget->setVisible(true);
    }
}

bool CameraControlPanel::isEdgeDetectCompatible(int mode)
{
    switch (mode) {
        case Core::SegmentMode:
        case Core::ObjectMode:
        case Core::DuplexMode:
            return false;
        default:
            return true;
    }
}

void CameraControlPanel::updateCircleCount(int count)
{
    if (circle_count_label) {
        circle_count_label->setText(QString("Circles Detected: %1").arg(count));
    }
}

void CameraControlPanel::onCircleParam2Changed()
{
    bool ok;
    double param2 = circle_param2_edit->text().toDouble(&ok);
    if (ok && param2 >= 5.0 && param2 <= 100.0) {
        emit circleParam2Changed(param2);
    }
}

void CameraControlPanel::takeScreenshot()
{
	// Check if loaded screen
	QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        emit showWarning(tr("Screenshot"), tr("No screen is currently available."));
		return;
    }
	// Add Serial number of lens if possible, else put #
	QString serial = serial_input && !serial_input->text().isEmpty() ? serial_input->text() : "#";
	// Get the window image
	QPixmap pix;
	if (overlayState)
		//Capture the entire top-level window (the whole application)
		pix = screen->grabWindow(this->window()->winId());
	else
		// Capture just the video widget if overlay is disabled
		pix = screen->grabWindow(gl_viewer_window->winId());
	// Assign the time and day, with the serial number for file name
	QString filename = QDateTime::currentDateTime().toString("'screenshot_%1_'yyyyMMdd_HHmmss'.png'").arg(serial);
    QString fileLocation = screenshotDirectory.isEmpty() ? filename : QDir(screenshotDirectory).filePath(filename);

    if (!pix.save(fileLocation)) {
        emit showWarning(tr("Screenshot"), tr("Failed to save screenshot."));
        return;
    }

    if (screenshot_status_label) {
        screenshot_status_label->setStyleSheet("color: #1f8f3a;");
        screenshot_status_label->setText(tr("Screenshot Saved: %1").arg(QFileInfo(fileLocation).fileName()));
        screenshot_status_label->setVisible(true);
    }
}