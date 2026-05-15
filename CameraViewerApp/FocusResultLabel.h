#pragma once
#include "MetricsManager.h"
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
      : QLabel(text, parent) {
    setAutoFillBackground(true);
  }

  void updateTextandColor(double score, MetricsManager mMgr) {

    // score will be -1 during condition when the focus tool was on and
    // updating at first, but was eventually turned off by the user
    // (hence not just the text change but also color change)
    if (score == -1) {
      this->setText(disabled);
      this->setStyleSheet("color:#ddd; font-weight:600;");
    }

    // change color and text of result depending on success rate
    else if ((0 < score) && (score < .65)) {
      mMgr.setFocusOptimal(false);
      this->setText(QCoreApplication::translate("DisplayResults",
                                                fail.toStdString().c_str()));
      this->setStyleSheet("color:FireBrick; font-weight:600;");
    }

    else if ((.65 <= score) && (score < .75)) {
      mMgr.setFocusOptimal(true);
      this->setText(QCoreApplication::translate("DisplayResults",
                                                wide_angle_success.toStdString().c_str()));
      this->setStyleSheet("color:#668b0b; font-weight:600;");
    }

    else if ((.75 <= score) && (score <= 10)) {
      mMgr.setFocusOptimal(true);
      this->setText(QCoreApplication::translate("DisplayResults",
                                                all_lens_success.toStdString().c_str()));
      this->setStyleSheet("color:ForestGreen; font-weight:600;");
    }

    else {
      mMgr.setFocusOptimal(false);
      this->setText(
          QCoreApplication::translate("DisplayResults",
                                      inconclusive.toStdString().c_str()));
      this->setStyleSheet("color:Gold; font-weight:600;");
    }

    this->update();
  }
  QString fail = "Failure";
  QString wide_angle_success = "Success (Wide Angle Lens)";
  QString all_lens_success = "Success (All lenses)";
  QString inconclusive = "Inconclusive";
  QString disabled = "Disabled";
};
