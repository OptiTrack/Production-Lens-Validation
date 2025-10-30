#pragma once

#include "CameraHelpers.h"
#include <opencv2/opencv.hpp>

namespace CameraLibrary {
	float EvaluateBitmapFocus(CameraLibrary::Bitmap* bmp);
	double VarianceOfLaplacian(const cv::Mat& lap);
	cv::Mat ConvertBitmapToMat(CameraLibrary::Bitmap* bmp);
}
