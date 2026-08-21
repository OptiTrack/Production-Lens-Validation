#pragma once
#include "MetricsManager.h"
#include "StatusStyle.h"
#include <QCoreApplication>
#include <QLabel>
#include <QString>
#include <QWidget>

class LensResultLabel : public QLabel {
public:
  LensResultLabel(const QString &text, QWidget *parent = nullptr)
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

  void updateTextandColor(const MetricsManager &mMgr) {

    MetricsManager::lensMetrics metrics = mMgr.getMetrics();
    if (metrics.lensDisp == MetricsManager::fail) {
      currentResultSource = "Failure";
      ui::setStatus(this, ui::Status::Fail);
    } else if (metrics.lensDisp == MetricsManager::check) {
      currentResultSource = "Check";
      ui::setStatus(this, ui::Status::Caution);
    } else {
      currentResultSource = "Pass";
      ui::setStatus(this, ui::Status::Pass);
    }
    retranslateUi();
    this->update();
  }

private:
  QString currentResultSource;
};
