#pragma once

#include "CameraHelpers.h"
#include <mutex>
#include <opencv2/opencv.hpp>
#include <vector>

namespace CircleDetectorConsts {

    // Thresholds for shape categorization
    constexpr float OVAL_UPPER_THRESHOLD = 0.92f;
    constexpr float OVAL_LOWER_THRESHOLD = 0.62f;
}

/**
 * @class CircleMarkerDetector
 * @brief Detects circular marker shapes using OpenCV Hough Circle Transform and
 * contour analysis
 *
 * Categorizes detected shapes as Circle, Oval, or Hook based on circularity
 * metrics.
 */
class CircleMarkerDetector {
public:
  enum class ShapeType {
    Circle = 0,
    Oval = 1,
    Hook = 2
  };

  struct CircleMarker {
    cv::Point2f center;
    float radius;
    bool isValid;
    float circularity = 1.0f; ///< Circularity metric (1.0 = perfect circle)
    ShapeType shapeType = ShapeType::Circle; ///< Categorized shape type
    std::vector<cv::Point> contour;          ///< Contour points for rendering
    float quality = 1.0f;                    ///< Detection quality metric
    int id;
  };

  /// @brief Detect circular markers in a frame
  /// @param bmp Bitmap frame to analyze
  /// @return Vector of detected circle markers
  std::vector<CircleMarker> DetectCircleMarkers(CameraLibrary::Bitmap *bmp);

  struct DetectionParams {
    double dp = 1.0;         ///< Inverse ratio of accumulator resolution
    double minDist = 10.0;   ///< Minimum distance between circles
    double param1 = 300.0;   ///< Upper threshold for Canny edge detection
    double param2 = 10.0;    ///< Accumulator threshold for circle detection
    double minRadius = 1.0;  ///< Minimum circle radius
    double maxRadius = 30.0; ///< Maximum circle radius
  };

  CircleMarkerDetector();
  explicit CircleMarkerDetector(const DetectionParams &params);
  ~CircleMarkerDetector() = default;

  std::vector<CircleMarker> DetectCircles(CameraLibrary::Bitmap *bmp);

  std::vector<CircleMarker> DetectCirclesFromMat(const cv::Mat &mat);

  void SetDetectionParams(const DetectionParams &params);

  const DetectionParams &GetDetectionParams() const { return m_params; }

  int GetLastDetectionCount() const { return m_lastDetectionCount; }

private:
  cv::Mat ConvertBitmapToMat(CameraLibrary::Bitmap *bmp);

  /// @brief Calculate circularity metric for a detected circle using contour
  /// geometry
  /// @param center Circle center point
  /// @param radius Circle radius
  /// @param contours Detected contours for comparison
  /// @param outContour Output: the contour used for calculation
  /// @return Circularity value (0.0 to 1.0, where 1.0 is perfect circle)
  float
  CalculateCircularity(const cv::Point2f &center, float radius,
                       const std::vector<std::vector<cv::Point>> &contours,
                       std::vector<cv::Point> &outContour);

  /// @brief Categorize shape based on circularity
  /// @param circularity The circularity value (0.0-1.0)
  /// @return The categorized shape type
  ShapeType CategorizeShape(float circularity);

  mutable std::mutex m_paramsMutex;
  DetectionParams m_params;
  int m_lastDetectionCount = 0;
};
