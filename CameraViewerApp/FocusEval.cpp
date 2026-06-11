#include <algorithm>
#include <bitmap.h>
#include <mutex>
#include <opencv2/core/cvdef.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <qlogging.h>
#include <stdexcept>

#include "FocusEval.h"

/// <summary>
/// Given a grayscale Mat, determine avg circularity and area of contours found
/// </summary>
/// <param name="bmp">Incoming frame bitmap data</param>
/// <returns>frameScore struct with quantative focus data</returns>
cv::Mat FocusEvaluator::ConvertBitmapToMat(CameraLibrary::Bitmap* bmp) {
    int width = bmp->PixelWidth();
    int height = bmp->PixelHeight();
    int bpp = bmp->GetBitsPerPixel();
    unsigned char* bits = bmp->GetBits();
    int cvType = (bpp == 8) ? CV_8UC1
        : (bpp == 24) ? CV_8UC3
        : (bpp == 32) ? CV_8UC4
        : -1;
    if (cvType == -1) {
        throw std::runtime_error("Unsupported bitmap color depth");
    }
    cv::Mat mat(height, width, cvType, const_cast<unsigned char*>(bits));
    return mat.clone(); // clone to ensure ownership
}

/// <summary>
/// Receives incoming frame and grades it's focus against existing data.
/// </summary>
/// <param name="bmp">Incoming frame data from camera</param>
/// <returns>Double ranging from 0.0 to 1.0, where 1.0 indicates optimal
/// focus</returns>
double FocusEvaluator::EvaluateBitmapFocus(CameraLibrary::Bitmap* bmp) {

    if (!bmp) {
        return 0.0;
    }

    cv::Mat img;
    cv::Mat lap;
    cv::Mat gray;
    cv::Mat brightMask;
    cv::Scalar lapMean;
    cv::Scalar lapStdDev;

    double imgMax = 0.0;

    try {
        img = ConvertBitmapToMat(bmp);
    }

    catch (const std::exception& e) {
        qWarning("[!] FocusEval bitmap conversion failed: %s", e.what());
        return 0.0;
    }

    if (img.channels() == 3) {
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    }
    else if (img.channels() == 4) {
        cv::cvtColor(img, gray, cv::COLOR_BGRA2GRAY);
    }
    else {
        img.copyTo(gray);
    }

    // Restrict the sharpness measurement to a mask of bright pixels, so the
    // score reflects marker-edge sharpness independent of how much of the
    // frame the markers occupy.
    cv::minMaxLoc(gray, nullptr, &imgMax);

    if (imgMax < minImageBrightness) {
        std::lock_guard<std::mutex> lock(scoreMutex);
        smoothedScore = (1.0 - ewmAlpha) * smoothedScore;
        return smoothedScore;
    }

    cv::threshold(
        gray,
        brightMask,
        imgMax * markerThreshRatio,
        255,
        cv::THRESH_BINARY);

    if (cv::countNonZero(brightMask) < minMaskPixels) {
        std::lock_guard<std::mutex> lock(scoreMutex);
        smoothedScore = (1.0 - ewmAlpha) * smoothedScore;
        return smoothedScore;
    }

    cv::Laplacian(gray, lap, CV_32F, 3);
    cv::meanStdDev(lap, lapMean, lapStdDev, brightMask);

    double lapStd = lapStdDev[0];

    std::lock_guard<std::mutex> lock(scoreMutex);

    // Adjust bounds based on newest values
    // EWM smoothing provides some resistance against sudden spikes
    if (lapStd > observedSharp) {
        observedSharp += boundsAlpha * (lapStd - observedSharp);
    }
    if (lapStd < observedBlur) {
        observedBlur += boundsAlpha * (lapStd - observedBlur);
    }

    //qDebug("[dbg] FocusEval lapStd=%.2f range=[%.2f, %.2f]", lapStd, observedBlur, observedSharp);

    double range = std::max(observedSharp - observedBlur, 1.0);

    double rawScore =
        std::clamp(
            (lapStd - observedBlur) / range,
            0.0,
            1.0);

    smoothedScore =
        ewmAlpha * rawScore +
        (1.0 - ewmAlpha) * smoothedScore;

    return smoothedScore;
}

/// <summary>
/// Clears existing focus data, to reset the tool to a "cold start" state.
/// Necessary when video mode is changed.
/// </summary>
void FocusEvaluator::onResetFocusStats() {

    qDebug("[dbg] FocusEvaluator reset");

    std::lock_guard<std::mutex> lock(scoreMutex);

    smoothedScore = 0.0;
    observedSharp = lapStdSharp;
    observedBlur = lapStdBlur;
}

/// <summary>
/// Changes state of focusToolEnabled (atomic bool) depending on state of
/// focus_button
/// </summary>
/// <param name="toggle">State (T/F) of focus_button</param>
/// <returns>Nothing</returns>
void FocusEvaluator::onSetFocusTool(bool toggle) {

    this->focusToolEnabled = toggle;
    qDebug("[dbg] focusToolEnabled = %d", toggle);
}
