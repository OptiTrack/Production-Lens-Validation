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
    /// @brief Represents a detected circular marker
    struct CircleMarker {
        cv::Point2f center;      ///< Center coordinates
        float radius;            ///< Radius in pixels
        bool isValid;            ///< Always true for detected circles
    };

    /// @brief Configuration parameters for circle detection
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

    /// @brief Detect circular markers in a bitmap frame
    std::vector<CircleMarker> DetectCircles(CameraLibrary::Bitmap* bmp);

    /// @brief Detect circular markers in an OpenCV Mat
    std::vector<CircleMarker> DetectCirclesFromMat(const cv::Mat& mat);

    /// @brief Update detection parameters
    void SetDetectionParams(const DetectionParams& params);

    /// @brief Get current detection parameters
    const DetectionParams& GetDetectionParams() const { return m_params; }

    /// @brief Get detection count from last run
    int GetLastDetectionCount() const { return m_lastDetectionCount; }

private:
    cv::Mat ConvertBitmapToMat(CameraLibrary::Bitmap* bmp);
    
    DetectionParams m_params;
    int m_lastDetectionCount = 0;
};
