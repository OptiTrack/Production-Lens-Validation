#include "MetricsManager.h"
#include <QCoreApplication>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <iterator>
#include <opencv2/core/types.hpp>
#include <qfiledialog.h>
#include <qlogging.h>
#include <qmessagebox.h>
#include <qstring.h>
#include <string>

const char *ENHeaders[] = {
    "Lens Serial Number",  "Lens Disposition", "Marker Appearance",
    "Circularity Score",   "Position X (px)",  "Position Y (px)",
    "Lens focus optimal?",
};

// TODO retranslate these headers
const char *CNHeaders[] = {
    u8"序列号",        u8"镜头布局",      u8"标记外观",           u8"圆度评分",
    u8"X 坐标 (像素)", u8"Y 坐标 (像素)", u8"镜头对焦是否最佳？",
};

using lensMetrics = MetricsManager::lensMetrics;

/// <summary>
/// Export the current lens metrics to CSV file
/// </summary>
/// <returns>True if export succeeded, false otherwise</returns>
bool MetricsManager::ExportMetrics() {

  if (m_snapshot.visibleMarkers.empty()) {
    QMessageBox::warning(nullptr, "No data!",
                         "No lens data is available for export.\rCheck lens "
                         "focus and try again.");
    return false;
  }

  QString defaultFileName =
      QString("lens_metrics_%1.csv").arg(m_snapshot.lensSerial.c_str());

  QString filePath = QFileDialog::getSaveFileName(
      nullptr, QCoreApplication::translate("MetricsManager", "Export Data"),
      defaultFileName,
      QCoreApplication::translate(
          "MetricsManager",
          "CSV Files (*.csv);;Text Files (*.txt);;All Files (*)"));

  if (filePath.isEmpty()) {
    QMessageBox::warning(nullptr, "No output path!",
                         "No output path has been selected for export.\rSelect "
                         "an output folder and try again.");
    return false;
  }

  std::ofstream out(filePath.toStdString(), std::ios::binary);
  if (!out.is_open()) {
    QMessageBox::critical(nullptr, "Failed to create file!",
                          "Could not create export file.\rCheck folder "
                          "permissions and try again.");
    return false;
  }

  const char **hTable = nullptr;
  size_t hCount = 0;

  if (m_snapshot.lang == MetricsManager::English) {
    hTable = ENHeaders;
    hCount = std::size(ENHeaders);
  } else {
    hTable = CNHeaders;
    hCount = std::size(CNHeaders);
  }

  // write headers
  for (size_t i = 0; i < hCount; i++) {
    if (i)
      out << ',';
    out << hTable[i];
  }
  out << "\n";

  for (const auto &d : m_snapshot.visibleMarkers) {
    out << m_snapshot.lensSerial << ","
        << LensDispositionToString(m_snapshot.lensDisp) << ","
        << MarkerClassifierToString(d.mClass) << "," << d.circularityScore
        << "," << d.centroid.x << "," << d.centroid.y << ","
        << (m_snapshot.lensFocusOptimal ? "true" : "false") << ","
        << "\n";
  }

  out.close();
  return true;
}

/// <summary>
/// Mock and test lens grading
/// </summary>
void MetricsManager::testMM() {

  // required to test
  m_metrics.lensFocusOptimal = true;

  // create some contour data, two ovals with same circularity, different
  // distances from the center
  setActiveResolution(cv::Size(640, 480));

  std::vector<CircleMarkerDetector::CircleMarker> collA = {
      {cv::Point2f(300, 220),
       0.0f,
       true,
       0.75f,
       CircleMarkerDetector::ShapeType::Oval,
       {},
       1.0f,
       0},
      {cv::Point2f(120, 64),
       0.0f,
       true,
       0.75f,
       CircleMarkerDetector::ShapeType::Oval,
       {},
       1.0f,
       1},
      {cv::Point2f(320, 240),
       0.0f,
       false,
       0.38f,
       CircleMarkerDetector::ShapeType::Hook,
       {},
       1.0f,
       2}};

  addMarkers(collA);

  clearMarkers();

  // Some circles
  std::vector<CircleMarkerDetector::CircleMarker> collB = {
      {cv::Point2f(320, 240),
       0.0f,
       true,
       0.94f,
       CircleMarkerDetector::ShapeType::Circle,
       {},
       1.0f,
       0},
      {cv::Point2f(320, 240),
       0.0f,
       true,
       0.92f,
       CircleMarkerDetector::ShapeType::Circle,
       {},
       1.0f,
       1},
      {cv::Point2f(320, 240),
       0.0f,
       true,
       0.98f,
       CircleMarkerDetector::ShapeType::Circle,
       {},
       1.0f,
       2}};
  addMarkers(collB);

  clearMarkers();
}

