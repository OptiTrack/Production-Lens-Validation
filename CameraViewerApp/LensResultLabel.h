#pragma once
#include "MetricsManager.h"
#include <QWidget>

class LensResultLabel : public QLabel {
public:
  LensResultLabel(const QString &text, QWidget *parent = nullptr)
      : QLabel(text, parent) {
    setAutoFillBackground(true);
  }

  void updateTextandColor(MetricsManager mMgr) {

    MetricsManager::lensMetrics metrics = mMgr.getMetrics();
    if (metrics.lensDisp == MetricsManager::fail) {
      this->setText(QCoreApplication::translate("DisplayResults", "Failure"));
      this->setStyleSheet("color:FireBrick; font-weight:600;");
    } else if (metrics.lensDisp == MetricsManager::check) {
      this->setText(QCoreApplication::translate("DisplayResults", "Check"));
      this->setStyleSheet("color:DarkOrange; font-weight:600;");
    } else {
      this->setText(QCoreApplication::translate("DisplayResults", "Pass"));
      this->setStyleSheet("color:ForestGreen; font-weight:600;");
    }
    this->update();
  }
};
