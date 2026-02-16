#include "QtCameraViewer.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTimer>
#include <QStackedLayout>
#include <QStyleFactory>
#include <QPushButton>
#include <QScreen>
#include <QDateTime>
#include <QLineEdit>
#include <QFileDialog>
#include <QCheckBox>

#include "./widgets/graphwidget.h"
#include "QtCameraConnectionManager.h"
#include "QtCameraPicker.h"
#include "QtCameraControlPanel.h"
#include "QtVideoWidget.h"
#include "CameraHelpers.h"
#include "MetricsExporter.h"

// Main Collection of Widgets and layouts for the application

using namespace CameraLibrary;

void QtCameraViewer::ApplyAppStyle()
{
	QApplication::setStyle(QStyleFactory::create("Fusion"));
	QPalette dark;
    dark.setColor(QPalette::Window,          QColor(53,53,53));
    dark.setColor(QPalette::WindowText,      Qt::white);
    dark.setColor(QPalette::Base,            QColor(35,35,35));
    dark.setColor(QPalette::AlternateBase,   QColor(53,53,53));
    dark.setColor(QPalette::ToolTipBase,     Qt::white);
    dark.setColor(QPalette::ToolTipText,     Qt::white);
    dark.setColor(QPalette::Text,            Qt::white);
    dark.setColor(QPalette::Button,          QColor(53,53,53));
    dark.setColor(QPalette::ButtonText,      Qt::white);
    dark.setColor(QPalette::BrightText,      Qt::red);
    dark.setColor(QPalette::Link,            QColor(42,130,218));
    dark.setColor(QPalette::Highlight,       QColor(42,130,218));
	dark.setColor(QPalette::HighlightedText, Qt::black);
    dark.setColor(QPalette::PlaceholderText, QColor(160,160,160));
	QApplication::setPalette(dark);
}

QtCameraViewer::QtCameraViewer(CameraConnectionManager* mgr,
	std::mutex& camMutex,
	std::shared_ptr<Camera>& currentCamera,
	std::atomic<uint64_t>& switchEpoch,
	std::atomic<unsigned>& activeSerial,
	CameraHelper::FrameRateCalculator& fpsCalc,
	DisplayResults* newText,
	MetricsExporter& metricsExporter,
	QWidget* parent)
	: QWidget(parent)
	, camera_manager(mgr)
	, camera_mutex(camMutex)
	, current_camera(currentCamera)
	, switch_epoch(switchEpoch)
	, active_serial(activeSerial)
	, fps_calculator(fpsCalc)
	, focus_result(newText)
	, metrics_exporter(metricsExporter)
{
	buildUi();
	wireSignals();
}

