#pragma once

#include <atomic>
#include <vector>

#include <algorithm>
#include <bitmap.h>
#include <deque>
#include <opencv2/core/cvdef.h>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/matx.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <qlogging.h>
#include <stdexcept>
#include "CircleMarkerDetector.h"
#include "FocusEval.h"

using Bitmap = CameraLibrary::Bitmap;
using frameScore = FocusEvaluator::frameScore;

const int minContourCount = 3;
const double loFocusThreshold = 0.3;
const double decayRate = 0.93; // per evaluation, forget old max val a little bit
const double ewm_alpha = 0.2;

/// <summary>
/// Given a grayscale Mat, determine avg circularity and area of contours found
/// </summary>
/// <param name="bmp">Incoming frame bitmap data</param>
/// <returns>frameScore struct with quantative focus data</returns>
frameScore FocusEvaluator::gradeFrame(const std::vector<CircleMarkerDetector::CircleMarker>& circles) {

  double weightedCircularity = 0.0;
  double totalArea = 0.0;
  int validContours = 0;

  // Check for corrupt collection, which should never exceed 10k contours.
  if (circles.size() > 10000) {
    return FocusEvaluator::frameScore{};
  }

  for (const auto &circle : circles) {
    if (!circle.isValid) continue;
    const auto &contour = circle.contour;
    if (contour.size() < 5) continue;

    try {
      double area = cv::contourArea(contour);
      double perimeter = cv::arcLength(contour, true);

      // Filter out noise / tiny fragments
      if (perimeter > 0 && area > 10.0) {
        double circularity =
            100 * (4.0 * CV_PI * area) / (perimeter * perimeter);
        weightedCircularity += circularity * area;
        validContours++;
        totalArea += area;
      }
    } catch (const cv::Exception &) {
      continue;
    }
  }

  FocusEvaluator::frameScore fs{};
  if (validContours > 0 && totalArea > 0.0) {
    fs.avgContourArea = totalArea / validContours;
    fs.circularity = weightedCircularity / totalArea;
    fs.contourCount = validContours;
  }
  return fs;

}

/// <summary>
/// Determines the best focus score in the current set
/// </summary>
/// <returns></returns>
double FocusEvaluator::getBestLocalFocus() {

  if (frameScoreSet.empty()) {
    return 0.0;
  }

  std::vector<double> scores;
  scores.reserve(frameScoreSet.size());
  for (const auto &fs : frameScoreSet) {
    scores.push_back(fs.circularity / (fs.avgContourArea + 1e-6));
  }
  // sort and retrieve 90th percentile values, ignoring outlier peaks
  std::sort(scores.begin(), scores.end());
  size_t idx = static_cast<size_t>(scores.size() * 0.9);
  if (idx >= scores.size())
    idx = scores.size() - 1;
  return scores[idx];
}

/// <summary>
/// Compares the current frame score against the local optimal score
/// </summary>
/// <param name="fs">Focus score values to be compared against current optimal
/// values</param> <returns></returns>
double FocusEvaluator::compareScoreToMax(const frameScore &fs) {

  // Reject frames with too few contours - score would be unreliable
  if (fs.contourCount < minContourCount) {
    return smoothedRatio;
  }

  // current frame's focus metric
  double curr = fs.circularity / (fs.avgContourArea + 1e-6);

  // Update global max if better focus achieved
  if (curr > maxInstanceScore) {
    maxInstanceScore = curr;
  }

  // compare best from dataset to global max (percentile-based baseline)
  double maxFocus = std::max(getBestLocalFocus(), maxInstanceScore);

  // Avoid division by near-zero
  if (maxFocus < 1e-6) {
    return 0.0;
  }

  double ratio = std::clamp(curr / maxFocus, 0.0, 1.0);

  // --- Cold-start guard ---
  // Don't report "optimal" if we haven't seen a strong enough absolute max yet.
  bool confident = (maxFocus >= loFocusThreshold);

  // return early if we've not seen enough data
  if (!confident) {
    qDebug("[dbg] Focus baseline not established (maxFocus=%.3f)", maxFocus);
    return maxFocus;
  }

  // decay max slowly in case we grabbed a transient peak
  double oldMax = maxInstanceScore;
  maxInstanceScore = std::max(curr, maxInstanceScore * decayRate);
  // qDebug("[dbg] Decay %.2f to %.2f", oldMax, maxInstanceScore);

  // apply EMA smoothing to ratio
  smoothedRatio = ewm_alpha * ratio + (1.0 - ewm_alpha) * smoothedRatio;

  return smoothedRatio;
}

/// <summary>
/// Adds a new score to the set, and removes old items if required
/// </summary>
/// <param name="fs">Incoming focus score values</param>
void FocusEvaluator::addFrameScore(frameScore fs) {
  frameScoreSet.push_back(fs);
  if (frameScoreSet.size() > sampleCount) {
    frameScoreSet.pop_front();
  }
}

/// <summary>
/// Clears existing focus data, to reset the tool to a "cold start" state.
/// Necessary when video mode is changed.
/// </summary>
void FocusEvaluator::onResetFocusStats() {
  qDebug("[dbg] FocusEvaluator reset: clearing data and resetting max score");
  std::lock_guard<std::mutex> lock(scoreMutex);
  frameScoreSet.clear();
  maxInstanceScore = 0.0;
  smoothedRatio = 0.0;
}

/// <summary>
/// Receives incoming frame and grades it's focus against existing data.
/// </summary>
/// <param name="bmp">Incoming frame data from camera</param>
/// <returns>Double ranging from 0.0 to 1.0, where 1.0 indicates optimal
/// focus</returns>
double FocusEvaluator::EvaluateBitmapFocus(const std::vector<CircleMarkerDetector::CircleMarker>& circles) {
  frameScore fs = gradeFrame(circles);
  std::lock_guard<std::mutex> lock(scoreMutex);
  double score = compareScoreToMax(fs);
  addFrameScore(fs);
  return score;
}

/// <summary>
/// Changes state of focusToolEnabled (atomic bool) depending on state of
/// focus_button
/// </summary>
/// <param name="toggle">State (T/F) of focus_button</param>
/// <returns>Nothing</returns>
void FocusEvaluator::onSetFocusTool(bool toggle) {
  this->focusToolEnabled = toggle;
  qDebug("[dbg] focusToolEnabled = %d", toggle);
}
