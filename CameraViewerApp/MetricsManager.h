#pragma once
#include <vector>
#include <string>

class MetricsManager {
public:

	enum lensDisposition {
		pass,
		check,
		fail
	};

	enum markerClass {
		hook,
		oval,
		circle,
	};

	struct contourData {
		markerClass mClass;
		int pxWPosition;
		int pxHPosition;
	};

	enum OutputLanguage {
		English,
		Chinese
	};

	struct lensMetrics {
		bool lensFocusOptimal = false;
		lensDisposition lensDisp = check;
		std::vector<contourData> visibleMarkers;
		std::string lensSerial;
		OutputLanguage lang = English;
	};

	// Export the current metrics
	bool ExportMetrics();

	// Accessors for the metrics
	lensMetrics& getMetrics() { return m_metrics; }
	const lensMetrics& getMetrics() const { return m_metrics; }

	void setLensSerial(const std::string& serial) { m_metrics.lensSerial = serial; }
	void setLanguage(OutputLanguage lang) { m_metrics.lang = lang; }
	void setDisposition(lensDisposition disp) { m_metrics.lensDisp = disp; }
	void setFocusOptimal(bool optimal) { m_metrics.lensFocusOptimal = optimal; }
	void addMarker(const contourData& marker) { m_metrics.visibleMarkers.push_back(marker); }
	void clearMarkers() { m_metrics.visibleMarkers.clear(); }

private:
	lensMetrics m_metrics;

	static const char* LensDispositionToString(MetricsManager::lensDisposition ds);
	static const char* MarkerClassifierToString(MetricsManager::markerClass mc);
};