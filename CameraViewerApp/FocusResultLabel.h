#pragma once
#include "MetricsManager.h"
#include "StatusStyle.h"
#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QString>
#include <QLabel>
#include <QPalette>
#include <QWidget>
#include <algorithm>

class FocusResultLabel : public QLabel {
public:
  FocusResultLabel(const QString &text, QWidget *parent = nullptr)
      : QLabel(parent), currentResultSource(text) {
    setAutoFillBackground(true);
    // Weight lives on the font: motive.css only supplies the status colour.
    QFont resultFont = font();
    resultFont.setBold(true);
    setFont(resultFont);
    retranslateUi();
  }

  void retranslateUi() {
    this->setText(QCoreApplication::translate(
        "DisplayResults", currentResultSource.toStdString().c_str()));
  }

  // The MetricsManager argument is kept for call-site compatibility. The
  // focus-optimal flag belongs to the caller's own object (see main.cpp);
  // setting it here only ever wrote to a by-value copy.
  void updateTextandColor(double score, const MetricsManager & /*mMgr*/) {

    // score will be -1 during condition when the focus tool was on and
    // updating at first, but was eventually turned off by the user
    // (hence not just the text change but also color change)
    if (score == -1) {
      currentResultSource = disabled;
      ui::setStatus(this, ui::Status::Neutral);
    }

    // change color and text of result depending on success rate
    else if ((0 < score) && (score < .65)) {
      currentResultSource = fail;
      ui::setStatus(this, ui::Status::Fail);
    }

    else if ((.65 <= score) && (score < .75)) {
      // Both tiers are a pass; the text says which lenses it covers.
      currentResultSource = wide_angle_success;
      ui::setStatus(this, ui::Status::Pass);
    }

    else if ((.75 <= score) && (score <= 10)) {
      currentResultSource = all_lens_success;
      ui::setStatus(this, ui::Status::Pass);
    }

    else {
      currentResultSource = inconclusive;
      ui::setStatus(this, ui::Status::Caution);
    }

    retranslateUi();
    this->update();
  }
  QString fail = "Failure";
  QString wide_angle_success = "Success (Wide Angle Lens)";
  QString all_lens_success = "Success (All lenses)";
  QString inconclusive = "Inconclusive";
  QString disabled = "Disabled";

private:
  QString currentResultSource;
};
