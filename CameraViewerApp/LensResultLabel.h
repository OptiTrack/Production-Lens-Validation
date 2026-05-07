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
    if (metrics.visibleMarkers.empty() ||
        metrics.lensDisp == MetricsManager::untested) {
      this->setText(QCoreApplication::translate("DisplayResults", "Unknown"));
      this->setStyleSheet("color:#ddd; font-weight:600;");
      this->update();
      return;
    }

    const bool hasHook =
        std::any_of(metrics.visibleMarkers.begin(), metrics.visibleMarkers.end(),
                    [](const MetricsManager::contourData &m) {
                      return m.mClass == MetricsManager::hook;
                    });

    if (hasHook) {
      this->setText(
          QCoreApplication::translate("DisplayResults", "Failure (Hook)"));
      this->setStyleSheet("color:FireBrick; font-weight:600;");
    } else if (metrics.lensDisp == MetricsManager::pass) {
      this->setText(QCoreApplication::translate("DisplayResults", "Pass"));
      this->setStyleSheet("color:Cyan; font-weight:600;");
    } else if (metrics.lensDisp == MetricsManager::fail ||
               metrics.lensDisp == MetricsManager::check) {
      this->setText(QCoreApplication::translate("DisplayResults", "Failure"));
      this->setStyleSheet("color:FireBrick; font-weight:600;");
    } else {
      this->setText(QCoreApplication::translate("DisplayResults", "Unknown"));
      this->setStyleSheet("color:#ddd; font-weight:600;");
    }
    this->update();
  }
};
