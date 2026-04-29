#pragma once

#include "CameraHelpers.h"
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <opencv2/opencv.hpp>
#include "CircleMarkerDetector.h"

#include <QObject>

class FocusEvaluator : public QObject {
  Q_OBJECT
public:
  double EvaluateBitmapFocus(const std::vector<CircleMarkerDetector::CircleMarker>& circles);

  struct frameScore {
    double circularity;
    double avgContourArea;
    int contourCount;
  };
  std::atomic_bool
      focusToolEnabled; // changed by focus UI control; set True by default

public slots:
  void onSetFocusTool(bool toggle);
  void onResetFocusStats();

private:
  frameScore gradeFrame(const std::vector<CircleMarkerDetector::CircleMarker>& circles);
  void addFrameScore(frameScore fs);
  double compareScoreToMax(const frameScore &fs);
  double getBestLocalFocus();

  // Shared & mutable scoring state - accessed from both the QtConcurrent worker
  // during EvaluateBitmapFocus and the main thread (onResetFocusStats).
  std::mutex scoreMutex;
  std::deque<frameScore> frameScoreSet;
  static constexpr size_t sampleCount = 65535;
  double maxInstanceScore{0.0};
  double smoothedRatio{0.0};
};
