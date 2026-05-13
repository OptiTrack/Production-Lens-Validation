#include "CircleMarkerDetector.h"
#include <QtLogging>
#include <algorithm>

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
  result.reserve(circles.size());

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

    result.push_back(marker);
  }

  // Assign IDs based on marker position
  // top-left is ID 0, left-to-right within rows, top-to-bottom across rows
  std::sort(result.begin(), result.end(), [](const CircleMarker &a,
                                            const CircleMarker &b) {
    if (a.center.y < b.center.y - 1e-3f) { //1e-3f is a tiny absolute tolerance (0.001 less than)
      return true;
    }
    if (a.center.y > b.center.y + 1e-3f) {
      return false;
    }
    return a.center.x < b.center.x;
  });

  for (int i = 0; i < static_cast<int>(result.size()); ++i) {
    result[i].id = i;
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

    if (contours.empty() || radius <= 0) {
        qDebug("No markers!");
        return 0.0f;
    }


    // Expected area of the ideal circle — used for sanity filtering.
    const double expectedArea = CV_PI * radius * radius;
    if (expectedArea == 0) {
        qDebug("Bad radius for circlemarkerdetector!");
    }

    int    bestContourIdx = -1;
    double bestDistance   = 1e9;


    // Choose nearest valid contour.
    for (size_t i = 0; i < contours.size(); ++i)
    {

        if (contours[i].size() < 10)
            continue;

        // Area must be at least 20% of the expected circle area to be a plausible candidate.
        cv::Moments m = cv::moments(contours[i]);
        if (m.m00 < 0.20 * expectedArea)
            continue;

        cv::Point2f centroid(
            static_cast<float>(m.m10 / m.m00),
            static_cast<float>(m.m01 / m.m00));

        double dist = cv::norm(centroid - center);
        if (dist > radius * 2.0)
            continue;

        if (dist < bestDistance)
        {
            bestDistance   = dist;
            bestContourIdx = static_cast<int>(i);
        }
    }

    if (bestContourIdx < 0) {
        qDebug("No best contour!");
        return 0.0f; 
    }

    const std::vector<cv::Point> &rawContour = contours[bestContourIdx];

    // Compute circularity using ellipse fitting (consistent with ROI zoom mode)
    double circularity = 0.0;
    if (rawContour.size() >= 5) {
        try {
            cv::RotatedRect ellipse = cv::fitEllipse(rawContour);
            float majorAxis =
                std::max(ellipse.size.width, ellipse.size.height) / 2.0f;
            float minorAxis =
                std::min(ellipse.size.width, ellipse.size.height) / 2.0f;
            if (majorAxis > 0) {
                circularity = std::clamp(
                    static_cast<double>(minorAxis / majorAxis), 0.0, 1.0);
                if (circularity == 0.0) {
                    qDebug("Circularity clamped to zero!");
                }
            }
        } catch (...) {
            qDebug("Exception!");
            circularity = 0.0;
        }
    }

    // Smooth only for the output contour (rendering / overlay).
    std::vector<cv::Point> smoothContour;
    cv::approxPolyDP(rawContour, smoothContour, 2.0, true);

    outContour = (smoothContour.size() >= 5)
                     ? smoothContour
                     : rawContour;

    return static_cast<float>(circularity);
}

CircleMarkerDetector::ShapeType
CircleMarkerDetector::CategorizeShape(float circularity) {

  if (circularity > CircleDetectorConsts::OVAL_UPPER_THRESHOLD) {
    return ShapeType::Circle;
  } else if (circularity >= CircleDetectorConsts::OVAL_LOWER_THRESHOLD) {
    return ShapeType::Oval;
  } else {
    return ShapeType::Hook;
  }
}

void CircleMarkerDetector::SetDetectionParams(const DetectionParams &params) {
  std::lock_guard<std::mutex> lock(m_paramsMutex);
  m_params = params;
}
