#pragma once

#include <fstream>
#include <string>

#include "MetricsExporter.h"
#include <ios>
#include <qlogging.h>
#include <iterator>

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
	u8"Y（像素)"
};

using lensMetrics = MetricsExporter::lensMetrics;

/// <summary>
/// Given an object containing lens data, export to CSV file
/// </summary>
/// <param name="lm">lensMetrics object</param>
/// <param name="outputPath">Path of output file</param>
/// <returns></returns>
bool MetricsExporter::ExportMetrics(MetricsExporter::lensMetrics lm, std::string outputPath) {
	
	/*
	QString defaultFileName = QString("lens_metrics_%1.csv")
		.arg(QString::number(current_camera->Serial()));

	QString filePath = QFileDialog::getSaveFileName(
		this,
		tr("Export Data"),
		defaultFileName,
		tr("CSV Files (*.csv);;Text Files (*.txt);;All Files (*)")
	);

	QString err;
	if (!MetricsExporter::ExportMetrics(, &err)) {
		emit camera_controls->showWarning("Export Failed", err.isEmpty() ? "Failed to export lens metrics." : err);
	}
	else {
		emit camera_controls->showWarning("Export Successful", "Lens metrics exported successfully.");
	}
	*/

	std::ofstream out(outputPath, std::ios::binary);
	if (!out.is_open()) {
		qDebug("[dbg] File creation failed!");
		return false;
	}

	const char** hTable = nullptr;
	size_t hCount = 0;

	// language check
	if (lm.lang == English) {
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

	for (const auto& d : lm.lensDefects) {
		out << lm.lensSerial << ","
			<< DispositionToString(lm.lensDisp) << ","
			<< DefectTypeToString(d.dType) << ","
			<< d.pxWPosition << ","
			<< d.pxHPosition << ","
			<< std::boolalpha << std::to_string(lm.lensFocusOptimal) << ","
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
