#ifndef METRICSCONTROLLER_H
#define METRICSCONTROLLER_H

#pragma once

#include "./widgets/graphwidget.h"
#include "QtCameraControlPanel.h"

#include <QObject>
#include <QString>
#include <QGroupBox>
#include <QLabel>
#include <QHash>

class MetricController : public QObject {
    Q_OBJECT

public:
    MetricController(MetricWidgets *metricWidgets);
    void addData(qreal id, QHash<QString, qreal> metrics);
    MetricWidgets *getMetricWidgets();
    QList<QVector<qreal>>getGraphData(int i);

private:
    MetricWidgets *m_metricWidgets;
};
#endif // METRICSCONTROLLER_H
