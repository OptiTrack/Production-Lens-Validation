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
  for (const auto &circle : circles) {
    contourData circ = {
        (circle.shapeType == CircleMarkerDetector::ShapeType::Circle)
            ? markerClass::circle
        : (circle.shapeType == CircleMarkerDetector::ShapeType::Oval)
            ? markerClass::oval
        : (circle.shapeType == CircleMarkerDetector::ShapeType::Hook)
            ? markerClass::hook
            : markerClass::circle,
        circle.center, circle.circularity};
    m_metrics.visibleMarkers.push_back(circ);
  }

  UpdateLensDisposition();
}

/// <summary>
/// Given a collection of markers, evaluate overall lens condition.
/// Saves a snapshot of the result whenever markers are present so
/// ExportMetrics() always has data regardless of render loop timing.
/// </summary>
void MetricsManager::UpdateLensDisposition() {

  if (m_metrics.visibleMarkers.empty()) {
    m_metrics.lensDisp = lensDisposition::untested;
    m_metrics.lensScore = 0;
    return;
  }

  const double penaltyExponent = 1.0;
  double totalPenalty = 0.0;

  for (auto &m : m_metrics.visibleMarkers) {

    // hooks = immediate fail
    if (m.mClass == markerClass::hook) {
      qDebug("[dbg] Hook marker at (%.2f, %.2f) — LENS FAIL",
              m.centroid.x, m.centroid.y);
      m_metrics.lensDisp = lensDisposition::fail;
      m_metrics.lensScore = 0;
      m_snapshot = m_metrics;
      return;
    }

    double error = 1.0 - m.circularityScore;
    double penalty = std::pow(error, penaltyExponent);
    totalPenalty += penalty;

    double markerHypotToCenter =
        std::hypot(std::abs(imageCenter.x - m.centroid.x),
            std::abs(imageCenter.y - m.centroid.y));

    // if oval, determine distance from center the centroid is, and apply scaled
    // penalty based on distance. The further from center, the greater penalty,
    // as we expect more deformation/pincushion there. Scale penalty with
    // circularity for more dynamic scoring.
    if (m.mClass == markerClass::oval) {

      qDebug("\n[dbg] Oval marker at (%.2f, %.2f) with circularity %.2f",
             m.centroid.x, m.centroid.y, m.circularityScore);

      if (hypotToCenter == 0.0) {
        qDebug("[!] hypotToCenter is zero, setActiveResolution() not called?");
        continue;
      }

      // marker centroid deviation from true image center as weight
      // added scaling value to increase severity
      double scaledMultiplier = 2 * 1 - (markerHypotToCenter / hypotToCenter);

      qDebug("[dbg] Max hypot: %.2f, marker hypot to center: %.2f, Distance "
             "weight: %.2f",
             hypotToCenter, markerHypotToCenter, scaledMultiplier);

      // scale distance with non-circularity
      penalty = scaledMultiplier * (1 - m.circularityScore);

      qDebug("[dbg] Calculated oval marker penalty: %.2f", penalty);
    }

    // if circular, apply penalty proportional to circularity
    else if (m.mClass == markerClass::circle) {
      penalty = (1 - m.circularityScore); // apply unweighted circularity score
      qDebug("[dbg] Calculated circle marker penalty: %.2f", penalty);
    }

    totalPenalty += penalty;

    qDebug("[dbg] Marker (%s) at (%.2f, %.2f): circularity=%.2f, error=%.2f, penalty=%.4f",
           MarkerClassifierToString(m.mClass), m.centroid.x, m.centroid.y,
           m.circularityScore, error, penalty);
  }

  // Squared-error penalty makes multiple lower-scoring markers worse than a
  // single marker at the same score, and makes lower scores hurt faster.
  double scorePct = std::max(0.0, 1.0 - totalPenalty);
  if (scorePct >= passingScoreThreshold) {
    m_metrics.lensDisp = lensDisposition::pass;
  } else if (scorePct >= checkingScoreThreshold) {
    m_metrics.lensDisp = lensDisposition::check;
  } else {
    m_metrics.lensDisp = lensDisposition::fail;
  }
  m_metrics.lensScore = scorePct;
  qDebug("[dbg] Overall lens health: %.2f%%\n", scorePct * 100.0);

  // Retain this result so ExportMetrics() always has the last valid frame data
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
