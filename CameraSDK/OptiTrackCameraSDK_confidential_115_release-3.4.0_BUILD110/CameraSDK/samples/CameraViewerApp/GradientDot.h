#pragma once
#include <QWidget>
#include <QPainter>
#include <algorithm>

class GradientDot : public QWidget {
public:
    explicit GradientDot(QWidget *parent = nullptr)
        : QWidget(parent), value(0.0) {}

    void setValue(double v) {
        value = std::clamp(v, 0.0, 1.0);
        update();  // repaint with new color
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        // 0.0 → red, 1.0 → green
        QColor color = QColor::fromHsvF(value * 0.33, 1.0, 1.0);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(rect().adjusted(2, 2, -2, -2));
    }

private:
    double value;
};
