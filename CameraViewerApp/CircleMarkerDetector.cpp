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
  return mat.clone();
}

std::vector<CircleMarkerDetector::CircleMarker>
CircleMarkerDetector::DetectCircles(CameraLibrary::Bitmap *bmp) {
  cv::Mat mat = ConvertBitmapToMat(bmp);
  return DetectCirclesFromMat(mat);
}

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

  cv::Mat blurred;
  cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 1.0);

  // Detect circles using Hough Circle Transform
  std::vector<cv::Vec3f> circles;
  cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT, params.dp,
                   params.minDist, params.param1, params.param2,
                   static_cast<int>(params.minRadius),
                   static_cast<int>(params.maxRadius));

  m_lastDetectionCount = static_cast<int>(circles.size());

  // Convert results to CircleMarker objects
  std::vector<CircleMarker> result;
  result.reserve(circles.size());

  for (const auto &circle : circles) {
    CircleMarker marker;
    marker.center = cv::Point2f(circle[0], circle[1]);
    marker.radius = circle[2];
    marker.isValid = true;

    marker.circularity = CalculateCircularity(
        blurred,
        marker.center,
        marker.radius,
        marker.contour);

    marker.shapeType = CategorizeShape(marker.circularity);

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

  for (int i = 0; i < static_cast<int>(result.size()); ++i) {
    result[i].id = i;
  }

  return result;
}

float CircleMarkerDetector::CalculateCircularity(
    const cv::Mat &blurred,
    const cv::Point2f &center,
    float radius,
    std::vector<cv::Point> &outContour)
{
    outContour.clear();

    if (blurred.empty() || radius <= 0) {
        qDebug("Not a valid circle");
        return 0.0f;
    }

    // Crop a padded ROI around center of the circle center
    const int padding = static_cast<int>(radius * 0.5f);
    const int x = std::max(0, static_cast<int>(center.x - radius) - padding);
    const int y = std::max(0, static_cast<int>(center.y - radius) - padding);
    const int w = std::min(blurred.cols - x, static_cast<int>(radius * 2) + padding * 2);
    const int h = std::min(blurred.rows - y, static_cast<int>(radius * 2) + padding * 2);

    if (w <= 0 || h <= 0) {
        qDebug("A valid ROI could not be found.");
        return 0.0f;
    }

    cv::Mat roi = blurred(cv::Rect(x, y, w, h));

    cv::Mat otsuTemp;
    double otsuThresh = cv::threshold(roi, otsuTemp, 0, 255,
                                      cv::THRESH_BINARY | cv::THRESH_OTSU);
    otsuThresh = std::max(otsuThresh, 10.0);

    cv::Mat edges;
    cv::Canny(roi, edges, otsuThresh * 0.5, otsuThresh, 3);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        qDebug("No Contour of the circle could be found");
        return 0.0f;
    }

    size_t bestIdx = 0;
    double bestLen = 0.0;
    for (size_t i = 0; i < contours.size(); ++i) {
        double len = cv::arcLength(contours[i], false);
        if (len > bestLen) {
            bestLen = len;
            bestIdx = i;
        }
    }

    double circularity = 0.0;
    try {
        cv::RotatedRect ellipse = cv::fitEllipse(contours[bestIdx]);
        float majorAxis = std::max(ellipse.size.width, ellipse.size.height) / 2.0f;
        float minorAxis = std::min(ellipse.size.width, ellipse.size.height) / 2.0f;
        if (majorAxis > 0) {
            circularity = std::clamp(
                static_cast<double>(minorAxis / majorAxis), 0.0, 1.0);
        }
    } catch (...) {
        qDebug("Could not calculate circularity");
        circularity = 0.0;
    }

    outContour = contours[bestIdx];
    for (auto &pt : outContour) {
        pt.x += x;
        pt.y += y;
    }

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