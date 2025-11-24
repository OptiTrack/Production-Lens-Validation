#include "graphwidget.h"

GraphWidget::GraphWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    QOpenGLWidget::setFormat(format);

    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
}

void GraphWidget::addData(qreal x, qreal y) {

    if (y < -1e6 || y > 1e6 || std::isnan(y)) {
        qWarning() << "Invalid y value ignored:" << y;
        return;
    }

    if (!xData.isEmpty() && x < xData.last()) {
        xData.clear();
        yData.clear();
        xScrollOffset = 0.0;
    }

    xData.append(x);
    yData.append(y);

    if (x > xScrollOffset + xWindowSize) {
        xScrollOffset = x - xWindowSize;
    }
    update();
}

QList<QVector<qreal>> GraphWidget::getData() {

    QList<QVector<qreal>> data;
    data.append(xData);
    data.append(yData);
    return data;
}

void GraphWidget::drawMarker(qreal x, qreal y, 
                             float ringXRad, float ringYRad,
                             float diskXRad, float diskYRad)
{
    
    // Outer white ring
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_TRIANGLE_FAN);
      glVertex2f(x, y);
      for (int i = 0; i <= segments; ++i) {
        float angle = i * 2.0f * M_PI / float(segments);
        float xx = x + ringXRad * std::cos(angle);
        float yy = y + ringYRad * std::sin(angle);
        glVertex2f(xx, yy);
      }
    glEnd();

    // Inner blue disk
    glColor3f(0.0f, 0.9216f, 1.0f);
    glBegin(GL_TRIANGLE_FAN);
      glVertex2f(x, y);
      for (int i = 0; i <= segments; ++i) {
        float angle = i * 2.0f * M_PI / float(segments);
        float xx = x + diskXRad * std::cos(angle);
        float yy = y + diskYRad * std::sin(angle);
        glVertex2f(xx, yy);
      }
    glEnd();
}

void GraphWidget::initializeGL() {
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.1765f, 0.1765f, 0.1765f, 1.0f);
}

void GraphWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void GraphWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (xData.isEmpty()) return;

    // Fixed Y-axis range for focus metrics (0-1 with padding)
    qreal yMin = 0.0;
    qreal yMax = 1.0;
    qreal yPadding = (yMax - yMin) * 0.15;  // 15% padding on top and bottom
    yMin -= yPadding;
    yMax += yPadding;

    qreal xPadding = xWindowSize * 0.1;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(xScrollOffset - xPadding, xScrollOffset + xWindowSize + xPadding, yMin, yMax, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(0.0f, 0.9216f, 1.0f);
    glLineWidth(lineWidth);
    glBegin(GL_LINE_STRIP);
    for (size_t i = 0; i < xData.size(); ++i) {
        if (xData[i] >= xScrollOffset && xData[i] <= xScrollOffset + xWindowSize) {
            glVertex2f(xData[i], yData[i]);
        }
    }
    glEnd();

    // Compute world‐space marker size from pixel size:
    int w = width();
    int h = height();
    float worldXmin = (xScrollOffset - xPadding);
    float worldXmax = (xScrollOffset + xWindowSize + xPadding);
    float worldWidth  = worldXmax - worldXmin;    // = xWindowSize + 2*xPadding

    float worldYmin = yMin;
    float worldYmax = yMax;
    float worldHeight = worldYmax - worldYmin;    // = (yMax - yMin)

    // How many world‐units per pixel on X and Y
    float pxToWorldX = worldWidth  / float(w);
    float pxToWorldY = worldHeight / float(h);

    // two pixel‐radii: one for the white ring, one for the inner disk
    int diskPixels   = 4;
    int ringPixels   = diskPixels + 1;

    // convert to world‐space radii
    float ringRadiusX = ringPixels * pxToWorldX;
    float ringRadiusY = ringPixels * pxToWorldY;

    float diskRadiusX = diskPixels * pxToWorldX;
    float diskRadiusY = diskPixels * pxToWorldY;

    // Draw “last‐point” marker at (lastX,lastY):
    qreal lastX = xData.last();
    qreal lastY = yData.last();
    if (lastX >= xScrollOffset &&
        lastX <= xScrollOffset + xWindowSize) {
        drawMarker(lastX, lastY,
                   ringRadiusX,  ringRadiusY,
                   diskRadiusX,  diskRadiusY); 
    }
}
