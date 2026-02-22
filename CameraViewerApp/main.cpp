#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <memory>
#include <cstdlib>
#include <QFuture>
#include <QApplication>
#include <QLabel>
#include <QDateTime>
#include <qfile.h>

// For chinese translation support
#include <QTranslator>
#include <QLibraryInfo>

#include <QCoreApplication>
#include "QtCameraConnectionManager.h"
#include "QtVideoWidget.h"
#include "QtCameraViewer.h"
#include "QtCameraControlPanel.h"
#include "metricscontroller.h"
#include "CameraHelpers.h"
#include "BitmapPool.h"
#include "FocusEval.h"
#include "MetricsManager.h"

#ifdef HAVE_FFMPEG
#include "videodecoder.h"
#endif

#include <qfuture.h>
#include <qlogging.h>
#include <QtConcurrent/qtconcurrentrun.h>
#include "FocusResultLabel.h"
#include "LensResultLabel.h"
#include <qobject.h>

using namespace CameraLibrary;

int main(int argc, char *argv[])
{
    // Guard to execute the cameralibrary shutdown on exit
    struct ShutdownGuard {
        std::atomic_bool done{false};
        std::thread*     captureThread{nullptr};
        std::atomic_bool* runningFlag{nullptr};
        CameraConnectionManager* mgr{nullptr};

        void finalize() {
            bool expected = false;
            if (!done.compare_exchange_strong(expected, true)) {
                return;
            }

            if (runningFlag) {
                runningFlag->store(false, std::memory_order_release);
            }

            if (captureThread && captureThread->joinable()) {
                captureThread->join();
            }

            CameraManager::X().Shutdown();
        }
        ~ShutdownGuard() { finalize(); }
    } guard;

    QApplication app(argc, argv);

    // Declare a QTranslator to load translations for the application and Qt's built-in strings
    // This will load app_en.ts and app_zh_CN.ts from the i18n directory, as well as the corresponding Qt translations if locale is Chinese
    // appTranslator and qtTranslator are declared here to ensure they remain in scope for the duration of the application, as QTranslator must not be destroyed while installed in the application


    // for our application's custom translations
    QTranslator appTranslator; 

    // for Qt's built-in translations (e.g. file dialog buttons)
    QTranslator qtTranslator; 

    QFile file("motive.css");

    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = file.readAll();
        app.setStyleSheet(styleSheet);
        file.close();
    }

    else 
        qWarning() << "[!] Could not open stylesheet file.";

    // ==== Camera manager ========================================================
    auto* mgr = new CameraConnectionManager(); 

    // ==== Shared state for capture thread ======================================
    std::mutex cam_mutex;
    std::shared_ptr<Camera> current_camera;
    std::atomic<uint64_t> switch_epoch{0};
    std::atomic<unsigned>  active_serial{0};

    CameraHelper::FrameRateCalculator fps_calculator{0.5 /*smoothing*/ };

    FocusResultLabel* focus_result = new FocusResultLabel("Disabled");
    LensResultLabel* lens_result = new LensResultLabel("Unknown");

    MetricsManager mMgr;

    // The core UI/window for the program
    auto* viewer = new QtCameraViewer(mgr, cam_mutex, current_camera, switch_epoch, active_serial,
                                      fps_calculator, focus_result, lens_result, mMgr, nullptr);

   
    // Remove any current installed translators to ensure a clean slate, then install the appropriate ones based on the current locale
    // use [&] to capture the app and translators by reference so they remain in scope and can be modified inside the lambda
    auto applyLanguage = [&](const QString& localeName) {
        app.removeTranslator(&appTranslator);
        app.removeTranslator(&qtTranslator);

        const QString i18nDir = QCoreApplication::applicationDirPath() + "/i18n";
        const QString qtTrDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);

        if (localeName == QLatin1String("zh_CN")) {

            if (!appTranslator.load("app_zh_CN", i18nDir))
                qWarning() << "[i18n] Failed to load app_zh_CN from" << i18nDir;
            
            else
                app.installTranslator(&appTranslator);

            // Try qtbase first, then qt (covers more Qt6 installs)
            QLocale loc(QLocale::Chinese, QLocale::China);

            bool ok = qtTranslator.load(loc, "qtbase", "_", qtTrDir)
                || qtTranslator.load(loc, "qt", "_", qtTrDir);

            if (!ok)
                qWarning() << "[i18n] Failed to load Qt zh_CN translations from" << qtTrDir;

            else 
                app.installTranslator(&qtTranslator);
        }

        if (viewer) 
            viewer->retranslateUi();
    };


    QObject::connect(viewer, &QtCameraViewer::languageChanged,
        &app, [applyLanguage](const QString& locale) { applyLanguage(locale); });

    applyLanguage(viewer->currentLanguage());

    // get instance to camera control panel for metrics updates
    auto* panel = viewer->getControlPanel();

    // set up shared metrics object and make Qt signal connection
    QObject::connect(panel, &CameraControlPanel::exportMetricsRequested,
        [&mMgr]() {
            mMgr.ExportMetrics();
        });

    viewer->resize(1100, 600);
    viewer->show();
    viewer->focus_score = 0;

    // Bitmap resource
    BitmapPool bmp_pool([](int w, int h, int bpp, int stride) -> Bitmap* {
        auto fmt = (bpp == 8) ? Bitmap::EightBit
                : (bpp == 16) ? Bitmap::SixteenBit
                : (bpp == 24) ? Bitmap::TwentyFourBit
                            : Bitmap::ThirtyTwoBit;
        return new Bitmap(w, h, stride, fmt);
    });

    // ==== Main Application Thread ===============================================
    std::atomic_bool running(true);
    std::atomic_bool focusToolEnabled(true); // changed by focus UI control
    std::atomic_bool circleDetectionEnabled(false); // changed by circle detection UI control
    //std::atomic_bool focusToolEnabled(true); // changed by focus UI control; set True by default

    FocusEvaluator fe;
    fe.focusToolEnabled = true; // changed by focus UI control; set True by default
    const int focusEvalFrameGap = 10;
    int frameCount = 0;

    // Wire UI signals after creating evaluator and panel
    if (panel) {
        QObject::connect(panel, &CameraControlPanel::circleDetectionToggled, &app, [&](bool enabled){
            circleDetectionEnabled.store(enabled, std::memory_order_release);
        });

        QObject::connect(panel, &CameraControlPanel::circleParam2Changed, &app, [&](double param2){
            try {
                auto det = fe.GetCircleDetector();
                if (det) {
                    auto params = det->GetDetectionParams();
                    params.param2 = param2;
                    det->SetDetectionParams(params);
                }
            } catch (...) {}
        });
    }

    // Start time for relative timestamps in metrics
    auto startTime = std::chrono::steady_clock::now();
    // change whether focus tool is enabled via it's toggle button
    QObject::connect(panel, &CameraControlPanel::focusToolToggled, &fe, &FocusEvaluator::onSetFocusTool);
    QObject::connect(panel, &CameraControlPanel::zoomValueChanged, viewer, &QtCameraViewer::setViewerZoomValue);

    std::thread capture([&]() {
        for (;;) {
            if (!running) break;

            // // DEBUG
            // qDebug("[dbg] main.cpp fe.focusToolEnabled = %d", fe.focusToolEnabled.load());

            std::shared_ptr<Camera> cam;
            { std::lock_guard<std::mutex> lk(cam_mutex); cam = current_camera; }
            if (!cam) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
            const uint64_t current_epoch = switch_epoch.load(std::memory_order_acquire);
            const unsigned current_serial = active_serial.load(std::memory_order_acquire);
            const unsigned cam_serial = cam->Serial();

            while (auto frame = cam->LatestFrame()) {
                if (switch_epoch.load(std::memory_order_acquire) != current_epoch ||
                    active_serial.load(std::memory_order_acquire) != current_serial ||
                    cam_serial != current_serial) {
                    break;
                }

                fps_calculator.update(*frame);

                int w = frame->Width();
                int h = frame->Height();
                if (w <= 0 || h <= 0) break;

                const auto ftype = frame->FrameType();
                unsigned outBpp = (ftype == Core::ObjectMode) || (ftype == Core::SegmentMode) ? 24U
                    : (ftype == Core::GrayscaleMode) ? 8U
                    : 32U;
                const int bytesPerPixel = int(outBpp / 8);
                const int stride = w * bytesPerPixel;

                auto* raw_bmp = bmp_pool.acquire(w, h, int(outBpp), stride);

                // bmp will be freed after last copy is out of scope
                auto bmp_shared = std::shared_ptr<CameraLibrary::Bitmap>(
                    raw_bmp,
                    [&bmp_pool](CameraLibrary::Bitmap* b) { bmp_pool.release(b); }
                );

                frame->Rasterize(*cam, raw_bmp);

                // update lens result with current grade
                QtConcurrent::run([lens_result, &mMgr]() {
                    QMetaObject::invokeMethod(qApp, [lens_result, &mMgr]() {
                        lens_result->updateTextandColor(mMgr);
                        }, Qt::QueuedConnection);
                });

                // if focus evaluation enabled, do so now
                if (fe.focusToolEnabled && frameCount == 0) {
                    auto* bmp_clone = bmp_pool.acquire(w, h, int(outBpp), stride);
                    std::memcpy(bmp_clone->GetBits(), raw_bmp->GetBits(), size_t(h * stride));

                    auto bmp_clone_shared = std::shared_ptr<CameraLibrary::Bitmap>(
                        bmp_clone,
                        [&bmp_pool](CameraLibrary::Bitmap* b) { bmp_pool.release(b); }
                    );

                    QtConcurrent::run([&fe, focus_result, bmp_clone_shared, panel, viewer, &startTime, &circleDetectionEnabled, &mMgr]() {
                        int circleCount = 0;
                        if (circleDetectionEnabled.load(std::memory_order_acquire)) {
                            auto circles = fe.DetectCircleMarkers(bmp_clone_shared.get());
                            circleCount = static_cast<int>(circles.size());
                        }

                        double score = fe.EvaluateBitmapFocus(bmp_clone_shared.get());
                        qDebug("[dbg] Focus score: %.2f", score);

                        auto now = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
                        qreal relativeTime = elapsed.count() / 1000.0;

                        QMetaObject::invokeMethod(
                            qApp,
                            [focus_result, score, circleCount, panel, viewer, relativeTime, &mMgr]() {

                                focus_result->updateTextandColor(score, mMgr);

                                if (panel) {
                                    panel->updateCircleCount(circleCount);
                                }

                                viewer->focus_score = score;

                                if (panel && panel->getFocusMetricsController()) {
                                    QHash<QString, qreal> focusMetrics;
                                    focusMetrics["FocusQuality"] = score;
                                    focusMetrics["CircleCount"] = circleCount;
                                    panel->getFocusMetricsController()->addData(relativeTime, focusMetrics);
                                    //qDebug("[metrics] Added focus data at t=%.2f, score=%.2f", relativeTime, score);
                                }
                            },
                            Qt::QueuedConnection
                        );
                    });
                }

                QMetaObject::invokeMethod(viewer->videoContainer(), [raw_bmp, viewer, &bmp_pool](){
                    viewer->videoWidget()->updateFrameFromBitmap(raw_bmp);
                }, Qt::QueuedConnection);

                frameCount += 1;
                frameCount = frameCount % focusEvalFrameGap;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    // Ensure Camera Library Shutdown on program exit
    guard.captureThread = &capture;
    guard.runningFlag   = &running;
    guard.mgr           = mgr;

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&](){
        if (viewer)
            viewer->close();

        guard.finalize();
    });

    std::atexit([](){
        try { CameraManager::X().Shutdown(); } catch (...) {}
    });

    // Wire up circle detection controls
    QObject::connect(panel, &CameraControlPanel::circleDetectionToggled, [&circleDetectionEnabled](bool enabled){
        circleDetectionEnabled.store(enabled, std::memory_order_release);
        qDebug("[ui] Circle detection %s", enabled ? "enabled" : "disabled");
    });

    QObject::connect(panel, &CameraControlPanel::circleParam2Changed, [&fe](double param2){
        auto params = fe.GetCircleDetector()->GetDetectionParams();
        params.param2 = param2;
        fe.GetCircleDetector()->SetDetectionParams(params);
        qDebug("[ui] Circle param2 changed to %.1f", param2);
    });

    const int rc = app.exec();
    guard.finalize();
    delete mgr;
    return rc;
}
