#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QVector>
#include <QList>

class GraphWidget: public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    GraphWidget(QWidget *parent = nullptr);
    void addData(qreal x, qreal y);
    QList<QVector<qreal>>getData();
    void setYAxisRange(qreal min, qreal max); // Set fixed Y-axis range
    void setAutoScale(bool enable); // Enable/disable auto-scaling
    void updateScrollPosition(qreal currentTime); // Update scroll without adding data

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QVector<qreal> xData;
    QVector<qreal> yData;

    qreal xWindowSize = 10.0;
    qreal xScrollOffset = 0.0;
    
    // Y-axis configuration
    bool autoScaleY = true;
    qreal fixedYMin = 0.0;
    qreal fixedYMax = 1.0;
    
    const int segments = 20;
    const float lineWidth = 3.0f;
    const float markerRadius = 1.0f;
    const float ringRadius = markerRadius + 0.1f;

    void drawMarker(qreal x, qreal y, float ringXRad, float ringYRad, float diskXRad, float diskYRad);
};

#endif // GRAPHWIDGET_H
