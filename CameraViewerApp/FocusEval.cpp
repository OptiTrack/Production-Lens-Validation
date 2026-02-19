#pragma once

#include <atomic>
#include <vector>

#include <opencv2/core/cvdef.h>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/matx.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <bitmap.h>
#include <algorithm>
#include <deque>
#include <stdexcept>
#include <qlogging.h>


#include "FocusEval.h"
#include "CircleMarkerDetector.h"

using Bitmap = CameraLibrary::Bitmap;
using frameScore = FocusEvaluator::frameScore;

	std::deque<FocusEvaluator::frameScore> frameScoreSet;
	size_t sampleCount = 64;				
	double maxInstanceScore = 0.0;

	const double hiFocusThreshold = 0.95;
	const double midFocusThreshold = 0.6;
	const double loFocusThreshold = 0.3;

	const double decayRate = 0.93; // per evaluation, forget old max val a little bit

	/// <summary>
	/// Converts a bitmap to an openCV mat
	/// </summary>
	/// <param name="bmp"></param>
	/// <returns></returns>
	cv::Mat FocusEvaluator::ConvertBitmapToMat(CameraLibrary::Bitmap* bmp) {
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

	/// <summary>
	/// Given a grayscale Mat, determine avg circularity and area of contours found
	/// </summary>
	/// <param name="bmp">Incoming frame bitmap data</param>
	/// <returns>frameScore struct with quantative focus data</returns>
	frameScore FocusEvaluator::gradeFrame(CameraLibrary::Bitmap* bmp) {

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

		cv::Mat edges;

		const int lowThreshold = 100;
		const int ratio = 3;
		const int kernel_size = 3;

		cv::Mat smoothed;
		cv::GaussianBlur(gray, smoothed, cv::Size(3, 3), 1.0);
		cv::Canny(smoothed, edges, lowThreshold, lowThreshold * ratio, kernel_size);

		/*
		cv::imshow("Canny", edges);
		cv::waitKey(500);
		cv::destroyAllWindows();
		*/

		std::vector<std::vector<cv::Point>> contours;
		std::vector<cv::Vec4i> hierarchy;
		cv::findContours(edges, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

		double totalCircularity = 0.0;
		double totalArea = 0.0;
		int validContours = 0;

		for (const auto& contour : contours) {
			double area = cv::contourArea(contour);
			double perimeter = cv::arcLength(contour, true);

			// Filter out noise / tiny fragments
			if (perimeter > 0 && area > 10.0) {
				double circularity = 100 * (4.0 * CV_PI * area) / (perimeter * perimeter);
				totalCircularity += circularity;
				validContours++;
				totalArea += area;
			}
		}

		FocusEvaluator::frameScore fs{};
		if (validContours > 0) {
			fs.avgContourArea = totalArea / validContours;
			fs.cirularity = totalCircularity / validContours;
			fs.contourCount = validContours;
		}

		return fs;
	}

	/// <summary>
	/// Determines the best focus score in the current set
	/// </summary>
	/// <returns></returns>
	double FocusEvaluator::bestLocalFocus() {

		if (frameScoreSet.empty()) {
			return 0.0;
		}

		double maxScore = 0.0;

		for (const auto& fs : frameScoreSet) {
			// highest circularity / area ratio preferred
			double f = fs.cirularity / (fs.avgContourArea + 1e-6);
			if (f > maxScore) {
				maxScore = f;
			}
		}
		return maxScore;
	}

	/// <summary>
	/// Compares the current frame score against the local optimal score
	/// </summary>
	/// <param name="fs">Focus score values to be compared against current optimal values</param>
	/// <returns></returns>
	double FocusEvaluator::compareScoreToMax(const frameScore& fs) {

		// current frame's focus metric
	    double curr = fs.cirularity / (fs.avgContourArea + 1e-6);

		// Update global max if better focus achieved
		if (curr > maxInstanceScore) {
			maxInstanceScore = curr;
		}

		// compare best from dataset to global max
		double maxFocus = std::max(bestLocalFocus(), maxInstanceScore);

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

		// otherwise, classify quality
		if (ratio >= hiFocusThreshold) {
			qDebug("[dbg] Focus optimal!");
		} 
		else if (ratio >= midFocusThreshold) {
			qDebug("[dbg] Focus close to optimal...");
		} 
		else if (ratio > loFocusThreshold) {
			qDebug("[dbg] Focus improving...");
		} 
		else {
			qDebug("[dbg] Focus poor.");
		}

		// decay max slowly in case we grabbed a transient peak
		double oldMax = maxInstanceScore;
		maxInstanceScore = std::max(curr, maxInstanceScore * decayRate);
		//qDebug("[dbg] Decay %.2f to %.2f", oldMax, maxInstanceScore);

		return ratio;
	}

	/// <summary>
	/// Adds a new score to the set, and removes old items if required
	/// </summary>
	/// <param name="fs">Incoming focus score values</param>
	void FocusEvaluator::addFrameScore(frameScore fs) {
		frameScoreSet.push_back(fs);
		if (frameScoreSet.size() > sampleCount) {
			frameScoreSet.pop_front();
			//qDebug("[dbg] Removed old focus score.");
		}
	}

	/// <summary>
	/// Receives incoming frame and grades it's focus against existing data.
	/// </summary>
	/// <param name="bmp">Incoming frame data from camera</param>
	/// <returns>Double ranging from 0.0 to 1.0, where 1.0 indicates optimal focus</returns>
	double FocusEvaluator::EvaluateBitmapFocus(CameraLibrary::Bitmap* bmp) {
		
		frameScore fs = gradeFrame(bmp);

		/*
		qDebug("\n");
		qDebug("[dbg] Circularity: %.2f", fs.cirularity);
		qDebug("[dbg] Avg contour area: %.2f", fs.avgContourArea);
		qDebug("[dbg] Contours: %d", fs.contourCount);
		qDebug("\n");
		*/
		
		double score = compareScoreToMax(fs);
		addFrameScore(fs);

		return score;
	}

	/// <summary>
	/// Detects circular markers in the bitmap frame using Hough Circle detection
	/// </summary>
	/// <param name="bmp">Input frame bitmap from camera</param>
	/// <returns>Vector of detected circle markers</returns>
	std::vector<CircleMarkerDetector::CircleMarker> FocusEvaluator::DetectCircleMarkers(CameraLibrary::Bitmap* bmp) {
		if (!m_circleDetector) {
			qWarning("[dbg] Circle detector not initialized");
			return std::vector<CircleMarkerDetector::CircleMarker>();
		}

		try {
			auto circles = m_circleDetector->DetectCircles(bmp);
			
			if (!circles.empty()) {
				qDebug("[dbg] Circle Detection: Found %d circles", static_cast<int>(circles.size()));
				for (size_t i = 0; i < circles.size(); ++i) {
					const auto& circle = circles[i];
					qDebug("[dbg]   Circle %zu: center=(%.1f, %.1f), radius=%.1f",
						i, circle.center.x, circle.center.y, circle.radius);
				}
			}

			return circles;
		}
		catch (const std::exception& e) {
			qWarning("[dbg] Circle detection failed: %s", e.what());
			return std::vector<CircleMarkerDetector::CircleMarker>();
		}
	}

	/// <summary>
	/// Changes state of focusToolEnabled (atomic bool) depending on state of focus_button
	/// </summary>
	/// <param name="toggle">State (T/F) of focus_button</param>
	/// <returns>Nothing</returns>
	void FocusEvaluator::onSetFocusTool(bool toggle) {
		this->focusToolEnabled = toggle;
		qDebug("[dbg] focusToolEnabled = %d", toggle);
	}