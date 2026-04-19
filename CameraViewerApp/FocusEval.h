#pragma once

#include "CameraHelpers.h"
#include <atomic>
#include <memory>
#include <opencv2/opencv.hpp>

#include <QObject>

class FocusEvaluator : public QObject {
  Q_OBJECT
public:
  double EvaluateBitmapFocus(CameraLibrary::Bitmap *bmp);

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
  cv::Mat ConvertBitmapToMat(CameraLibrary::Bitmap *bmp);
  frameScore gradeFrame(CameraLibrary::Bitmap *bmp);
  void addFrameScore(frameScore fs);
  double compareScoreToMax(const frameScore &fs);
  double getBestLocalFocus();
};
