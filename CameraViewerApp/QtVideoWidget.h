#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QOpenGLWindow>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QColor>
#include <QByteArray>
#include <atomic>

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "cameralibrary.h"
#include <vector>

class VideoWidget : public QOpenGLWindow, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit VideoWidget(UpdateBehavior behavior = QOpenGLWindow::NoPartialUpdate);
    ~VideoWidget() override;

    struct RoiInfo {
        double circularity;
        cv::Rect rect;
        cv::Point2f centroid;
    };

public slots:
    void updateFrameFromBitmap(CameraLibrary::Bitmap* bmp);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    GLuint gl_texture = 0;
    int    texture_width = 0;
    int    texture_height = 0;
    QOpenGLVertexArrayObject vertex_array;
    QOpenGLBuffer            vertext_buffer{QOpenGLBuffer::VertexBuffer};
    std::unique_ptr<QOpenGLShaderProgram> program_shader;
    int position_attribute = -1;
    int uv_attribute  = -1;
    int sampler_uniform  = -1;
    // Edge detection uniforms using OpenGL shader
    int edge_mask_uniform = -1;
    int edge_color_uniform = -1;
    int edge_alpha_uniform = -1;
    GLuint edgeMaskTex = 0;
    float last_edge_focus_score = 0.0f; // 0..1 where 1 = sharp
    int   frame_width = 0;
    int   frame_height = 0;
    QColor background_color = Qt::black;

    QByteArray byte_array_staging;
    int pending_width = 0;
    int pending_height = 0;
    int pending_bpp = 0;
    int pending_stride = 0;
    std::atomic<bool> has_pending{false};
    std::atomic<bool> edge_detect_enabled{false};
    std::atomic<bool> roiZoomEnabled{ false };

    // Temporal smoothing for ROI centroids to prevent glitching
    std::vector<cv::Point2f> prev_roi_centroids;
    const float centroid_smoothing_alpha = 0.05f; 		// Lower = more smoothing (0.1-0.3)
    const float centroid_matching_threshold = 10000.0f; // Max distance squared for tracking

    // ROI detection parameters
    const int roi_extraction_margin = 30; 		// Margin around detected contours (pixels)
    const size_t roi_max_count = 5; 			// Maximum number of ROIs to detect

    // Edge detection parameters
    const double canny_low_threshold = 100.0; 	// Canny low threshold
    const double canny_high_threshold = 300.0; 	// Canny high threshold (ratio: 3:1)
    const int canny_kernel_size = 3; 			// Canny aperture size

    // Cached parameters for drawing shapes after edge detection
    struct ShapeDrawParams {
        int quadW = 0;
        int quadH = 0;
        int imgCenterX = 0;
        int imgCenterY = 0;
        int diamondX = 0;
        int diamondY = 0;
        int diamondW = 0;
        int diamondH = 0;
        int combinedW = 0;
        int combinedH = 0; // Original combined image dimensions
        std::vector<cv::Point> diamondPts;
        bool isValid = false;

        // Default constructor (optional since we initialized above)
        ShapeDrawParams() = default;

        // Optional constructor to set some values at creation
        ShapeDrawParams(int qw, int qh, int cx, int cy)
            : quadW(qw), quadH(qh), imgCenterX(cx), imgCenterY(cy), isValid(false) {
        }
    } shapeParams;

    // Swizzle cache to avoid re-setting every frame
    enum class SwizzleMode { DefaultRGBA, RedToRGB };
    SwizzleMode swizzle_mode = SwizzleMode::DefaultRGBA;
    GLint  current_internal_format = 0;
    GLenum current_format = 0;
    GLenum current_type = 0;
    int    current_bpp = 0;

private:
    void ensureProgram();
    void ensureVaoVbo();
    void updateQuad(float dstX, float dstY, float dstW, float dstH);
    void setSwizzleIfNeeded(SwizzleMode want);
    void applyEdgeDetection(cv::Mat& gray, int w, int h);
	cv::Mat applyRoiZoomToFrame(unsigned char* src, cv::Mat& gray, int w, int h, int stride);
	void drawShapesOverlay(float dstX, float dstY, float dstW, float dstH);
    std::vector<RoiInfo> extractROIs(const cv::Mat& gray, const cv::Mat& edges, int margin, size_t maxROIs);
	cv::Mat zoomCrop(const cv::Mat& src, const cv::Point& center, float zoom);

public slots:
    void setEdgeDetectEnabled(bool enabled) { edge_detect_enabled.store(enabled, std::memory_order_release); }
	void setRoiZoomEnabled(bool enabled) { roiZoomEnabled.store(enabled, std::memory_order_release); }
};

#endif // VIDEOWIDGET_H
