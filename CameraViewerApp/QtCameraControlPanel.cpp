#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>
#include "widgets/graphwidget.h"
#include "metricscontroller.h"

#include "QtCameraControlPanel.h"
#include "QtCameraConnectionManager.h"
#include "CameraHelpers.h"

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

    auto* row1 = new QWidget(this);
    auto* h1   = new QHBoxLayout(row1); h1->setContentsMargins(0,0,0,0);

    // Tab: Exposure / FPS / Gain
    auto* tab0 = new QWidget(this);
    auto* v0 = new QVBoxLayout(tab0);

    // Group: Camera Controls (exposure, fps, gain)

    auto* camGroup = new QGroupBox("General Camera Controls");
    auto* camLayout = new QVBoxLayout(this); camLayout->setContentsMargins(6,6,6,6);
    camGroup->setLayout(camLayout);
    
    // Exposure: slider from 1 to 200
    exposure_slider = new QSlider(Qt::Horizontal, this);
    exposure_slider->setRange(1, 200);
    exposure_slider->setValue(50);
    exposure_slider->setMaximumWidth(150);
    exposure_label = new QLabel("50", camGroup);
    exposure_label->setMaximumWidth(75);
    exposure_label->setMinimumWidth(75);
    connect(exposure_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        exposure_label->setText(QString::number(val) + " ms");
    });
    exposure_button = new QPushButton("Apply", camGroup);
    exposure_button->setProperty("primary", true);
    connect(exposure_button, &QPushButton::clicked, this, [this](){
        onSetExposure();
    });

    // Frame Rate: slider from 1 to 1000
    fps_slider = new QSlider(Qt::Horizontal, camGroup);
    fps_slider->setRange(1, 1000);
    fps_slider->setValue(30);
    fps_slider->setMaximumWidth(150);
    fps_label = new QLabel("30", camGroup);
    fps_label->setMaximumWidth(80);
    fps_label->setMinimumWidth(80);
    connect(fps_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        fps_label->setText(QString::number(val) + " fps");
    });
    fps_button  = new QPushButton("Apply", camGroup);
    fps_button->setProperty("primary", true);
    connect(fps_button, &QPushButton::clicked, this, [this](){
        onSetFps();
    });

    // Gain: slider from 0 to 7
    gain_slider = new QSlider(Qt::Horizontal, camGroup);
    gain_slider->setRange(0, 7);
    gain_slider->setValue(0);
    gain_slider->setMaximumWidth(100);
    gain_label = new QLabel("0", camGroup);
    gain_label->setMaximumWidth(60);
    gain_label->setMinimumWidth(60);
    connect(gain_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        gain_label->setText(QString::number(val) + " dB");
    });
    gain_button  = new QPushButton("Apply", camGroup);
    gain_button->setProperty("primary", true);
    connect(gain_button, &QPushButton::clicked, this, [this](){
        onSetGain();
    });

    // Build compact horizontal widgets for each camera control
    auto* exposureWidget = new QWidget(camGroup);
    auto* expLayout = new QHBoxLayout(exposureWidget); expLayout->setContentsMargins(0,0,0,0); expLayout->setSpacing(8);
    auto* expLabel = new QLabel("Exposure:", exposureWidget);
    expLabel->setMinimumWidth(80);
    expLabel->setMaximumWidth(80);
    expLayout->addWidget(expLabel, 0, Qt::AlignLeft);
    expLayout->addWidget(exposure_slider);
    expLayout->addWidget(exposure_label, 0, Qt::AlignLeft);
    expLayout->addWidget(exposure_button);

    auto* fpsWidget = new QWidget(camGroup);
    auto* fpsLayoutW = new QHBoxLayout(fpsWidget); fpsLayoutW->setContentsMargins(0,0,0,0); fpsLayoutW->setSpacing(8);
    auto* fpsLbl = new QLabel("FPS:", fpsWidget);
    fpsLbl->setMinimumWidth(80);
    fpsLbl->setMaximumWidth(80);
    fpsLayoutW->addWidget(fpsLbl, 0, Qt::AlignLeft);
    fpsLayoutW->addWidget(fps_slider);
    fpsLayoutW->addWidget(fps_label, 0, Qt::AlignLeft);
    fpsLayoutW->addWidget(fps_button);

    auto* gainWidget = new QWidget(camGroup);
    auto* gainLayoutW = new QHBoxLayout(gainWidget); gainLayoutW->setContentsMargins(0,0,0,0); gainLayoutW->setSpacing(8);
    auto* gainLbl = new QLabel("Gain:", gainWidget);
    gainLbl->setMaximumWidth(80);
    gainLbl->setMinimumWidth(80);
    gainLayoutW->addWidget(gainLbl, 0, Qt::AlignLeft);
    gainLayoutW->addWidget(gain_slider);
    gainLayoutW->addWidget(gain_label, 0, Qt::AlignLeft);
    gainLayoutW->addWidget(gain_button);

    leftTabWidget->addTab(tab0, QString("Controls"));

    camLayout->addWidget(exposureWidget);
    //camLayout->addSpacing(8);
    camLayout->addWidget(fpsWidget);
    camLayout->addWidget(gainWidget);

    // NEW!!
    // camGroup->setLayout(camLayout);
    v0->addWidget(camGroup);
    v0->addStretch();
    //v0->addLayout(camLayout);
    h1->addWidget(leftTabWidget);
    
    //root->addWidget(leftTabWidget);

    // Row: Video modes - convert previous buttons into a single dropdown embedded with other controls
    auto* tab1 = new QWidget(this);
    auto* v1 = new QVBoxLayout(tab1);

    // Edge Detect mode: behave like Grayscale but enable an edge-overlay in the viewer
    // Add a Video Mode dropdown next to existing controls so modes appear with other controls
    video_mode_combo = new QComboBox(tab1);
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
        // Disable edge button for incompatible modes (Segment, Object, Duplex)
        const bool isCompatible = isEdgeDetectCompatible(mode);
        edge_button->setEnabled(isCompatible);
        if (!isCompatible && edge_button->isChecked()) {
            edge_button->setChecked(false);
            emit edgeDetectToggled(false);
        }
    });

    // Group: Video Modes (dropdown + Edge Detect toggle)
    auto* videoGroup = new QGroupBox("Video Mode");
    auto* videoLayout = new QVBoxLayout(videoGroup); videoLayout->setContentsMargins(6,6,6,6);
    videoGroup->setLayout(videoLayout);
    videoLayout->addWidget(video_mode_combo);
    edge_button = new QPushButton("Edge Detect", videoGroup);
    edge_button->setCheckable(true);
    edge_button->setProperty("secondary", true);
    
    // Set initial state based on first item in combo (Segment mode)
    const int initialMode = video_mode_combo->itemData(0).toInt();
    edge_button->setEnabled(isEdgeDetectCompatible(initialMode));
    
    connect(edge_button, &QPushButton::toggled, this, [this](bool checked){
        if (checked) {
            if (!currentSerialValid()) { emit showWarning("No Camera", "No camera is currently selected."); edge_button->setChecked(false); return; }
        }
        emit edgeDetectToggled(checked);
    });
    edge_button->setToolTip("Enable edge overlay in viewer: Works on Grayscale, Precision, and MJPEG modes");

    leftTabWidget->addTab(tab1, QString("Video Modes"));
    videoLayout->addWidget(edge_button);
    v1->addWidget(videoGroup);
    v1->addStretch();

    // Tab: Color compression / gamma
    auto* tab2 = new QWidget(this);
    auto* v2   = new QVBoxLayout(tab2);

    // Group: Color Compression (quality, bitrate, mode dropdown)
    auto* compGroup = new QGroupBox("Color Compression and Gamma");
    auto* compLayout = new QVBoxLayout(compGroup); compLayout->setContentsMargins(6,6,6,6);
    compGroup->setLayout(compLayout);

    // Quality slider (0.0 - 1.0, scaled to 0-100 for slider)
    quality_slider = new QSlider(Qt::Horizontal, compGroup);
    quality_slider->setRange(0, 100);
    quality_slider->setValue(75);
    quality_slider->setMaximumWidth(120);
    quality_label = new QLabel("0.75", compGroup);
    quality_label->setMaximumWidth(50);
    quality_label->setMinimumWidth(50);
    connect(quality_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        quality_label->setText(QString::number(val / 100.0, 'f', 2));
    });

    // Bitrate slider (0 - 10000 Mbps)
    bitrate_slider = new QSlider(Qt::Horizontal, compGroup);
    bitrate_slider->setRange(0, 200);
    bitrate_slider->setValue(50);
    bitrate_slider->setMaximumWidth(120);
    bitrate_label = new QLabel("50.00", compGroup);
    bitrate_label->setMaximumWidth(60);
    bitrate_label->setMinimumWidth(60);
    connect(bitrate_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        bitrate_label->setText(QString::number(val / 100.0, 'f', 2));
    });

    mode_combo = new QComboBox(compGroup);
    mode_combo->addItem("Variable Bitrate", QVariant(0));
    mode_combo->addItem("Constant Bitrate", QVariant(1));

    set_compression_button = new QPushButton("Apply", compGroup);
    set_compression_button->setProperty("primary", true);
    connect(set_compression_button, &QPushButton::clicked, this, &CameraControlPanel::onSetCompression);

    // Build compression controls widget
    auto* compressionCtrlsWidget = new QWidget(compGroup);
    auto* compressionCtrlsLayout = new QVBoxLayout(compressionCtrlsWidget); compressionCtrlsLayout->setContentsMargins(0,0,0,0); compressionCtrlsLayout->setSpacing(8);
    auto* qualityLbl = new QLabel("Quality:", compressionCtrlsWidget);
    compressionCtrlsLayout->addWidget(qualityLbl, 0, Qt::AlignLeft);
    compressionCtrlsLayout->addWidget(quality_slider);
    compressionCtrlsLayout->addWidget(quality_label, 0, Qt::AlignLeft);

    auto* bitrateLbl = new QLabel("Bitrate:", compressionCtrlsWidget);
    compressionCtrlsLayout->addWidget(bitrateLbl, 0, Qt::AlignLeft);
    compressionCtrlsLayout->addWidget(bitrate_slider);
    compressionCtrlsLayout->addWidget(bitrate_label, 0, Qt::AlignLeft);

    compressionCtrlsLayout->addWidget(mode_combo);
    compressionCtrlsLayout->addWidget(set_compression_button);

    // Group: Color Compression (quality, bitrate, mode dropdown)
    auto* gammaGroup = new QGroupBox("Gamma");
    auto* gammaLayout = new QVBoxLayout(gammaGroup); gammaLayout->setContentsMargins(6,6,6,6);
    gammaGroup->setLayout(gammaLayout);

    // Gamma slider (0.1 - 1.0)
    gamma_slider = new QSlider(Qt::Horizontal, compGroup);
    gamma_slider->setRange(1, 10);
    gamma_slider->setValue(10);
    gamma_slider->setMaximumWidth(100);
    gamma_label = new QLabel("1.0", compGroup);
    gamma_label->setMaximumWidth(40);
    gamma_label->setMinimumWidth(40);
    connect(gamma_slider, QOverload<int>::of(&QSlider::valueChanged), this, [this](int val){
        gamma_label->setText(QString::number(val / 10.0, 'f', 1));
    });

    gamma_button = new QPushButton("Apply", compGroup);
    gamma_button->setProperty("primary", true);
    connect(gamma_button, &QPushButton::clicked, this, &CameraControlPanel::onSetGamma);

    auto* gammaCtrlsWidget = new QWidget(compGroup);
    auto* gammaCtrlsLayout = new QVBoxLayout(gammaCtrlsWidget); gammaCtrlsLayout->setContentsMargins(0,0,0,0); gammaCtrlsLayout->setSpacing(8);
    auto* gammaLbl = new QLabel("Gamma:", gammaCtrlsWidget);
    gammaCtrlsLayout->addWidget(gammaLbl, 0, Qt::AlignLeft);
    gammaCtrlsLayout->addWidget(gamma_slider);
    gammaCtrlsLayout->addWidget(gamma_label, 0, Qt::AlignLeft);
    gammaCtrlsLayout->addWidget(gamma_button);

    compLayout->addWidget(compressionCtrlsWidget);
    gammaLayout->addWidget(gammaCtrlsWidget);

    leftTabWidget->addTab(tab2, QString("Color"));
    v2->addWidget(compGroup);
    v2->addWidget(gammaGroup);
    v2->addStretch();

	// Tab for statistics graphs with MetricController integration
    auto* tabStats = new QWidget(this);
    auto* vStats = new QVBoxLayout(tabStats);

    // Create Focus Metrics with controller
    QVector<QString> focusLabels = {"FocusQuality"};
    QVector<QString> focusDescriptions = {""};
    QVector<bool> focusGraphs = {true};
    MetricWidgets* focusMetrics = createMetricWidgets("Focus Statistics", "", focusLabels, focusDescriptions, focusGraphs);
    focusMetricsController = new MetricController(focusMetrics);
    vStats->addWidget(focusMetrics->groupBox);

    // Create Lens Metrics with controller
    QVector<QString> lensLabels = {"LensValidation"};
    QVector<QString> lensDescriptions = {""};
    QVector<bool> lensGraphs = {true};
    MetricWidgets* lensMetrics = createMetricWidgets("Lens Statistics", "", lensLabels, lensDescriptions, lensGraphs);
    lensMetricsController = new MetricController(lensMetrics);
    vStats->addWidget(lensMetrics->groupBox);
    vStats->addStretch();

    leftTabWidget->addTab(tabStats, "Statistics");

    root->addWidget(leftTabWidget);
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

void CameraControlPanel::onSetTab0Visibility() {
    // check if first tab in widget is visible
    if (this->leftTabWidget->isTabVisible(0)) {
        this->leftTabWidget->setTabVisible(0, false);
        if (!(this->leftTabWidget->isTabVisible(1)) && !(this->leftTabWidget->isTabVisible(2) ) && !(this->leftTabWidget->isTabVisible(3))) {
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
        if (!(this->leftTabWidget->isTabVisible(0)) && !(this->leftTabWidget->isTabVisible(2)) && !(this->leftTabWidget->isTabVisible(3))) {
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
        if (!(this->leftTabWidget->isTabVisible(0)) && !(this->leftTabWidget->isTabVisible(1)) && !(this->leftTabWidget->isTabVisible(3))) {
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
        if (!(this->leftTabWidget->isTabVisible(0)) && !(this->leftTabWidget->isTabVisible(1)) && !(this->leftTabWidget->isTabVisible(2))) {
            this->leftTabWidget->setVisible(false);
        }
    }
    else {
        this->leftTabWidget->setTabVisible(3, true);
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