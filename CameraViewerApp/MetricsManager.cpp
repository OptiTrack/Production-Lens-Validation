#include <fstream>
#include <string>
#include <qfiledialog.h>
#include "MetricsManager.h"
#include <ios>
#include <qlogging.h>
#include <iterator>
#include <qstring.h>
#include <QCoreApplication>

const char* ENHeaders[] = {
	"Serial number",
	"Disposition",
	"Marker appearance",
	"X (px)",
	"Y (px)",
	"Lens focus optimal",
};

const char* CNHeaders[] = {
	u8"序列号",
	u8"处置",
	u8"缺陷类型",
	u8"X（像素)",
	u8"Y（像素)",
	u8"镜头对焦是否最佳？",
};

using lensMetrics = MetricsManager::lensMetrics;

/// <summary>
/// Export the current lens metrics to CSV file
/// </summary>
/// <returns>True if export succeeded, false otherwise</returns>
bool MetricsManager::ExportMetrics() {

	QString defaultFileName = QString("lens_metrics_%1.csv").arg(m_metrics.lensSerial.c_str());

	QString filePath = QFileDialog::getSaveFileName(
		nullptr,
		QCoreApplication::translate("MetricsManager", "Export Data"),
		defaultFileName,
		QCoreApplication::translate("MetricsManager", "CSV Files (*.csv);;Text Files (*.txt);;All Files (*)")
	);

	if (filePath.isEmpty()) {
		return false;
	}

	std::ofstream out(filePath.toStdString(), std::ios::binary);
	if (!out.is_open()) {
		qDebug("[dbg] File creation failed!");
		return false;
	}

	const char** hTable = nullptr;
	size_t hCount = 0;

	// language check
	if (m_metrics.lang == MetricsManager::English) {
		hTable = ENHeaders;
		hCount = std::size(ENHeaders);
	}
	else {
		hTable = CNHeaders;
		hCount = std::size(CNHeaders);
	}

	// write headers
	for (size_t i = 0; i < hCount; i++) {
		if (i) out << ',';
		out << hTable[i];
	}
	out << "\n";

	for (const auto& d : m_metrics.visibleMarkers) {
		out << m_metrics.lensSerial << ","
			<< LensDispositionToString(m_metrics.lensDisp) << ","
			<< MarkerClassifierToString(d.mClass) << ","
			<< d.pxWPosition << ","
			<< d.pxHPosition << ","
			<< (m_metrics.lensFocusOptimal ? "true" : "false") << ","
			<< "\n";
	}

	out.close();
	return true;
}

/// <summary>
/// Convert lens overall disposition to string
/// </summary>
/// <param name="ds"></param>
/// <returns></returns>
const char* MetricsManager::LensDispositionToString(MetricsManager::lensDisposition ds)
{
	switch (ds) {
	case MetricsManager::lensDisposition::pass:
		return "Pass";
	case MetricsManager::lensDisposition::fail:
		return "Fail";
	case MetricsManager::lensDisposition::check:
		return "Check";
	default:
		return "Unknown";
	}
}

/// <summary>
/// Convert defect type enum to string
/// </summary>
/// <param name="dt"></param>
/// <returns></returns>
const char* MetricsManager::MarkerClassifierToString(MetricsManager::markerClass dt)
{
	switch (dt) {
	case MetricsManager::markerClass::hook:
		return "Hook";
	case MetricsManager::markerClass::oval:
		return "Oval";
	case MetricsManager::markerClass::circle:
		return "Circle";
	default:
		return "Unknown";
	}
}
