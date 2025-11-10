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

	/// <summary>
	/// Given a grayscale Mat, determine avg circularity of contours found
	/// </summary>
	/// <param name="gray"></param>
	/// <returns></returns>
	double AverageCircularityScore(const cv::Mat& gray) {
		cv::Mat thresh;
		cv::threshold(gray, thresh, 150, 255, cv::THRESH_BINARY);

		std::vector<std::vector<cv::Point>> contours;
		std::vector<cv::Vec4i> hierarchy;
		cv::findContours(thresh, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

		double totalCircularity = 0.0;
		int validContours = 0;

		// todo: will it be necessary to know the number of anticipated markers?
		const int expectedMarkers = 3;

		for (const auto& contour : contours) {
			double area = cv::contourArea(contour);
			double perimeter = cv::arcLength(contour, true);

			if (perimeter > 0 && area > 10.0) { 
				double circularity = (4.0 * CV_PI * area) / (perimeter * perimeter);
				totalCircularity += circularity;
				validContours++;
			}
		}

		// visualization
		/*
		cv::Mat image_copy;
		cv::cvtColor(gray, image_copy, cv::COLOR_GRAY2BGR);
		cv::drawContours(image_copy, contours, -1, cv::Scalar(0, 255, 0), 2);
		cv::imshow("Contours", image_copy);
		cv::waitKey(1000);
		cv::destroyAllWindows();
		*/

		if (validContours == 0)
			return 0.0;

		return totalCircularity / expectedMarkers;
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
		double circScore = AverageCircularityScore(gray);
		qDebug("Circularity score: %.2f", circScore);

		double variance = VarianceOfLaplacian(gray);
		return static_cast<float>(variance);
	}

}

