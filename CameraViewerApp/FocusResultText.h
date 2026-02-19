#pragma once
#include <QWidget>
#include <QPalette>
#include <QLabel>
#include <QColor>
#include <QFont>
#include <algorithm>
#include "MetricsExporter.h"
#include <QCoreApplication>

class DisplayResults : public QLabel {
public:
    DisplayResults(const QString &text, QWidget *parent = nullptr)
        : QLabel(text, parent)
    {
        setAutoFillBackground(true);
    }

    void updateTextandColor(double score, MetricsExporter mExport) {
        // change color and text of result depending on success rate
        if ((0 < score) && (score < .65)) {
			mExport.setFocusOptimal(false);
			this->setText(QCoreApplication::translate("DisplayResults", "Failure"));
            this->setStyleSheet("color:FireBrick; font-weight:600;");
        }

        else if ((.65 <= score) && (score < .75)) {
			mExport.setFocusOptimal(true);
			this->setText(QCoreApplication::translate("DisplayResults", "Success (Wide Angle Lens)"));
            this->setStyleSheet("color:#668b0b; font-weight:600;");
        }

        else if ((.75 <= score) && (score <= 10)) {
            mExport.setFocusOptimal(true);
			this->setText(QCoreApplication::translate("DisplayResults", "Success (All lenses)"));
            this->setStyleSheet("color:ForestGreen; font-weight:600;");
        }

        else {
            mExport.setFocusOptimal(false);
			this->setText(QCoreApplication::translate("DisplayResults", "Inconclusive"));
            this->setStyleSheet("color:Gold; font-weight:600;");
        }

        this->update();
    }
};
