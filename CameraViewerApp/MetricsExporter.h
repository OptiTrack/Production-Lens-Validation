#pragma once
#include <vector>
#include <string>

class MetricsExporter {
public:

	enum lensDisposition {
		pass,
		check,
		fail
	};

	enum defectType {
		hook,
		oval,
		none
	};

	struct lensDefect {
		defectType dType;
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
		std::vector<lensDefect> lensDefects;
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
	void addDefect(const lensDefect& defect) { m_metrics.lensDefects.push_back(defect); }
	void clearDefects() { m_metrics.lensDefects.clear(); }

private:
	lensMetrics m_metrics;

	static const char* DispositionToString(MetricsExporter::lensDisposition ds);
	static const char* DefectTypeToString(MetricsExporter::defectType dt);
};