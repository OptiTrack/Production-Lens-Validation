#pragma once
#include "MetricsManager.h"
#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QWidget>
#include <algorithm>

class FocusScoreLabel : public QLabel {
public:
  FocusScoreLabel(const QString &text, QWidget *parent = nullptr)
      : QLabel(text, parent) {
    setAutoFillBackground(true);
  }

  void updateNumber(double score, MetricsManager mMgr) {
    this->setText(QString::number(score * 100, 'd'));
    this->setStyleSheet("color:#ddd; font-weight:600;");

    this->update();
  }
};
