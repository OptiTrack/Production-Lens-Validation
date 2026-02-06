#pragma once

#include <vector>
#include <opencv2/opencv.hpp>
#include "CameraHelpers.h"

/**
 * @class CircleMarkerDetector
 * @brief Detects circular marker shapes using OpenCV Hough Circle Transform
 * 
 * Simple, direct circle detection without overcomplicated validation.
 */
class CircleMarkerDetector {
public:
    struct CircleMarker {
        cv::Point2f center;
        float radius;
        bool isValid;
    };

    struct DetectionParams {
        double dp = 1.0;         ///< Inverse ratio of accumulator resolution
        double minDist = 10.0;   ///< Minimum distance between circles
        double param1 = 300.0;   ///< Upper threshold for Canny edge detection
        double param2 = 20.0;    ///< Accumulator threshold for circle detection
        double minRadius = 1.0;  ///< Minimum circle radius
        double maxRadius = 30.0; ///< Maximum circle radius
    };

    CircleMarkerDetector();
    explicit CircleMarkerDetector(const DetectionParams& params);
    ~CircleMarkerDetector() = default;

    std::vector<CircleMarker> DetectCircles(CameraLibrary::Bitmap* bmp);

    std::vector<CircleMarker> DetectCirclesFromMat(const cv::Mat& mat);

    void SetDetectionParams(const DetectionParams& params);

    const DetectionParams& GetDetectionParams() const { return m_params; }

    int GetLastDetectionCount() const { return m_lastDetectionCount; }

private:
    cv::Mat ConvertBitmapToMat(CameraLibrary::Bitmap* bmp);
    
    DetectionParams m_params;
    int m_lastDetectionCount = 0;
};
