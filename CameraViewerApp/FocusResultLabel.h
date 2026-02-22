#pragma once
#include <QWidget>
#include <QPalette>
#include <QLabel>
#include <QColor>
#include <QFont>
#include <algorithm>
#include "MetricsManager.h"
#include <QCoreApplication>

class FocusResultLabel : public QLabel {
public:
    FocusResultLabel(const QString &text, QWidget *parent = nullptr)
        : QLabel(text, parent)
    {
        setAutoFillBackground(true);
    }

    void updateTextandColor(double score, MetricsExporter mExport) {

        // score will be -1 during condition when the focus tool was on and
        // updating at first, but was eventually turned off by the user
        // (hence not just the text change but also color change)
        if (score == -1) {
            this->setText("Disabled");
            this->setStyleSheet("color:#ddd; font-weight:600;");
        }

        // change color and text of result depending on success rate
        else if ((0 < score) && (score < .65)) {
			mExport.setFocusOptimal(false);
			this->setText(QCoreApplication::translate("DisplayResults", "Failure"));
            this->setStyleSheet("color:FireBrick; font-weight:600;");
        }

        else if ((.65 <= score) && (score < .75)) {
            mMgr.setFocusOptimal(true);
			this->setText(QCoreApplication::translate("DisplayResults", "Success (Wide Angle Lens)"));
            this->setStyleSheet("color:#668b0b; font-weight:600;");
        }

        else if ((.75 <= score) && (score <= 10)) {
            mMgr.setFocusOptimal(true);
			this->setText(QCoreApplication::translate("DisplayResults", "Success (All lenses)"));
            this->setStyleSheet("color:ForestGreen; font-weight:600;");
        }

        else {
            mMgr.setFocusOptimal(false);
			this->setText(QCoreApplication::translate("DisplayResults", "Inconclusive"));
            this->setStyleSheet("color:Gold; font-weight:600;");
        }

        this->update();
    }
};
