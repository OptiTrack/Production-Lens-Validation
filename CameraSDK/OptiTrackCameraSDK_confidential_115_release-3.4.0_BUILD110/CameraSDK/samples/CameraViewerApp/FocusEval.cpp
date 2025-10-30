#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <optional>
#include <memory>
#include <vector>
#include <cstdlib>

#include <opencv2/opencv.hpp>

#include <QApplication>
#include <QMetaObject>

#include "cameralibrary.h"

namespace CameraLibrary {

	cv::Mat ConvertBitmapToMat(CameraLibrary::Bitmap* bmp) {
		int width = bmp->PixelWidth();
		int height = bmp->PixelHeight();
		int bpp = bmp->GetBitsPerPixel();
		unsigned char* bits = bmp->GetBits();
		int cvType = (bpp == 8) ? CV_8UC1 :
			(bpp == 24) ? CV_8UC3 :
			(bpp == 32) ? CV_8UC4 : -1;
		if (cvType == -1) {
			throw std::runtime_error("Unsupported bitmap color depth");
		}

		cv::Mat mat(height, width, cvType, const_cast<unsigned char*>(bits));

		return mat.clone(); // clone to ensure ownership
	}

	double VarianceOfLaplacian(const cv::Mat& gray)
	{
		cv::Mat lap;
		cv::Laplacian(gray, lap, CV_64F); // CV_64F for numerical stability

		cv::Scalar mean, stddev;
		cv::meanStdDev(lap, mean, stddev);
		return stddev[0] * stddev[0];
	}

	float EvaluateBitmapFocus(CameraLibrary::Bitmap* bmp) {

		cv::Mat img = ConvertBitmapToMat(bmp);

		cv::Mat gray;
		if (img.channels() == 3) {
			cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
		}
		else if (img.channels() == 4) {
			cv::cvtColor(img, gray, cv::COLOR_BGRA2GRAY);
		}
		else {
			gray = img;
		}

		//cv::GaussianBlur(gray, gray, cv::Size(3, 3), 0);

		double variance = VarianceOfLaplacian(gray);
		return static_cast<float>(variance);
	}

}