void QtCameraViewer::buildUi()
{
	auto* mainLayout = new QVBoxLayout(this);
	auto* v = new QVBoxLayout(this);
	auto* h2 = new QHBoxLayout(this);

	// Row 1: Camera picker
	camera_picker = new CameraPicker(camera_manager, this);
	v->addWidget(camera_picker);

	// controls panel that later comes in Row 5
	camera_controls = new CameraControlPanel(camera_manager, metrics_exporter, this);

	// Row 2: Status bar with FPS
	status_bar = new QWidget(this);
	auto* sh = new QHBoxLayout(status_bar);
    sh->setContentsMargins(6,0,6,0);
	fps_label = new QLabel("FPS: —", status_bar);
	fps_label->setStyleSheet("color:#ddd; font-weight:600;");
	sh->addWidget(fps_label);
	sh->addStretch(1);

	v->addWidget(status_bar);

	// Row 3: Another status bar, this time with focus eval results
	second_status_bar = new QWidget(this);
	auto* second_box = new QHBoxLayout(second_status_bar);
    second_box->setContentsMargins(6,0,6,0);
	focus_result_label = new QLabel("Focus Result:", second_status_bar);
	focus_result_label->setStyleSheet("color:#ddd; font-weight:600;");
	focus_result->setStyleSheet("color:CadetBlue; font-weight:600;");
	second_box->addWidget(focus_result_label);
	second_box->addWidget(focus_result);
	second_box->addStretch(1);

	v->addWidget(second_status_bar);

	auto* fpsTimer = new QTimer(this);
	fpsTimer->setInterval(500);
	connect(fpsTimer, &QTimer::timeout, this, [this]() {
		fps_label->setText(QString("FPS: %1").arg(fps_calculator.current(), 0, 'f', 1));
		});
	fpsTimer->start();

	// Row 4: Third status bar, this time holding toggle buttons for the tabs' visibility
	third_status_bar = new QWidget(this);
	auto* third_box = new QHBoxLayout(third_status_bar);
	third_box->setContentsMargins(6, 0, 6, 0);
	auto* toggle_label = new QLabel("Toggle Tabs:", third_status_bar);
	auto* tab0_visibility_button = new QPushButton("Control Tab", third_status_bar);

	// tab0_visibility_button->setMaximumSize(50, 50);
	connect(tab0_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab0Visibility);
	auto* tab1_visibility_button = new QPushButton("Video Modes Tab", third_status_bar);
	connect(tab1_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab1Visibility);
	auto* tab2_visibility_button = new QPushButton("Color Tab", third_status_bar);
	connect(tab2_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab2Visibility);
	auto* tab3_visibility_button = new QPushButton("Statistics", third_status_bar);
	connect(tab3_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab3Visibility);
	auto* tab4_visibility_button = new QPushButton("Exporter Tab", third_status_bar);
    connect(tab4_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab4Visibility);

	third_box->addWidget(toggle_label);
	third_box->addWidget(tab0_visibility_button);
	third_box->addWidget(tab1_visibility_button);
	third_box->addWidget(tab2_visibility_button);
	third_box->addWidget(tab3_visibility_button);
	third_box->addWidget(tab4_visibility_button);
	third_box->addStretch(1);

	v->addWidget(third_status_bar);

	// Row 5: Another status bar, this time with focus eval results
	// only add camera_controls after all of the other things (camera picker, etc.)


	// Center stacked layout
	center_widget = new QWidget(this);
	stacked_layout = new QStackedLayout(center_widget);

	// Empty pane
	empty_pane = new QWidget(center_widget);
	auto* emptyLayout = new QVBoxLayout(empty_pane);
	emptyLayout->setAlignment(Qt::AlignCenter);
	auto* emptyLabel = new QLabel("No Cameras Connected", empty_pane);
	QFont f = emptyLabel->font(); f.setPointSize(f.pointSize() + 6); f.setBold(true);
	emptyLabel->setFont(f);
	emptyLabel->setAlignment(Qt::AlignCenter);
	emptyLayout->addWidget(emptyLabel);

	// Video pane
	gl_viewer_window = new VideoWidget();
	viewer_container = QWidget::createWindowContainer(gl_viewer_window, center_widget);
	viewer_container->setFocusPolicy(Qt::StrongFocus);

	// Set the video widget in the control panel so it can capture it for screenshots
	camera_controls->setVideoWidget(gl_viewer_window);

	stacked_layout->addWidget(empty_pane);
	stacked_layout->addWidget(viewer_container);
	setEmptyState(camera_picker->combo() && camera_picker->combo()->count() > 0);

	// video pane on left, camera controls and stats on right
	h2->addWidget(center_widget, 1);
	h2->addWidget(camera_controls);

	mainLayout->addLayout(v);
	mainLayout->addLayout(h2);
}

void QtCameraViewer::wireSignals()
{
	// Empty-state follows camera presence
	connect(camera_picker, &CameraPicker::camerasPresentChanged,
		this, &QtCameraViewer::setEmptyState);

	// Selection changes update shared state and control panel
	connect(camera_picker, &CameraPicker::serialChanged,
		this, &QtCameraViewer::handleSerialSelected);

	// Forward Edge Detect toggle from control panel to the video widget
	if (camera_controls) {
		connect(camera_controls, &CameraControlPanel::edgeDetectToggled,
			this, [this](bool enabled) { if (gl_viewer_window) gl_viewer_window->setEdgeDetectEnabled(enabled); });
		connect(camera_controls, &CameraControlPanel::onMarkerZoomToggled,
			this, [this](bool enabled) { if (gl_viewer_window) gl_viewer_window->setRoiZoomEnabled(enabled); });

	}
}

void QtCameraViewer::setEmptyState(bool anyCamerasPresent)
{
	stacked_layout->setCurrentWidget(anyCamerasPresent ? viewer_container : empty_pane);
}

void QtCameraViewer::handleSerialSelected(std::optional<unsigned> serialOpt)
{
	if (!serialOpt) {
		std::lock_guard<std::mutex> lk(camera_mutex);
		current_camera.reset();
		camera_controls->setSelectedSerial(0);
		setEmptyState(false);
		fps_calculator.reset();
		return;
	}

	const auto serial = static_cast<qulonglong>(*serialOpt);
	active_serial.store(static_cast<unsigned>(serial), std::memory_order_release);
	switch_epoch.fetch_add(1, std::memory_order_acq_rel);

	auto cams = camera_manager->GetCameras();
	for (auto& c : cams) {
		if (static_cast<qulonglong>(c->Serial()) == serial) {
			c->SetTextOverlay(false);
			{
				std::lock_guard<std::mutex> lk(camera_mutex);
				current_camera = c;
				active_serial.store(c->Serial(), std::memory_order_release);
			}

			// Tell the controls which serial to drive
			camera_controls->setSelectedSerial(static_cast<unsigned>(serial));

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
			setEmptyState(true);
			break;
		}
	}
}

void QtCameraViewer::onSetFocusHUDVisibility(bool toggle) {
    if (this->second_status_bar) {
        this->second_status_bar->setVisible(toggle);
    }
}
