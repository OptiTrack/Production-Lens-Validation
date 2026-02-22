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
#include <QCoreApplication>

#include "./widgets/graphwidget.h"
#include "QtCameraConnectionManager.h"
#include "QtCameraPicker.h"
#include "QtCameraControlPanel.h"
#include "QtVideoWidget.h"
#include "CameraHelpers.h"
#include "MetricsManager.h"

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
	FocusResultLabel* focusResult,
	LensResultLabel* lensResult,
	MetricsManager& MetricsManager,
	QWidget* parent)
	: QWidget(parent)
	, camera_manager(mgr)
	, camera_mutex(camMutex)
	, current_camera(currentCamera)
	, switch_epoch(switchEpoch)
	, active_serial(activeSerial)
	, fps_calculator(fpsCalc)
	, focus_result(focusResult)
	, lens_result(lensResult)
	, metrics_manager(MetricsManager)
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

	// Controls panel that later comes in Row 5
	camera_controls = new CameraControlPanel(camera_manager, metrics_manager, this);

	// Row 2: Status bar with FPS
	fps_bar = new QWidget(this);
	auto* sh = new QHBoxLayout(fps_bar);
    sh->setContentsMargins(6,0,6,0);
	fps_label = new QLabel("FPS: —", fps_bar);
	fps_label->setStyleSheet("color:#ddd; font-weight:600;");
	sh->addWidget(fps_label);
	sh->addStretch(1);

	language_label = new QLabel(fps_bar);
	language_combo = new QComboBox(fps_bar);
	language_combo->addItem(QStringLiteral("English"), QStringLiteral("en"));
	language_combo->addItem(QStringLiteral("Simplified Chinese"), QStringLiteral("zh_CN"));
	connect(language_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),

		this, [this](int idx) {
			if (!language_combo || idx < 0) return;

			emit languageChanged(language_combo->itemData(idx).toString());
		});

	sh->addWidget(language_label);
	sh->addWidget(language_combo);

	v->addWidget(status_bar);

	// Row 3: Another status bar, this time with focus eval results
	second_status_bar = new QWidget(this);
	auto* second_box = new QHBoxLayout(second_status_bar);
	second_box->setContentsMargins(6, 0, 6, 0);

	focus_result_label = new QLabel("Focus Result:", second_status_bar);
	focus_result_label->setStyleSheet("color:#ddd; font-weight:600;");
	focus_result_label->setMinimumWidth(80);

	lens_result_label = new QLabel("Lens Grade:", second_status_bar);
	lens_result_label->setStyleSheet("color:#ddd; font-weight:600;");
	lens_result_label->setMinimumWidth(80);

	focus_result->setStyleSheet("color:CadetBlue; font-weight:600;");
	focus_result->setMinimumWidth(200); 
	focus_result->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

	lens_result->setStyleSheet("color:CadetBlue; font-weight:600;");
	lens_result->setMinimumWidth(80);
	lens_result->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

	second_box->addWidget(focus_result_label);
	second_box->addWidget(focus_result);
	
	second_box->addWidget(lens_result_label);
	second_box->addWidget(lens_result);
	second_box->addStretch(1);

	v->addWidget(second_status_bar);
	v->addWidget(fps_bar);

	auto* fpsTimer = new QTimer(this);
	fpsTimer->setInterval(500);
	connect(fpsTimer, &QTimer::timeout, this, [this]() {
		const QString fmt = fps_format.isEmpty()
			? QStringLiteral("FPS: %1")
			: fps_format;
		fps_label->setText(fmt.arg(fps_calculator.current(), 0, 'f', 1));
		});
	fpsTimer->start();

    // Row 3: Another status bar, this time with focus eval result (red, green, yellow)
    focus_result_bar = new QWidget(this);
    auto* second_box = new QHBoxLayout(focus_result_bar);
    second_box->setContentsMargins(6,0,6,0);
    focus_result_label = new QLabel("Focus Result:", focus_result_bar);
    focus_result_label->setStyleSheet("color:#ddd; font-weight:600;");
    focus_result->setStyleSheet("color:CadetBlue; font-weight:600;");
    second_box->addWidget(focus_result_label);
    second_box->addWidget(focus_result);
    second_box->addStretch(1);

    v->addWidget(focus_result_bar);

    // Row 4: Another status bar, this time with focus eval score (actual number)
    focus_score_bar = new QWidget(this);
    auto* third_box = new QHBoxLayout(focus_score_bar);
    third_box->setContentsMargins(6,0,6,0);
    focus_score_label = new QLabel("Focus Score:", focus_score_bar);
    focus_score_label->setStyleSheet("color:#ddd; font-weight:600;");
    focus_score_display = new QLabel(focus_score_bar);
    focus_score_display->setText(QString::number(focus_score));
    focus_score_display->setStyleSheet("color:CadetBlue; font-weight:600;");
    third_box->addWidget(focus_score_label);
    third_box->addWidget(focus_score_display);
    third_box->addStretch(1);

    v->addWidget(focus_score_bar);

	// change visibility of focus tool HUD
    connect(camera_controls, &CameraControlPanel::focusHUDToggled, this, &QtCameraViewer::onSetFocusHUDVisibility);

	// Row 5: Contains toggle buttons for the tabs' visibility
    toggle_tabs_bar = new QWidget(this);
    auto* toggle_tabs_box = new QHBoxLayout(toggle_tabs_bar);
    toggle_tabs_box->setContentsMargins(6,0,6,0);
    auto* toggle_label = new QLabel("Toggle Tabs:", toggle_tabs_bar);
    auto* tab0_visibility_button = new QPushButton("Controls", toggle_tabs_bar);
    // tab0_visibility_button->setMaximumSize(50, 50);
    connect(tab0_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab0Visibility);
    auto* tab1_visibility_button = new QPushButton("Lens", toggle_tabs_bar);
    connect(tab1_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab1Visibility);
    auto* tab2_visibility_button = new QPushButton("Quality", toggle_tabs_bar);
    connect(tab2_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab2Visibility);
    auto* tab3_visibility_button = new QPushButton("Statistics", toggle_tabs_bar);
    connect(tab3_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab3Visibility);
	auto* tab4_visibility_button = new QPushButton("Exporter", toggle_tabs_bar);
    connect(tab4_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab4Visibility);

    toggle_tabs_box->addWidget(toggle_label);
    toggle_tabs_box->addWidget(tab0_visibility_button);
    toggle_tabs_box->addWidget(tab1_visibility_button);
    toggle_tabs_box->addWidget(tab2_visibility_button);
    toggle_tabs_box->addWidget(tab3_visibility_button);
	toggle_tabs_box->addWidget(tab4_visibility_button);
    toggle_tabs_box->addStretch(1);

	
	v->addWidget(toggle_tabs_bar);


	// only add camera_controls after all of the other things (camera picker, etc.)


	// Center stacked layout
	center_widget = new QWidget(this);
	stacked_layout = new QStackedLayout(center_widget);

	// Empty pane
	empty_pane = new QWidget(center_widget);
	auto* emptyLayout = new QVBoxLayout(empty_pane);
	emptyLayout->setAlignment(Qt::AlignCenter);
	empty_label = new QLabel(empty_pane);
	QFont f = empty_label->font(); f.setPointSize(f.pointSize() + 6); f.setBold(true);
	empty_label->setFont(f);
	empty_label->setAlignment(Qt::AlignCenter);
	emptyLayout->addWidget(empty_label);

	// Video pane
	gl_viewer_window = new VideoWidget();
	gl_viewer_window->setNewZoomValue(1.f);

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

	retranslateUi();
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