/// <summary>
/// Convert and add all circles from a detection pass, then evaluate once.
/// </summary>
void MetricsManager::addMarkers(
    const std::vector<CircleMarkerDetector::CircleMarker> &circles) {
  m_metrics.visibleMarkers.clear();

  auto classFromScore = [](float score) -> markerClass {
    if (score > 0.92f)  return markerClass::circle;
    if (score >= 0.70f) return markerClass::oval;
    return markerClass::hook;
  };

  std::unordered_set<int> currentIds;

  for (const auto &circle : circles) {
    auto it = m_smoothedMarkers.find(circle.id);

    if (it == m_smoothedMarkers.end()) {
      SmoothedMarker sm;
      sm.id               = circle.id;
      sm.centroid         = circle.center;
      sm.radius           = circle.radius;
      sm.circularityScore = circle.circularity;
      sm.rawCircularity   = circle.circularity;
      sm.mClass           = classFromScore(circle.circularity);
      sm.forceHookDisplay = (sm.mClass == markerClass::hook);
      it = m_smoothedMarkers.emplace(circle.id, sm).first;
    } else {
      SmoothedMarker &sm = it->second;
      bool rawHook = (classFromScore(circle.circularity) == markerClass::hook);
      sm.centroid = cv::Point2f(
          static_cast<float>(markerSmoothingAlpha * circle.center.x +
                             (1.0 - markerSmoothingAlpha) * sm.centroid.x),
          static_cast<float>(markerSmoothingAlpha * circle.center.y +
                             (1.0 - markerSmoothingAlpha) * sm.centroid.y));
      sm.radius = static_cast<float>(markerSmoothingAlpha * circle.radius +
                                     (1.0 - markerSmoothingAlpha) * sm.radius);
      if (rawHook) {
        sm.forceHookDisplay = true;
        sm.rawCircularity   = circle.circularity;
      } else {
        sm.forceHookDisplay  = false;
        sm.circularityScore  = markerSmoothingAlpha * circle.circularity +
                               (1.0 - markerSmoothingAlpha) * sm.circularityScore;
        sm.mClass = classFromScore(sm.circularityScore);
      }
    }

    currentIds.insert(circle.id);
    const SmoothedMarker &sm = it->second;
    contourData cd;
    cd.id              = sm.id;
    cd.mClass          = sm.forceHookDisplay ? markerClass::hook : sm.mClass;
    cd.centroid        = sm.centroid;
    cd.circularityScore = sm.forceHookDisplay ? sm.rawCircularity : sm.circularityScore;
    m_metrics.visibleMarkers.push_back(cd);
  }

  for (auto it = m_smoothedMarkers.begin(); it != m_smoothedMarkers.end();) {
    if (currentIds.count(it->first)) {
      ++it;
    } else if (++it->second.missedFrames > maxMissingFrames) {
      it = m_smoothedMarkers.erase(it);
    } else {
      ++it;
    }
  }

  UpdateLensDisposition();
}

