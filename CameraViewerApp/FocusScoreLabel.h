#pragma once
#include <QWidget>
#include <QPalette>
#include <QLabel>
#include <QColor>
#include <QFont>
#include <algorithm>
#include "MetricsManager.h"
#include <QCoreApplication>

class FocusScoreLabel : public QLabel {
public:
    FocusScoreLabel(const QString &text, QWidget *parent = nullptr)
        : QLabel(text, parent)
    {
        setAutoFillBackground(true);
    }

    void updateNumber(double score, MetricsManager mMgr) {
        this->setText(QString::number(score, 'g', 2));
        this->setStyleSheet("color:#ddd; font-weight:600;");

        this->update();
    }
};
