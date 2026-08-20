#pragma once

#include <atomic>
#include <mutex>

#include <QObject>
#include <opencv2/opencv.hpp>

#include "CameraHelpers.h"
#include "core/FocusScore.h"

class FocusEvaluator : public QObject {
    Q_OBJECT

public:
    explicit FocusEvaluator(QObject* parent = nullptr)
        : QObject(parent),
          focusToolEnabled(true) {}

    double EvaluateBitmapFocus(CameraLibrary::Bitmap* bmp);

    std::atomic_bool focusToolEnabled;

public slots:
    void onSetFocusTool(bool toggle);
    void onResetFocusStats();

private:
    cv::Mat ConvertBitmapToMat(CameraLibrary::Bitmap* bmp);

    // The scoring math, its tuning constants and the per-session adaptive
    // bounds live in core/FocusScore.h, which has no Qt, OpenCV or Camera SDK
    // dependency and is unit tested in /tests. This class owns the mutex that
    // serialises access to the model.
    std::mutex scoreMutex;
    focus::FocusScoreModel scoreModel;
};