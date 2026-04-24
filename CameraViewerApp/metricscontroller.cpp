#include "metricscontroller.h"

MetricController::MetricController(MetricWidgets *metricWidgets)
{
  addMetricWidgets(metricWidgets);
}

void MetricController::addMetricWidgets(MetricWidgets *metricWidgets) {
  if (!metricWidgets || m_metricWidgets.contains(metricWidgets)) {
    return;
  }
  m_metricWidgets.append(metricWidgets);
}

void MetricController::addData(qreal id, QHash<QString, qreal> metrics) {
  for (MetricWidgets *metricWidgets : m_metricWidgets) {
    if (!metricWidgets) {
      continue;
    }

    QVector<QLabel *> &dataLabels = metricWidgets->dataLabels;
    QVector<GraphWidget *> &metricGraphs = metricWidgets->metricGraphs;

    for (int i = 0; i < dataLabels.count(); ++i) {
      QLabel *dataLabel = dataLabels.at(i);
      QString key = dataLabel->objectName().remove("DataLabel");

      if (metrics.contains(key)) {
        qreal value = metrics.value(key);
        const QString unitsSuffix =
            metricWidgets->units.isEmpty() ? QString()
                                           : QStringLiteral(" ") +
                                                 metricWidgets->units;
        dataLabel->setText(QString::number(value * 100, 'd') + unitsSuffix);

        const bool passing = value >= metricWidgets->passingThreshold;
        const int metricFontSizePx =
            dataLabel->property("metricFontSizePx").toInt();
        QString labelStyle = passing ? QStringLiteral("color: cyan;")
                                     : QStringLiteral("color: #ddd;");
        labelStyle += QStringLiteral(" font-weight: 700; padding-left: 0px; "
                                     "padding-right: 0px;");
        if (metricFontSizePx > 0) {
          labelStyle +=
              QStringLiteral(" font-size: %1px;").arg(metricFontSizePx);
        }
        dataLabel->setStyleSheet(labelStyle);

        GraphWidget *metricGraph = metricGraphs.at(i);
        if (metricGraph) {
          metricGraph->addData(id, value);
        }
      }
    }
  }
}

MetricWidgets *MetricController::getMetricWidgets() {
  return m_metricWidgets.isEmpty() ? nullptr : m_metricWidgets.first();
}

QList<QVector<qreal>> MetricController::getGraphData(int i) {
  MetricWidgets *metricWidgets = getMetricWidgets();
  if (!metricWidgets || i < 0 || i >= metricWidgets->metricGraphs.size() ||
      !metricWidgets->metricGraphs[i]) {
    return {};
  }
  return metricWidgets->metricGraphs[i]->getData();
}
