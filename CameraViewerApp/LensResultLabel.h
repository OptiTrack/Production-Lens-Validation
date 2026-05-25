#pragma once
#include "MetricsManager.h"
#include <QCoreApplication>
#include <QLabel>
#include <QString>
#include <QWidget>

class LensResultLabel : public QLabel {
public:
  LensResultLabel(const QString &text, QWidget *parent = nullptr)
      : QLabel(parent), currentResultSource(text) {
    setAutoFillBackground(true);
    retranslateUi();
  }

  void retranslateUi() {
    this->setText(QCoreApplication::translate(
        "DisplayResults", currentResultSource.toStdString().c_str()));
  }

  void updateTextandColor(MetricsManager mMgr) {

    MetricsManager::lensMetrics metrics = mMgr.getMetrics();
    if (metrics.lensDisp == MetricsManager::fail) {
      currentResultSource = "Failure";
      this->setStyleSheet("color:FireBrick; font-weight:600;");
    } else if (metrics.lensDisp == MetricsManager::check) {
      currentResultSource = "Check";
      this->setStyleSheet("color:DarkOrange; font-weight:600;");
    } else {
      currentResultSource = "Pass";
      this->setStyleSheet("color:Cyan; font-weight:600;");
    }
    retranslateUi();
    this->update();
  }

private:
  QString currentResultSource;
};
