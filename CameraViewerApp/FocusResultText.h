#pragma once
#include <QWidget>
#include <QPalette>
#include <QLabel>
#include <QColor>
#include <QFont>
#include <algorithm>

class DisplayResults : public QLabel {
public:
    DisplayResults(const QString &text, QWidget *parent = nullptr)
        : QLabel(text, parent)
    {
        setAutoFillBackground(true);
    }

    void updateTextandColor(double &score) {
        // change color and text of result depending on success rate
        if ((0 < score) && (score < .65)) {
            this->setText("Failure");
            this->setStyleSheet("color:FireBrick; font-weight:600;");
        }
        else if ((.65 <= score) && (score < .75)) {
            this->setText("Success (Wide Angle Lens)");
            this->setStyleSheet("color:#668b0b; font-weight:600;");
        }
        else if ((.75 <= score) && (score <= 10)) {
            this->setText("Success (All lenses)");
            this->setStyleSheet("color:ForestGreen; font-weight:600;");
        }
        else {
            this->setText("Inconclusive");
            this->setStyleSheet("color:Gold; font-weight:600;");
        }
        this->update();
    }
};
