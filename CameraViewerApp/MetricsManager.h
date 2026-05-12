#pragma once
#include "CircleMarkerDetector.h"
#include <opencv2/core/types.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class MetricsManager {
public:
  enum lensDisposition { pass, check, fail, untested };

  enum markerClass {
    hook,
    oval,
    circle,
  };

  struct contourData {
    int id = -1;
    markerClass mClass;      // appearance classification of contour
    cv::Point2f centroid;    // defines position of contour in image
    double circularityScore; // closeness of contour to perfect circle (1.0 max)
  };

  struct SmoothedMarker {
    int id = -1;
    markerClass mClass = circle;
    cv::Point2f centroid = cv::Point2f(0.0f, 0.0f);
    float radius = 0.0f;
    double circularityScore = 0.0;
    bool forceHookDisplay = false;
    double rawCircularity = 0.0;
    int missedFrames = 0;
  };

  enum OutputLanguage { English, Chinese };

  struct lensMetrics {
    bool lensFocusOptimal = false;
    lensDisposition lensDisp = check;
    double lensScore = 0;
    std::vector<contourData> visibleMarkers;
    std::string lensSerial;
    OutputLanguage lang = English;
  };

  // Export the current metrics
  bool ExportMetrics();

  // Accessors for the metrics
  const lensMetrics &getMetrics() const { return m_metrics; }
  const double getLensScore() const { return m_metrics.lensScore; }

  void setLensSerial(const std::string &serial) {
    m_metrics.lensSerial = serial;
    m_snapshot.lensSerial = serial;
  }
  void setLanguage(OutputLanguage lang) {
    m_metrics.lang = lang;
    m_snapshot.lang = lang;
  }
  void setDisposition(lensDisposition disp) { m_metrics.lensDisp = disp; }
  void setFocusOptimal(bool optimal) { m_metrics.lensFocusOptimal = optimal; }

  void setActiveResolution(cv::Size size) {
    imageW = size.width;
    imageH = size.height;
    imageCenter = cv::Point2f(size.width / 2.0f, size.height / 2.0f);
    hypotToCenter = std::hypot(imageCenter.x, imageCenter.y);
    markerMatchDistanceThreshold = static_cast<float>(
        std::max(30.0, std::hypot(imageW, imageH) * 0.06));
  }

  void testMM();

  void addMarker(const contourData &marker) {
    m_metrics.visibleMarkers.push_back(marker);
    UpdateLensDisposition();
  }

  void
  addMarkers(const std::vector<CircleMarkerDetector::CircleMarker> &circles);

  void clearMarkers() {
    m_metrics.visibleMarkers.clear();
    UpdateLensDisposition();
  }

  std::vector<CircleMarkerDetector::CircleMarker> getSmoothedMarkers() const;

private:
  // lens grading vars
  double passingScoreThreshold = 0.90; // minimum score for 'pass'
  double checkingScoreThreshold = 0.78; // minimum score for 'check'; below this is a fail

  // vars relating to image dimensions
  cv::Point2f imageCenter;
  double hypotToCenter;
  int imageW;
  int imageH;

  double markerSmoothingAlpha = 0.2;
  float markerMatchDistanceThreshold = 50.0f;
  int maxMissingFrames = 3;
  std::unordered_map<int, SmoothedMarker> m_smoothedMarkers;

  lensMetrics m_metrics;  // current metrics
  lensMetrics m_snapshot; // metrics snapshot of prior frame

  void UpdateLensDisposition();
  static const char *
  LensDispositionToString(MetricsManager::lensDisposition ds);
  static const char *MarkerClassifierToString(MetricsManager::markerClass mc);
};
