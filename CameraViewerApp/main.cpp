#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <memory>
#include <cstdlib>
#include <QFuture>
#include <QApplication>
#include <QLabel>

#include <opencv2/opencv.hpp>

#include "QtCameraConnectionManager.h"
#include "QtVideoWidget.h"
#include "QtCameraViewer.h"
#include "CameraHelpers.h"
#include "BitmapPool.h"
#include "QtCameraControlPanel.h" 
#include "FocusEval.h"
#include "GradientDot.h"

#ifdef HAVE_FFMPEG
#include "videodecoder.h"
#endif

#include <qfuture.h>
#include <qlogging.h>
#include <QtConcurrent/qtconcurrentrun.h>
#include <qobjectdefs.h>

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
    QtCameraViewer::ApplyAppStyle();

    // ==== Camera manager ========================================================
    auto* mgr = new CameraConnectionManager(); 

    // ==== Shared state for capture thread ======================================
    std::mutex cam_mutex;
    std::shared_ptr<Camera> current_camera;
    std::atomic<uint64_t> switch_epoch{0};
    std::atomic<unsigned>  active_serial{0};

    CameraHelper::FrameRateCalculator fps_calculator{0.5 /*smoothing*/ };

    DisplayResults* focus_result = new DisplayResults("Disabled");

    // The core UI/window for the program
    auto* viewer = new QtCameraViewer(mgr, cam_mutex, current_camera, switch_epoch, active_serial,
                                      fps_calculator, focus_result, nullptr);
    viewer->resize(1100, 600);
    viewer->show();

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
    
    FocusEvaluator fe;
    const int focusEvalFrameGap = 10;
    int frameCount = 0;

    GradientDot* dot = new GradientDot();
    dot->resize(200, 200);
    dot->move(viewer->videoContainer()->width() - 30, 10); // top-right corner
    dot->setValue(0.3); // red
    dot->show();
    dot->raise();

    std::thread capture([&](){
        for (;;) {
            if (!running) break;

            std::shared_ptr<Camera> cam;
            { std::lock_guard<std::mutex> lk(cam_mutex); cam = current_camera; }
            if (!cam) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
            const uint64_t current_epoch  = switch_epoch.load(std::memory_order_acquire);
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

                double score = 0;

                // if focus evaluation enabled, do so now
                if (focusToolEnabled && frameCount == 0) {
                    QFuture<void> result = QtConcurrent::run([&fe, &focus_result, &dot, bmp_shared, &score]() {
                        score = fe.EvaluateBitmapFocus(bmp_shared.get());
                        qDebug("[dbg] Focus score: %.2f", score);

                        QMetaObject::invokeMethod(
                            qApp,
                            [focus_result, dot, score]() {

								dot->setValue(score);
                                
                                focus_result->updateTextandColor(score);
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
        if (viewer) {
            viewer->close();
        }
        guard.finalize();
    });
    std::atexit([](){
        try { CameraManager::X().Shutdown(); } catch (...) {}
    });

    const int rc = app.exec();
    guard.finalize();
    delete mgr;
    return rc;
}
