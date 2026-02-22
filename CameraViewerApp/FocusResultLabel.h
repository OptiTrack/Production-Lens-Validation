#pragma once
#include <QWidget>
#include <QLabel>
#include "MetricsManager.h"
#include <QCoreApplication>

class FocusResultLabel : public QLabel {
public:
    FocusResultLabel(const QString &text, QWidget *parent = nullptr)
        : QLabel(text, parent)
    {
        setAutoFillBackground(true);
    }

    void updateTextandColor(double score, MetricsManager mMgr) {
        // change color and text of result depending on success rate
        if ((0 < score) && (score < .65)) {
			mMgr.setFocusOptimal(false);
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
