#include <QApplication>
#include <QDateTime>
#include <QFuture>
#include <QLabel>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <qfile.h>
#include <qtimer.h>
#include <thread>

// For chinese translation support
#include <QLibraryInfo>
#include <QTranslator>

#include "BitmapPool.h"
#include "CameraHelpers.h"
#include "CircleMarkerDetector.h"
#include "FocusEval.h"
#include "MetricsManager.h"
#include "QtCameraConnectionManager.h"
#include "QtCameraControlPanel.h"
#include "QtCameraViewer.h"
#include "QtVideoWidget.h"
#include "metricscontroller.h"
#include <QCoreApplication>

#ifdef HAVE_FFMPEG
#include "videodecoder.h"
#endif

#include "FocusResultLabel.h"
#include "FocusScoreLabel.h"
#include "LensResultLabel.h"
#include <QtConcurrent/qtconcurrentrun.h>
#include <qfuture.h>
#include <qlogging.h>
#include <qobject.h>

using namespace CameraLibrary;

int main(int argc, char *argv[]) {
  // Guard to execute the cameralibrary shutdown on exit
  struct ShutdownGuard {
    std::atomic_bool done{false};
    std::thread *captureThread{nullptr};
    std::atomic_bool *runningFlag{nullptr};
    CameraConnectionManager *mgr{nullptr};

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

  // Declare a QTranslator to load translations for the application and Qt's
  // built-in strings This will load app_en.ts and app_zh_CN.ts from the i18n
  // directory, as well as the corresponding Qt translations if locale is
  // Chinese appTranslator and qtTranslator are declared here to ensure they
  // remain in scope for the duration of the application, as QTranslator must
  // not be destroyed while installed in the application

  // for our application's custom translations
  QTranslator appTranslator;

  // for Qt's built-in translations (e.g. file dialog buttons)
  QTranslator qtTranslator;

  QFile file(QCoreApplication::applicationDirPath() + "/motive.css");

  if (file.open(QFile::ReadOnly | QFile::Text)) {
    QString styleSheet = file.readAll();
    app.setStyleSheet(styleSheet);
    file.close();
  }

  else
    qWarning() << "[!] Could not open stylesheet file.";

  auto *mgr = new CameraConnectionManager();

  // ==== Shared state for capture thread ====
  std::mutex cam_mutex;
  std::shared_ptr<Camera> current_camera;
  std::atomic<uint64_t> switch_epoch{0};
  std::atomic<unsigned> active_serial{0};

  FocusResultLabel *focus_result = new FocusResultLabel("Disabled");
  FocusScoreLabel *focus_score = new FocusScoreLabel("0");
  LensResultLabel *lens_result = new LensResultLabel("Unknown");

  MetricsManager mMgr;
  //mMgr.testMM();

  QTimer focusTimer, gradeTimer, circleDetectTimer;

  // set when QtConcurrent focus/grade is running
  std::atomic<bool> focusBusy{false};
  std::atomic<bool> gradeBusy{false};
  std::atomic<bool> circleDetectionBusy{false};

  // bitmap frame shared between timed events
  std::shared_ptr<CameraLibrary::Bitmap> framePtr;
  std::shared_ptr<std::vector<CircleMarkerDetector::CircleMarker>> markerCollection;

  FocusEvaluator fe;        // Focus evaluator instance
  CircleMarkerDetector cmd; // Circle marker detector instance

  const int focusIntervalMs = 125;       // Interval for focus evaluation (ms)
  const int gradeIntervalMs = 250;       // Interval for grading (ms)   
  const int circleDetectIntervalMs = 125; // Interval for circle detection (ms)

  circleDetectTimer.start(circleDetectIntervalMs);
  focusTimer.start(focusIntervalMs);
  gradeTimer.start(gradeIntervalMs);

  // The core UI/window for the program
  auto *viewer = new QtCameraViewer(
      mgr, cam_mutex, current_camera, switch_epoch, active_serial,
      focus_result, focus_score, lens_result, mMgr, nullptr);

  // Remove any current installed translators to ensure a clean slate, then
  // install the appropriate ones based on the current locale use [&] to capture
  // the app and translators by reference so they remain in scope and can be
  // modified inside the lambda
  auto applyLanguage = [&](const QString &localeName) {
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

      bool ok = qtTranslator.load(loc, "qtbase", "_", qtTrDir) ||
                qtTranslator.load(loc, "qt", "_", qtTrDir);

      if (!ok)
        qWarning() << "[i18n] Failed to load Qt zh_CN translations from"
                   << qtTrDir;

      else
        app.installTranslator(&qtTranslator);
    }

    if (viewer)
      viewer->retranslateUi();
  };

  // get instance to camera control panel for metrics updates
  auto *panel = viewer->getControlPanel();

  QObject::connect(
    viewer, &QtCameraViewer::languageChanged, &app,
    [applyLanguage](const QString &locale) { applyLanguage(locale); });

  applyLanguage(viewer->currentLanguage());

  // set up shared metrics object and make Qt signal connection
  QObject::connect(panel, &CameraControlPanel::exportMetricsRequested,
                   [&mMgr]() { mMgr.ExportMetrics(); });

  viewer->resize(1400, 800);
  viewer->show();

  // Bitmap resource
  BitmapPool bmp_pool([](int w, int h, int bpp, int stride) -> Bitmap * {
    auto fmt = (bpp == 8)    ? Bitmap::EightBit
               : (bpp == 16) ? Bitmap::SixteenBit
               : (bpp == 24) ? Bitmap::TwentyFourBit
                             : Bitmap::ThirtyTwoBit;
    return new Bitmap(w, h, stride, fmt);
  });

  // ==== Main Application Thread ====
  std::atomic_bool running(true);
  std::atomic_bool focusToolEnabled(true); // changed by focus UI control
  std::atomic_bool circleDetectionEnabled(
      false); // changed by circle detection UI control

  double focusScore = 0.0; // Latest focus score

  fe.focusToolEnabled =
      true; // changed by focus UI control; set True by default

  // Start time for relative timestamps in metrics
  auto startTime = std::chrono::steady_clock::now();

  QObject::connect(panel, &CameraControlPanel::resetFocusStats, &fe,
                   &FocusEvaluator::onResetFocusStats);
  QObject::connect(panel, &CameraControlPanel::resetFocusStats, &app,
                   [&markerCollection]() {
                     std::atomic_store_explicit(
                         &markerCollection,
                         std::shared_ptr<std::vector<CircleMarkerDetector::CircleMarker>>{},
                         std::memory_order_release);
                   });
  QObject::connect(panel, &CameraControlPanel::focusToolToggled, &fe,
                   &FocusEvaluator::onSetFocusTool);
  QObject::connect(panel, &CameraControlPanel::zoomValueChanged, viewer,
                   &QtCameraViewer::setViewerZoomValue);

  // Wire circle detection to VideoWidget
  if (viewer && viewer->videoWidget()) {
    QObject::connect(panel, &CameraControlPanel::circleDetectionToggled,
                     viewer->videoWidget(),
                     &VideoWidget::setCircleDetectionEnabled);
  }

  // Update shared marker collection on a timer
  // The timer value used here should be less than or equal to the min(focustimer, gradetimer). 
  QObject::connect(&circleDetectTimer, &QTimer::timeout, [&]() {
      // if we're not grading or focusing, don't gather markers.
      if (!fe.focusToolEnabled.load(std::memory_order_acquire) &&
          !circleDetectionEnabled.load(std::memory_order_acquire)) {
          return;
      }

      auto localFrame = std::atomic_load_explicit(&framePtr, std::memory_order_acquire);

      if (!circleDetectionBusy.exchange(true)) {
      QtConcurrent::run([&cmd, localFrame, &circleDetectionBusy, &markerCollection]() {
          struct Guard {
              std::atomic<bool>& flag;
              ~Guard() { flag.store(false); }
          } guard{ circleDetectionBusy };

          if (!localFrame) return;

          auto newMarkers = std::make_shared<std::vector<CircleMarkerDetector::CircleMarker>>(
              cmd.DetectCircleMarkers(localFrame.get())
          );
          std::atomic_store_explicit(&markerCollection, newMarkers, std::memory_order_release);
      });
      }
  });

  QObject::connect(&focusTimer, &QTimer::timeout, [&]() {
    if (!fe.focusToolEnabled.load(std::memory_order_acquire)) {
      return;
    }

    auto localMarkers = std::atomic_load_explicit(&markerCollection, std::memory_order_acquire);
    if (!localMarkers) {
      return;
    }

    if (!focusBusy.exchange(true)) {

      const auto& circles = *localMarkers;
      QtConcurrent::run([&fe, focus_result, focus_score, panel,
                         viewer, &startTime, &mMgr, &focusBusy, circles]() {
        struct Guard {
          std::atomic<bool> &flag;
          ~Guard() { flag.store(false); }
        } guard{focusBusy};

        double score = fe.EvaluateBitmapFocus(circles);
        // qDebug("[dbg] Focus score: %.2f", score);

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - startTime);
        qreal relativeTime = elapsed.count() / 1000.0;

        QMetaObject::invokeMethod(
            qApp,
            [focus_result, focus_score, score, panel, viewer, relativeTime,
             &mMgr]() {
              mMgr.setFocusOptimal(score >= 0.65);
              focus_result->updateTextandColor(score, mMgr);
              focus_score->updateNumber(score, mMgr);

              if (panel && panel->getFocusMetricsController()) {
                QHash<QString, qreal> focusMetrics;
                focusMetrics["FocusQuality"] = score;
                panel->getFocusMetricsController()->addData(relativeTime,
                                                            focusMetrics);
              }
            },
            Qt::QueuedConnection);
      });
    }

    // if the focus tool isn't enabled, set the score to 0 and result to
    // "disabled"
    if (!fe.focusToolEnabled) {
      if (!focusBusy.exchange(true)) {
        QFuture<void> result = QtConcurrent::run(
            [&focus_result, &focus_score, viewer, &mMgr, &focusBusy]() {
              QMetaObject::invokeMethod(
                  qApp,
                  [focus_result, focus_score, viewer, mMgr]() {
                    focus_result->updateTextandColor(-1, mMgr);
                    focus_score->updateNumber(0, mMgr);
                  },
                  Qt::QueuedConnection);
              focusBusy.store(false);
            });
      }
    }
  });

  QObject::connect(&gradeTimer, &QTimer::timeout, [&]() {
    if (!fe.focusToolEnabled) {
      return;
    }

    auto localFrame = std::atomic_load_explicit(&framePtr, std::memory_order_acquire);
    auto localMarkers = std::atomic_load_explicit(&markerCollection, std::memory_order_acquire);

    if (!localFrame || !localMarkers) {
      return;
    }

    const auto& circles = *localMarkers;

    if (circleDetectionEnabled.load(std::memory_order_acquire) && localFrame) {
      if (!gradeBusy.exchange(true)) {
        QtConcurrent::run([circles, panel, viewer, &startTime,
                           &mMgr, &gradeBusy]() {
          struct Guard {
            std::atomic<bool> &flag;
            ~Guard() { flag.store(false); }
          } guard{gradeBusy};

          int circleCount = static_cast<int>(circles.size());
          qDebug("[dbg] Contours: %d", circleCount);

          auto now = std::chrono::steady_clock::now();
          auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
              now - startTime);
          qreal relativeTime = elapsed.count() / 1000.0;

          QMetaObject::invokeMethod(
              qApp,
              [circleCount, circles, panel, viewer, relativeTime, &mMgr]() {
                if (circleCount > 0) {
                  mMgr.addMarkers(circles);
                }
                double lensScore = mMgr.getLensScore();

                if (panel) {
                  panel->updateCircleCount(circleCount);
                }

                if (viewer && viewer->videoWidget()) {
                  viewer->videoWidget()->setDetectedCircleMarkers(
                      mMgr.getSmoothedMarkers());
                }

                if (panel && panel->getLensMetricsController()) {
                  QHash<QString, qreal> lensMetrics;
                  lensMetrics["LensHealth"] = lensScore;
                  panel->getLensMetricsController()->addData(relativeTime,
                                                             lensMetrics);
                }
                mMgr.clearMarkers();
              },
              Qt::QueuedConnection);
        });
      }
    }
    // update lens result with current grade
    lens_result->updateTextandColor(mMgr);
  });

  std::thread capture([&]() {
    for (;;) {
      if (!running)
        break;

      std::shared_ptr<Camera> cam;
      {
        std::lock_guard<std::mutex> lk(cam_mutex);
        cam = current_camera;
      }
      if (!cam) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }
      const uint64_t current_epoch =
          switch_epoch.load(std::memory_order_acquire);
      const unsigned current_serial =
          active_serial.load(std::memory_order_acquire);
      const unsigned cam_serial = cam->Serial();

      while (auto frame = cam->LatestFrame()) {
        if (switch_epoch.load(std::memory_order_acquire) != current_epoch ||
            active_serial.load(std::memory_order_acquire) != current_serial ||
            cam_serial != current_serial) {
          break;
        }

        int w = frame->Width();
        int h = frame->Height();
        if (w <= 0 || h <= 0)
          break;

        const auto ftype = frame->FrameType();
        unsigned outBpp =
            (ftype == Core::ObjectMode) || (ftype == Core::SegmentMode) ? 24U
            : (ftype == Core::GrayscaleMode)                            ? 8U
                                                                        : 32U;
        const int bytesPerPixel = int(outBpp / 8);
        const int stride = w * bytesPerPixel;

        auto *raw_bmp = bmp_pool.acquire(w, h, int(outBpp), stride);
        frame->Rasterize(*cam, raw_bmp);

        // Set up shared bmp resource for display, focus, and lens grade
        // purposes
        std::atomic_store_explicit(
            &framePtr,
            std::shared_ptr<CameraLibrary::Bitmap>(
                raw_bmp,
                [pool = &bmp_pool](CameraLibrary::Bitmap *b) { pool->release(b); }),
            std::memory_order_release);

        // repaint video widget with new frame
        QMetaObject::invokeMethod(
            viewer->videoContainer(),
            [framePtr, viewer]() {
              viewer->videoWidget()->updateFrameFromBitmap(framePtr.get());
            },
            Qt::QueuedConnection);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  });

  // Ensure Camera Library Shutdown on program exit
  guard.captureThread = &capture;
  guard.runningFlag = &running;
  guard.mgr = mgr;

  QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
    if (viewer)
      viewer->close();
    guard.finalize();
  });

  std::atexit([]() {
    try {
      CameraManager::X().Shutdown();
    } catch (...) {
    }
  });

  // Wire up circle detection controls
  QObject::connect(
      panel, &CameraControlPanel::circleDetectionToggled,
      [&circleDetectionEnabled](bool enabled) {
        circleDetectionEnabled.store(enabled, std::memory_order_release);
        qDebug("[ui] Circle detection %s", enabled ? "enabled" : "disabled");
      });

  QObject::connect(panel, &CameraControlPanel::circleParam2Changed,
                   [&cmd](double param2) {
                     auto params = cmd.GetDetectionParams();
                     params.param2 = param2;
                     cmd.SetDetectionParams(params);
                     qDebug("[ui] Circle param2 changed to %.1f", param2);
                   });

  QObject::connect(panel, &CameraControlPanel::clearLocksRequested, viewer, [viewer]() {
          viewer->videoWidget()->ClearROILocks();
          qDebug("Clearing locked quadrants...");
      });

  const int rc = app.exec();
  guard.finalize();
  delete viewer;
  delete mgr;
  return rc;
}
