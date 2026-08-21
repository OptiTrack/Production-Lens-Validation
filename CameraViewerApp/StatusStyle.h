#pragma once

// ---------------------------------------------------------------------------
// Status styling helper.
//
// Result and metric readouts change colour to signal severity. Doing that with
// setStyleSheet() on every update means Qt reparses a stylesheet for that widget
// each time - once per camera frame in the focus readout - and it scatters
// hard coded colours through the UI code.
//
// Instead, set a semantic state and let Designs/motive.css decide the colour:
//
//     ui::setStatus(label, ui::Status::Pass);
//
// The matching rules are the QLabel[status="..."] block in motive.css.
// ---------------------------------------------------------------------------

#include <QLatin1String>
#include <QStyle>
#include <QWidget>

namespace ui {

namespace Status {
// Severity increases downwards; the text of the label carries the detail.
inline constexpr const char *Neutral = "neutral";  // no signal yet, or disabled
inline constexpr const char *Pass = "pass";
inline constexpr const char *Caution = "caution";  // inconclusive, needs a look
inline constexpr const char *Fail = "fail";
}  // namespace Status

/// Applies a semantic status to a widget. Re-polishing is skipped when the
/// status has not changed, so calling this every frame costs almost nothing.
inline void setStatus(QWidget *widget, const char *status) {
	if (!widget) {
		return;
	}
	if (widget->property("status").toString() == QLatin1String(status)) {
		return;
	}
	widget->setProperty("status", status);

	// Dynamic properties do not re-evaluate the stylesheet on their own.
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}

}  // namespace ui
