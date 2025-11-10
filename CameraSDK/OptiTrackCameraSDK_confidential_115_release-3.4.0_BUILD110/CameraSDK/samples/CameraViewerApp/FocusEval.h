#pragma once

#include "CameraHelpers.h"
#include <opencv2/opencv.hpp>

class FocusEvaluator {
public:
	double EvaluateBitmapFocus(CameraLibrary::Bitmap* bmp);
	struct frameScore {
		double cirularity;
		double avgContourArea;
		int contourCount;
	};

private:
	cv::Mat ConvertBitmapToMat(CameraLibrary::Bitmap* bmp);
	frameScore gradeFrame(CameraLibrary::Bitmap* bmp);
	void addFrameScore(frameScore fs);
	double compareScoreToMax(const frameScore& fs);
	double bestLocalFocus();
};

