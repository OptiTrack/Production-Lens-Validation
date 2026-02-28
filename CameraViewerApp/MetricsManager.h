#pragma once
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core/types.hpp>

class MetricsManager {
public:

	enum lensDisposition {
		pass,
		check,
		fail,
		untested
	};

	enum markerClass {
		hook,
		oval,
		circle,
	};

	struct contourData {
		markerClass mClass;			// appearance classification of contour
		cv::Point2f centroid;		// defines position of contour in image
		double circularityScore;	// closeness of contour to perfect circle (1.0 max)
	};

	enum OutputLanguage {
		English,
		Chinese
	};

	struct lensMetrics {
		bool lensFocusOptimal = false;
		lensDisposition lensDisp = check;
		double lensScore = 0;
		std::vector<contourData> visibleMarkers;
		std::string lensSerial;
		OutputLanguage lang = English;
	};

	// Export the current metrics
	bool ExportMetrics();

	// Accessors for the metrics
	const lensMetrics& getMetrics() const { return m_metrics; }

	void setLensSerial(const std::string& serial) { m_metrics.lensSerial = serial; }
	void setLanguage(OutputLanguage lang) { m_metrics.lang = lang; }
	void setDisposition(lensDisposition disp) { m_metrics.lensDisp = disp; }
	void setFocusOptimal(bool optimal) { m_metrics.lensFocusOptimal = optimal; }

	void setActiveResolution(cv::Size size) { 
		imageW = size.width; 
		imageH = size.height; 
		imageCenter = cv::Point2f(size.width / 2.0f, size.height / 2.0f);
		hypotToCenter = std::hypot(imageCenter.x, imageCenter.y);
	}

	void testMM();

	void addMarker(const contourData& marker) {
		m_metrics.visibleMarkers.push_back(marker);

		// only evaluate if focus is optimal, no value otherwise.
		if (m_metrics.lensFocusOptimal) {
			UpdateLensDisposition();
		}
	}

	void clearMarkers() { 
		m_metrics.visibleMarkers.clear(); 
		UpdateLensDisposition();
	}

private:

	// lens grading vars
	double passingScoreThreshold = 0.90;	// minimum score for 'pass'
	double checkingScoreThreshold = 0.78;	// minimum score for 'check'; below this is a fail

	// vars relating to image dimensions
	cv::Point2f imageCenter;
	double hypotToCenter;
	int imageW;
	int imageH;
																	 
	lensMetrics m_metrics;
	void UpdateLensDisposition();
	static const char* LensDispositionToString(MetricsManager::lensDisposition ds);
	static const char* MarkerClassifierToString(MetricsManager::markerClass mc);
};