void QtCameraViewer::setViewerZoomValue(float val) {
	if (gl_viewer_window) {
		gl_viewer_window->setNewZoomValue(val);
	}
}


void QtCameraViewer::onSetFocusHUDVisibility(bool toggle) {
    this->focus_result_bar->setVisible(toggle);
    this->focus_score_bar->setVisible(toggle);
}

void QtCameraViewer::retranslateUi()
{
	fps_format = QCoreApplication::translate("QtCameraViewer", "FPS: %1");
	if (fps_label) {
		fps_label->setText(QCoreApplication::translate("QtCameraViewer", "FPS: —"));
	}
	if (focus_result_label) {
		focus_result_label->setText(QCoreApplication::translate("QtCameraViewer", "Focus Result:"));
	}
	if (lens_result_label) {
		lens_result_label->setText(QCoreApplication::translate("QtCameraViewer", "Lens Grade:"));
	}
	if (toggle_label) {
		toggle_label->setText(QCoreApplication::translate("QtCameraViewer", "Toggle Tabs:"));
	}
	if (tab0_visibility_button) {
		tab0_visibility_button->setText(QCoreApplication::translate("QtCameraViewer", "Control Tab"));
	}
	if (tab1_visibility_button) {
		tab1_visibility_button->setText(QCoreApplication::translate("QtCameraViewer", "Video Modes Tab"));
	}
	if (tab2_visibility_button) {
		tab2_visibility_button->setText(QCoreApplication::translate("QtCameraViewer", "Color Tab"));
	}
	if (tab3_visibility_button) {
		tab3_visibility_button->setText(QCoreApplication::translate("QtCameraViewer", "Statistics"));
	}
	if (tab4_visibility_button) {
		tab4_visibility_button->setText(QCoreApplication::translate("QtCameraViewer", "Exporter Tab"));
	}
	if (empty_label) {
		empty_label->setText(QCoreApplication::translate("QtCameraViewer", "No Cameras Connected"));
	}
	if (language_label) {
		language_label->setText(QCoreApplication::translate("QtCameraViewer", "Language:"));
	}
	if (language_combo && language_combo->count() >= 2) {
		language_combo->setItemText(0, QCoreApplication::translate("QtCameraViewer", "English"));
		language_combo->setItemText(1, QCoreApplication::translate("QtCameraViewer", "Simplified Chinese"));
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

QString QtCameraViewer::currentLanguage() const
{
	if (!language_combo || language_combo->currentIndex() < 0) {
		return QStringLiteral("en");
	}
	return language_combo->itemData(language_combo->currentIndex()).toString();
}
