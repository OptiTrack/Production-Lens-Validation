#pragma once

#include <atomic>
#include <mutex>

#include <QObject>
#include <opencv2/opencv.hpp>

#include "CameraHelpers.h"

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

    std::mutex scoreMutex;
    double smoothedScore = 0.0;

    // Initial Laplacian-stddev bounds, adjusted as new values arrive
    static constexpr double lapStdBlur  = 15.0;
    static constexpr double lapStdSharp = 200.0;

    // Per-session adaptive bounds. 
    // Sharp grows when exceeded, blur shrinks when undershot
    double observedSharp = lapStdSharp;
    double observedBlur  = lapStdBlur;

    static constexpr double boundsAlpha = 0.05; // EWM for bound expansion to prevent spikes from transient peaks
	static constexpr double ewmAlpha = 0.2;     // EWM alpha for smoothing the final score to prevent jitter

    static constexpr int minMaskPixels = 3; // minimum pixel width of a marker to filter noise

    static constexpr double markerThreshRatio = 0.5; // Mask anything within markerThreshRatio of the brightest pixel.
    static constexpr double minImageBrightness = 30.0; //  gates "frame too dark to contain any marker at all" before we divide by max.
};