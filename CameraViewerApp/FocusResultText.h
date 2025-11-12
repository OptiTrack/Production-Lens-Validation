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

    void setTextColor(const QColor &color) {
        QPalette palette = this->palette();
        //palette.setColor(this->backgroundRole(), color);
        palette.setColor(this->foregroundRole(), color);
        this->setPalette(palette);
    }
};
