#pragma once

#include "CameraHelpers.h"
#include "CircleMarkerDetector.h"
#include <opencv2/opencv.hpp>
#include <memory>
#include <atomic>

#include <QObject>

class FocusEvaluator : public QObject {
	Q_OBJECT
public:
	double EvaluateBitmapFocus(CameraLibrary::Bitmap* bmp);

	/// @brief Detect circular markers in a frame
	/// @param bmp Bitmap frame to analyze
	/// @return Vector of detected circle markers
	std::vector<CircleMarkerDetector::CircleMarker> DetectCircleMarkers(CameraLibrary::Bitmap* bmp);

	/// @brief Get the circle detector instance
	/// @return Shared pointer to the CircleMarkerDetector
	std::shared_ptr<CircleMarkerDetector> GetCircleDetector() const { return m_circleDetector; }

	struct frameScore {
		double cirularity;
		double avgContourArea;
		int contourCount;
	};
	std::atomic_bool focusToolEnabled; // changed by focus UI control; set True by default

public slots:
    void onSetFocusTool(bool toggle);

private:
	cv::Mat ConvertBitmapToMat(CameraLibrary::Bitmap* bmp);
	frameScore gradeFrame(CameraLibrary::Bitmap* bmp);
	void addFrameScore(frameScore fs);
	double compareScoreToMax(const frameScore& fs);
	double bestLocalFocus();

	std::shared_ptr<CircleMarkerDetector> m_circleDetector{std::make_shared<CircleMarkerDetector>()};
};

