#include <fstream>
#include <string>
#include <qfiledialog.h>
#include "MetricsManager.h"
#include <ios>
#include <qlogging.h>
#include <iterator>
#include <qstring.h>
#include <opencv2/core/types.hpp>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <QCoreApplication>

const char* ENHeaders[] = {
	"Serial number",
	"Disposition",
	"Marker appearance",
	"Circularity score",
	"Position X (px)",
	"Position Y (px)",
	"Lens focus optimal?",
};

// TODO retranslate these headers
const char* CNHeaders[] = {
	u8"序列号",
	u8"处置方式",
	u8"标记外观",
	u8"圆度评分",
	u8"X 坐标 (像素)",
	u8"Y 坐标 (像素)",
	u8"镜头对焦是否最佳？",
};

using lensMetrics = MetricsManager::lensMetrics;

/// <summary>
/// Export the current lens metrics to CSV file
/// </summary>
/// <returns>True if export succeeded, false otherwise</returns>
bool MetricsManager::ExportMetrics() {

	if (m_metrics.visibleMarkers.empty()) {
		qDebug("[!] No contour data to export!");
		return false;
	}

	QString defaultFileName = QString("lens_metrics_%1.csv").arg(m_metrics.lensSerial.c_str());

	QString filePath = QFileDialog::getSaveFileName(
		nullptr,
		QCoreApplication::translate("MetricsManager", "Export Data"),
		defaultFileName,
		QCoreApplication::translate("MetricsManager", "CSV Files (*.csv);;Text Files (*.txt);;All Files (*)")
	);

	if (filePath.isEmpty()) {
		qDebug("[!] No filename provided!");
		return false;
	}

	std::ofstream out(filePath.toStdString(), std::ios::binary);
	if (!out.is_open()) {
		qDebug("[!] File creation failed!");
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
			<< d.circularityScore << ","
			<< d.centroid.x << ","
			<< d.centroid.y << ","
			<< (m_metrics.lensFocusOptimal ? "true" : "false") << ","
			<< "\n";
	}

	out.close();
	return true;
}

/// <summary>
/// Mock and test lens grading
/// </summary>
void MetricsManager::testMM() {

	// required to test
	m_metrics.lensFocusOptimal = true; 

	// create some contour data, two ovals with same circularity, different distances from the center
	setActiveResolution(cv::Size(640,480));

	contourData oval1 = { markerClass::oval, cv::Point2f(300,220), 0.75 };
	addMarker(oval1);

	// We would expect this contour to be graded higher since it's closer to the center.
	contourData oval2 = { markerClass::oval, cv::Point2f(120,64), 0.75 };
	addMarker(oval2);

	// This marker should fail the lens. No hooks allowed.
	contourData hook1 = { markerClass::hook, cv::Point2f(320,240), 0.38 };
	addMarker(hook1);

	clearMarkers();

	// Some circles
	contourData circle1 = { markerClass::circle, cv::Point2f(320,240), 0.94 };
	addMarker(circle1);
	contourData circle2 = { markerClass::circle, cv::Point2f(320,240), 0.92 };
	addMarker(circle2);
	contourData circle3 = { markerClass::circle, cv::Point2f(320,240), 0.98 };
	addMarker(circle3);
}

/// <summary>
/// Given a collection of markers, evaluate overall lens condition
/// </summary>
void MetricsManager::UpdateLensDisposition() {

	if (m_metrics.visibleMarkers.empty()) {
		m_metrics.lensDisp = lensDisposition::untested;
		m_metrics.lensScore = 0;
		return;
	}

	// any quantity of hooks fail the lens immediately.
	if (std::any_of(
		m_metrics.visibleMarkers.begin(),
		m_metrics.visibleMarkers.end(),
		[](const auto& m) 
		{
			return m.mClass == markerClass::hook;
		})) 
	{
		m_metrics.lensDisp = lensDisposition::fail;
		m_metrics.lensScore = 0;
		qDebug("[dbg] Hook in collection! Lens fails.");
		return;
	}
		
	// score starts at max value, apply penalties as required
	double maxScore = 1.0 * m_metrics.visibleMarkers.size();
	double score = maxScore;

	for (const auto& m : m_metrics.visibleMarkers) {

		double penalty = 0;

		// if oval, determine distance from center the centroid is, and apply scaled penalty based on distance.
		// The further from center, the greater penalty, as we expect more deformation/pincushion there.
		// Scale penalty with circularity for more dynamic scoring.
		if (m.mClass == markerClass::oval) {

			qDebug("\n[dbg] Oval marker at (%.1f, %.1f) with circularity %.2f", m.centroid.x, m.centroid.y, m.circularityScore);

			if (hypotToCenter == 0.0) {
				qDebug("[!] hypotToCenter is zero, setActiveResolution() not called?");
				continue;
			}

			double markerHypotToCenter = std::hypot(std::abs(imageCenter.x - m.centroid.x),
													std::abs(imageCenter.y - m.centroid.y));

			// marker centroid deviation from true image center as weight
			double scaledMultiplier = 1 - (markerHypotToCenter / hypotToCenter); 

			qDebug("[dbg] Max hypot: %.1f, marker hypot: %.1f, Distance weight: %.1f",
				hypotToCenter,
				markerHypotToCenter,
				scaledMultiplier);

			// scale distance with non-circularity
			penalty = scaledMultiplier * (1 - m.circularityScore);

			qDebug("[dbg] Calculated oval marker penalty: %.1f", penalty);
		}

		// if circular, apply penalty proportional to circularity
		else if (m.mClass == markerClass::circle) {
			penalty = (1 - m.circularityScore); // apply unweighted circularity score
			qDebug("[dbg] Calculated circle marker penalty: %.1f", penalty);
		}
		score -= penalty;
	}

	// assign disposition to metrics object
	double scorePct = score / maxScore;
	if (scorePct >= passingScoreThreshold) {
		m_metrics.lensDisp = lensDisposition::pass;
	}
	else if (scorePct >= checkingScoreThreshold) {
		m_metrics.lensDisp = lensDisposition::check;
	}
	else {
		m_metrics.lensDisp = lensDisposition::fail;
	}
	m_metrics.lensScore = scorePct;
	qDebug("[dbg] Overall lens health: %.1f%\n", scorePct * 100.f);
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
	case MetricsManager::lensDisposition::untested:
		return "Untested";
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
