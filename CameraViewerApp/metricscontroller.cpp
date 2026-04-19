#include "metricscontroller.h"

MetricController::MetricController(MetricWidgets *metricWidgets)
    : m_metricWidgets(metricWidgets) {}

void MetricController::addData(qreal id, QHash<QString, qreal> metrics) {
  QVector<QLabel *> &dataLabels = m_metricWidgets->dataLabels;
  QVector<GraphWidget *> &metricGraphs = m_metricWidgets->metricGraphs;

  for (int i = 0; i < dataLabels.count(); ++i) {
    QLabel *dataLabel = dataLabels.at(i);
    QString key = dataLabel->objectName().remove("DataLabel");

    if (metrics.contains(key)) {
      qreal value = metrics.value(key);
      dataLabel->setText(QString::number(value, 'f', 3) + " " +
                         m_metricWidgets->units);

      const bool passing = value >= m_metricWidgets->passingThreshold;
      dataLabel->setStyleSheet(passing ? "color: cyan;" : "");

      GraphWidget *metricGraph = metricGraphs.at(i);
      if (metricGraph) {
        metricGraph->addData(id, value);
      }
    }
  }
}

MetricWidgets *MetricController::getMetricWidgets() { return m_metricWidgets; }

QList<QVector<qreal>> MetricController::getGraphData(int i) {
  return m_metricWidgets->metricGraphs[i]->getData();
}
