#include <fstream>
#include <string>
#include <qfiledialog.h>
#include "MetricsExporter.h"
#include <ios>
#include <qlogging.h>
#include <iterator>
#include <qstring.h>
#include <qobject.h>
#include <QCoreApplication>

const char* ENHeaders[] = {
	"Serial number",
	"Disposition",
	"Defect type",
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

using lensMetrics = MetricsExporter::lensMetrics;

/// <summary>
/// Export the current lens metrics to CSV file
/// </summary>
/// <returns>True if export succeeded, false otherwise</returns>
bool MetricsExporter::ExportMetrics() {

	QString defaultFileName = QString("lens_metrics_%1.csv").arg(m_metrics.lensSerial.c_str());

	QString filePath = QFileDialog::getSaveFileName(
		nullptr,
		QCoreApplication::translate("MetricsExporter", "Export Data"),
		defaultFileName,
		QCoreApplication::translate("MetricsExporter", "CSV Files (*.csv);;Text Files (*.txt);;All Files (*)")
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
	if (m_metrics.lang == MetricsExporter::English) {
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

	for (const auto& d : m_metrics.lensDefects) {
		out << m_metrics.lensSerial << ","
			<< DispositionToString(m_metrics.lensDisp) << ","
			<< DefectTypeToString(d.dType) << ","
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
const char* MetricsExporter::DispositionToString(MetricsExporter::lensDisposition ds)
{
	switch (ds) {
	case MetricsExporter::lensDisposition::pass:
		return "Pass";
	case MetricsExporter::lensDisposition::fail:
		return "Fail";
	case MetricsExporter::lensDisposition::check:
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
const char* MetricsExporter::DefectTypeToString(MetricsExporter::defectType dt)
{
	switch (dt) {
	case MetricsExporter::defectType::hook:
		return "Hook";
	case MetricsExporter::defectType::oval:
		return "Oval";
	case MetricsExporter::defectType::none:
		return "None";
	default:
		return "Unknown";
	}
}
