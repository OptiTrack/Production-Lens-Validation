#include "CircleMarkerDetector.h"
#include <QtLogging>

CircleMarkerDetector::CircleMarkerDetector() : m_params() {}

CircleMarkerDetector::CircleMarkerDetector(const DetectionParams &params)
    : m_params(params) {}

cv::Mat CircleMarkerDetector::ConvertBitmapToMat(CameraLibrary::Bitmap *bmp) {
  if (!bmp) {
    throw std::runtime_error("Null bitmap passed to CircleMarkerDetector");
  }

  int width = bmp->PixelWidth();
  int height = bmp->PixelHeight();
  int bpp = bmp->GetBitsPerPixel();
  unsigned char *bits = bmp->GetBits();

  int cvType = (bpp == 8)    ? CV_8UC1
               : (bpp == 24) ? CV_8UC3
               : (bpp == 32) ? CV_8UC4
                             : -1;

  if (cvType == -1) {
    throw std::runtime_error(
        "Unsupported bitmap color depth for circle detection");
  }

  cv::Mat mat(height, width, cvType, const_cast<unsigned char *>(bits));
  return mat.clone(); // Clone to ensure ownership
}

std::vector<CircleMarkerDetector::CircleMarker>
CircleMarkerDetector::DetectCircles(CameraLibrary::Bitmap *bmp) {
  cv::Mat mat = ConvertBitmapToMat(bmp);
  return DetectCirclesFromMat(mat);
}

/// <summary>
/// Detects circular markers in the bitmap frame using Hough Circle detection
/// </summary>
/// <param name="bmp">Input frame bitmap from camera</param>
/// <returns>Vector of detected circle markers</returns>
std::vector<CircleMarkerDetector::CircleMarker>
CircleMarkerDetector::DetectCircleMarkers(CameraLibrary::Bitmap *bmp) {

  try {
    auto circles = DetectCircles(bmp);

    if (!circles.empty()) {
      for (size_t i = 0; i < circles.size(); ++i) {
        const auto &circle = circles[i];
        const char *shapeStr =
            (circle.shapeType == CircleMarkerDetector::ShapeType::Circle)
                ? "Circle"
            : (circle.shapeType == CircleMarkerDetector::ShapeType::Oval)
                ? "Oval"
                : "Hook";
        // qDebug("[dbg]   Circle %zu: center=(%.1f, %.1f), radius=%.1f, c: %.2f
        // (%s)",
        //     i, circle.center.x, circle.center.y, circle.radius,
        //     circle.circularity, shapeStr);
      }
    }

    return circles;
  } catch (const std::exception &e) {
    qWarning("[dbg] Circle detection failed: %s", e.what());
    return std::vector<CircleMarkerDetector::CircleMarker>();
  }
}

std::vector<CircleMarkerDetector::CircleMarker>
CircleMarkerDetector::DetectCirclesFromMat(const cv::Mat &mat) {
  DetectionParams params;
  {
    std::lock_guard<std::mutex> lock(m_paramsMutex);
    params = m_params;
  }

  // Convert to grayscale
  cv::Mat gray;
  if (mat.channels() == 3) {
    cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
  } else if (mat.channels() == 4) {
    cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);
  } else {
    gray = mat;
  }

  // Detect circles using Hough Circle Transform
  cv::Mat blurred;
  cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 1.0);
  std::vector<cv::Vec3f> circles;
  cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT, params.dp,
                   params.minDist, params.param1, params.param2,
                   static_cast<int>(params.minRadius),
                   static_cast<int>(params.maxRadius));

  m_lastDetectionCount = static_cast<int>(circles.size());

  // Detect contours using Canny surrounding the detected circles
  cv::Mat edges;
  cv::adaptiveThreshold(blurred, edges, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                        cv::THRESH_BINARY, 11, 2);
  cv::Canny(edges, edges, 50, 150, 3);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(edges.clone(), contours, cv::RETR_LIST,
                   cv::CHAIN_APPROX_SIMPLE);

  // Convert results to CircleMarker objects
  std::vector<CircleMarker> result;
  int id = 0;
  
  for (const auto &circle : circles) {
    CircleMarker marker;
    marker.center = cv::Point2f(circle[0], circle[1]);
    marker.radius = circle[2];
    marker.isValid = true;

   marker.circularity = CalculateCircularity(
    marker.center,
    marker.radius,
    contours,
    marker.contour);
    
    marker.shapeType = CategorizeShape(marker.circularity);

    // Quality metric based on ideal circularity/shape
    if (marker.shapeType == ShapeType::Circle) {
      marker.quality = marker.circularity;
    } else if (marker.shapeType == ShapeType::Oval) {
      marker.quality = 0.5f;
    } else {
      marker.quality = 0.0f;
    }

    marker.id = id;
    id++;

    result.push_back(marker);
  }
  return result;
}

float CircleMarkerDetector::CalculateCircularity(
    const cv::Point2f &center,
    float radius,
    const std::vector<std::vector<cv::Point>> &contours,
    std::vector<cv::Point> &outContour)
{
    outContour.clear();

    if (contours.empty() || radius <= 0)
        return 1.0f;

    int bestContourIdx = -1;
    double bestDistance = 1e9;

    //
    // Step 1: choose nearest valid contour
    //
    for (size_t i = 0; i < contours.size(); ++i) {
        if (contours[i].size() < 10)
            continue;

        cv::Moments m = cv::moments(contours[i]);
        if (m.m00 == 0)
            continue;

        cv::Point2f centroid(
            static_cast<float>(m.m10 / m.m00),
            static_cast<float>(m.m01 / m.m00));

        double dist = cv::norm(centroid - center);

        if (dist > radius * 2.0)
            continue;

        if (dist < bestDistance)
        {
            bestDistance = dist;
            bestContourIdx = static_cast<int>(i);
        }
    }

    if (bestContourIdx < 0)
        return 1.0f;

    //
    // Step 2: smooth contour
    //
    std::vector<cv::Point> smoothContour;
    cv::approxPolyDP(
        contours[bestContourIdx],
        smoothContour,
        2.0,
        true);

    if (smoothContour.size() < 5)
        return 1.0f;

    outContour = smoothContour;

    //
    // Step 3: robust circularity
    //
    double area = cv::contourArea(smoothContour);
    double perimeter = cv::arcLength(
        smoothContour,
        true);

    if (perimeter <= 0.0)
        return 1.0f;

    double circularity =
        (4.0 * CV_PI * area) /
        (perimeter * perimeter);

    return std::clamp(
        static_cast<float>(circularity),
        0.0f,
        1.0f);
}

CircleMarkerDetector::ShapeType
CircleMarkerDetector::CategorizeShape(float circularity) {
  // Thresholds for shape categorization
  const float OVAL_UPPER_THRESHOLD = 0.92f;
  const float OVAL_LOWER_THRESHOLD = 0.70f;

  if (circularity > OVAL_UPPER_THRESHOLD) {
    return ShapeType::Circle;
  } else if (circularity >= OVAL_LOWER_THRESHOLD) {
    return ShapeType::Oval;
  } else {
    return ShapeType::Hook;
  }
}

void CircleMarkerDetector::SetDetectionParams(const DetectionParams &params) {
  std::lock_guard<std::mutex> lock(m_paramsMutex);
  m_params = params;
}
