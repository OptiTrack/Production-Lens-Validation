#pragma once
#include "MetricsManager.h"
#include "StatusStyle.h"
#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QString>
#include <QWidget>
#include <algorithm>

class FocusScoreLabel : public QLabel {
public:
  FocusScoreLabel(const QString &text, QWidget *parent = nullptr)
      : QLabel(text, parent) {
    setAutoFillBackground(true);
    // Weight lives on the font: motive.css only supplies the status colour.
    QFont resultFont = font();
    resultFont.setBold(true);
    setFont(resultFont);
  }

  void updateNumber(double score, const MetricsManager & /*mMgr*/) {
    this->setText(QString::number(score * 100, 'f', 2));
    ui::setStatus(this, ui::Status::Neutral);

    this->update();
  }
};
