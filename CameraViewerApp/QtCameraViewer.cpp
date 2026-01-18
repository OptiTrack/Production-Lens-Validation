#include "QtCameraViewer.h"

#include <QApplication>
#include <QVBoxLayout>
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
                               std::atomic<unsigned>&  activeSerial,
                               CameraHelper::FrameRateCalculator& fpsCalc,
                               DisplayResults* newText,
                               QWidget* parent)
    : QWidget(parent)
    , camera_manager(mgr)
    , camera_mutex(camMutex)
    , current_camera(currentCamera)
    , switch_epoch(switchEpoch)
    , active_serial(activeSerial)
    , fps_calculator(fpsCalc)
    , focus_result(newText)
{
    buildUi();
    wireSignals();
}

void QtCameraViewer::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    auto* focus_and_scrnsht_layout = new QHBoxLayout(this);
    // used for the FPS and focus result displays (left side)
    auto* vl = new QVBoxLayout(this);
    // used for the screenshot tools (right side)
    auto* vr = new QVBoxLayout(this);
    // for the viewing pane (left) and camera_controls (right)
    auto* h2 = new QHBoxLayout(this);


    // Left-Hand Side ------------------------------------------
    // Row 1: Camera picker
    camera_picker = new CameraPicker(camera_manager, this);
    mainLayout->addWidget(camera_picker);
    mainLayout->addLayout(focus_and_scrnsht_layout);

    // controls panel that comes later
    camera_controls = new CameraControlPanel(camera_manager, this);

    // Row 2: FPS
    fps_bar = new QWidget(this);
    auto* sh = new QHBoxLayout(fps_bar);
    sh->setContentsMargins(6,0,6,0);
    fps_label = new QLabel("FPS: —", fps_bar);
    fps_label->setStyleSheet("color:#ddd; font-weight:600;");
    sh->addWidget(fps_label);
    sh->addStretch(1);

    vl->addWidget(fps_bar);

    auto* fpsTimer = new QTimer(this);
    fpsTimer->setInterval(500);
    connect(fpsTimer, &QTimer::timeout, this, [this](){
        fps_label->setText(QString("FPS: %1").arg(fps_calculator.current(), 0, 'f', 1));
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

    vl->addWidget(focus_result_bar);

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

    vl->addWidget(focus_score_bar);

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
    auto* tab1_visibility_button = new QPushButton("Video Modes", toggle_tabs_bar);
    connect(tab1_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab1Visibility);
    auto* tab2_visibility_button = new QPushButton("Color", toggle_tabs_bar);
    connect(tab2_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab2Visibility);
    auto* tab3_visibility_button = new QPushButton("Statistics", toggle_tabs_bar);
    connect(tab3_visibility_button, &QPushButton::clicked, camera_controls, &CameraControlPanel::onSetTab3Visibility);

    toggle_tabs_box->addWidget(toggle_label);
    toggle_tabs_box->addWidget(tab0_visibility_button);
    toggle_tabs_box->addWidget(tab1_visibility_button);
    toggle_tabs_box->addWidget(tab2_visibility_button);
    toggle_tabs_box->addWidget(tab3_visibility_button);
    toggle_tabs_box->addStretch(1);

    vl->addWidget(toggle_tabs_bar);
    focus_and_scrnsht_layout->addLayout(vl);
    focus_and_scrnsht_layout->addStretch(1);
    

    // Right-Hand Side ------------------------------------------
    vr->setAlignment(Qt::AlignRight);

    // Screenshot directory label and browse button on the right of row 2
    browse_bar = new QWidget(this);
    auto* browse_box = new QHBoxLayout(browse_bar);
    browse_box->setAlignment(Qt::AlignRight);
    browse_box->setContentsMargins(6,0,6,0);
    browse_label = new QLabel("Screenshot Dir:" + screenshotDirectory, browse_bar);
    browse_box->addWidget(browse_label);
    QPushButton* browse_button = new QPushButton(browse_bar);
    browse_button->setText("Browse...");
    browse_box->addWidget(browse_button);

    connect(browse_button, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this,
            "Select Screenshot Directory",
            screenshotDirectory
        );

        if (!dir.isEmpty())
            screenshotDirectory = dir;
            browse_label->setText("Screenshot Dir: " + screenshotDirectory);
    });

    vr->addWidget(browse_bar);

    // Add Screenshot button to the right of row 3
    screenshot_bar = new QWidget(this);
    auto* screenshot_box = new QHBoxLayout(screenshot_bar);
    screenshot_box->setAlignment(Qt::AlignRight);
    screenshot_box->setContentsMargins(6,0,6,0);
    QPushButton* screenshot_button = new QPushButton(screenshot_bar);
    // Add a text box to input lens serial number to the left of the button
    serial_input = new QLineEdit(screenshot_bar);
    serial_input->setPlaceholderText("Serial #");
    serial_input->setMaximumWidth(100);
    screenshot_box->addWidget(serial_input);
    screenshot_button->setText("Screenshot");
    screenshot_button->setToolTip("Take Screenshot");
    screenshot_button->setMaximumWidth(100);
    screenshot_box->addWidget(screenshot_button);
    screenshot_button->setProperty("primary", true);
    connect(screenshot_button, &QPushButton::clicked, this, [this]() {
        takeScreenshot();
        emit camera_controls->showWarning("Screenshot", "Screenshot saved!");
    });

    vr->addWidget(screenshot_bar);

    // Overlay enable/disable checkbox far right of row 4
    overlay_bar = new QWidget(this);
    auto* overlay_box = new QHBoxLayout(overlay_bar);
    overlay_box->setAlignment(Qt::AlignRight);
    overlay_box->setContentsMargins(6,0,6,0);
    overlay_button = new QCheckBox(overlay_bar);
    overlay_button->setText("Overlay Enabled");
    overlay_box->addWidget(overlay_button);
    overlay_button->setChecked(true);
    connect(overlay_button, &QCheckBox::clicked, this, [this]() {
    overlayState = !overlayState;
    if (overlayState)
        overlay_button->setText("Overlay Enabled");
    else
        overlay_button->setText("Overlay Disabled" );
    });

    vr->addWidget(overlay_bar);
    focus_and_scrnsht_layout->addLayout(vr);


    // only add camera_controls after all of the other things (camera picker, etc.)


    // Center stacked layout
    center_widget = new QWidget(this);
    stacked_layout  = new QStackedLayout(center_widget);

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

    stacked_layout->addWidget(empty_pane);
    stacked_layout->addWidget(viewer_container);
    setEmptyState(camera_picker->combo() && camera_picker->combo()->count() > 0);

    // video pane on left, camera controls and stats on right
    h2->addWidget(center_widget, 1);
    h2->addWidget(camera_controls);

    //mainLayout->addLayout(v);
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
                this, [this](bool enabled){ if (gl_viewer_window) gl_viewer_window->setEdgeDetectEnabled(enabled); });
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
// Take the screen shot and save to file
void QtCameraViewer::takeScreenshot()
{
    // Check if loaded screen
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    // Add Serial number of lens if possible, else put #
    QString serial = serial_input && !serial_input->text().isEmpty() ? serial_input->text(): "#";
    // Get the window image
    QPixmap pix;
    if (overlayState)
        pix = screen->grabWindow(this->winId());
    else
        pix = screen->grabWindow(gl_viewer_window->winId());
    // Assign the time and day, with the serial number for file name
    QString filename = QDateTime::currentDateTime().toString("'screenshot_%1_'yyyyMMdd_HHmmss'.png'").arg(serial);
    // File location selection
    if (screenshotDirectory.isEmpty()){
        pix.save(filename);
    }else{
        QString fileLocation = QDir(screenshotDirectory).filePath(filename);
        pix.save(fileLocation);
    }

}

void QtCameraViewer::onSetFocusHUDVisibility(bool toggle) {
    this->focus_result_bar->setVisible(toggle);
    this->focus_score_bar->setVisible(toggle);
}