std::vector<CircleMarkerDetector::CircleMarker>
MetricsManager::getSmoothedMarkers() const {
  std::vector<CircleMarkerDetector::CircleMarker> markers;
  markers.reserve(m_smoothedMarkers.size());

  for (const auto &entry : m_smoothedMarkers) {
    CircleMarkerDetector::CircleMarker marker;
    const SmoothedMarker &sm = entry.second;
    marker.id = sm.id;
    marker.center = sm.centroid;
    marker.radius = sm.radius;
    marker.isValid = true;
    marker.circularity = static_cast<float>(sm.forceHookDisplay
                                                ? sm.rawCircularity
                                                : sm.circularityScore);
    auto toShapeType = [](markerClass mc) {
      switch (mc) {
        case markerClass::circle: return CircleMarkerDetector::ShapeType::Circle;
        case markerClass::oval:   return CircleMarkerDetector::ShapeType::Oval;
        default:                  return CircleMarkerDetector::ShapeType::Hook;
        }
    };
    marker.shapeType = sm.forceHookDisplay ? CircleMarkerDetector::ShapeType::Hook
                                          : toShapeType(sm.mClass);
    marker.quality = marker.circularity;
    markers.push_back(marker);
  }

  std::sort(markers.begin(), markers.end(), [](const CircleMarkerDetector::CircleMarker &a,
                                              const CircleMarkerDetector::CircleMarker &b) {
    return a.id < b.id;
  });

  return markers;
}

/// <summary>
/// Given a collection of markers, evaluate overall lens condition.
/// Saves a snapshot of the result whenever markers are present so
/// ExportMetrics() always has data regardless of render loop timing.
/// </summary>
void MetricsManager::UpdateLensDisposition() {
  if (m_metrics.visibleMarkers.empty()) {
    m_metrics.lensDisp  = lensDisposition::untested;
    m_metrics.lensScore = 0.0;
    return;
  }

  // Any hook is an immediate fail — skip scoring entirely
  for (const auto &m : m_metrics.visibleMarkers) {
    if (m.mClass == markerClass::hook) {
      m_metrics.lensDisp  = lensDisposition::fail;
      m_metrics.lensScore = 0.0;
      m_snapshot = m_metrics;
      return;
    }
  }

  if (hypotToCenter == 0.0) {
    qDebug("[!] hypotToCenter is zero — setActiveResolution() not called?");
    return;
  }

  double maxScore = static_cast<double>(m_metrics.visibleMarkers.size());
  double score    = maxScore;

  for (const auto &m : m_metrics.visibleMarkers) {
    double markerHypot = std::hypot(imageCenter.x - m.centroid.x,
                                    imageCenter.y - m.centroid.y);

    // Central markers scored more strictly; edge markers more leniently,
    // since pincushion distortion naturally affects the periphery more.
    double distanceWeight = 2.0 - (markerHypot / hypotToCenter);
    double penalty        = distanceWeight * (1.0 - m.circularityScore);

    qDebug("[dbg] Marker %d (%.2f, %.2f): circularity=%.2f weight=%.2f penalty=%.2f",
           m.id, m.centroid.x, m.centroid.y, m.circularityScore, distanceWeight, penalty);

    score -= penalty;
  }

  score = std::clamp(score, 0.0, maxScore);
  double scorePct = score / maxScore;

  if (scorePct >= passingScoreThreshold) {
    m_metrics.lensDisp = lensDisposition::pass;
  } else if (scorePct >= checkingScoreThreshold) {
    m_metrics.lensDisp = lensDisposition::check;
  } else {
    m_metrics.lensDisp = lensDisposition::fail;
  }

  m_metrics.lensScore = scorePct;
  qDebug("[dbg] Overall lens health: %.2f%%\n", scorePct * 100.0);
  m_snapshot = m_metrics;
}

/// <summary>
/// Convert lens overall disposition to string
/// </summary>
/// <param name="ds"></param>
/// <returns></returns>
const char *
MetricsManager::LensDispositionToString(MetricsManager::lensDisposition ds) {
  switch (ds) {
  case MetricsManager::lensDisposition::pass:
    return "Pass";
  case MetricsManager::lensDisposition::fail:
    return "Fail";
  case MetricsManager::lensDisposition::check:
    return "Check";
  case MetricsManager::lensDisposition::untested:
    return "Untested";
  default:
    return "Unknown";
  }
}

/// <summary>
/// Convert defect type enum to string
/// </summary>
/// <param name="dt"></param>
/// <returns></returns>
const char *
MetricsManager::MarkerClassifierToString(MetricsManager::markerClass dt) {
  switch (dt) {
  case MetricsManager::markerClass::hook:
    return "Hook";
  case MetricsManager::markerClass::oval:
    return "Oval";
  case MetricsManager::markerClass::circle:
    return "Circle";
  default:
    return "Unknown";
  }
}
