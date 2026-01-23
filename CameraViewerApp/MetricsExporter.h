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
		bool lensFocusOptimal;
		lensDisposition lensDisp;
		std::vector<lensDefect> lensDefects;
		std::string lensSerial;
		OutputLanguage lang;
	};

	static bool ExportMetrics(MetricsExporter::lensMetrics lm, std::string outputPath);

private:
	static const char* DispositionToString(MetricsExporter::lensDisposition ds);
	static const char* DefectTypeToString(MetricsExporter::defectType dt);
};