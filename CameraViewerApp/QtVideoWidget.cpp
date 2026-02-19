#include "QtVideoWidget.h"
#include <algorithm>
#include <cstring>
// OpenCV for simple image processing (edge detection)
#include <opencv2/imgproc.hpp>
#include <opencv2/core/cvdef.h>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>
#include <bitmap.h>
#include <gl/GL.h>
#include <cstddef>
#include <memory>
#include <qopenglext.h>
#include <qsurfaceformat.h>
#include <qvectornd.h>
#include <qopenglbuffer.h>
#include <qopenglshaderprogram.h>
#include <qopenglwindow.h>
#include <vector>
#include <opencv2/core/types.hpp>
#include <limits>

// Specialized GL Viewer for displaying bitmaps from a camera

using namespace CameraLibrary;
using RoiInfo = VideoWidget::RoiInfo;

namespace {
struct Vertex { float x, y, u, v; };

// Convert widget pixel coords to NDC (-1..1), with origin top-left in pixels
inline float toNdcX(float x, float W) { return (x / W) * 2.0F - 1.0F; }
inline float toNdcY(float y, float H) { return 1.0F - (y / H) * 2.0F; }
}

VideoWidget::VideoWidget(UpdateBehavior behavior)
    : QOpenGLWindow(behavior) {
    QSurfaceFormat f = format(); f.setSwapInterval(0); setFormat(f);
}

VideoWidget::~VideoWidget() {
    makeCurrent();
    if (gl_texture) glDeleteTextures(1, &gl_texture);
    if (edgeMaskTex) glDeleteTextures(1, &edgeMaskTex); // Delete edge mask texture
    vertext_buffer.destroy();
    vertex_array.destroy();
    program_shader.reset();
    doneCurrent();
}

void VideoWidget::initializeGL() {
    initializeOpenGLFunctions();

    glGenTextures(1, &gl_texture);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,   GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,   GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create edge mask texture (single-channel) for edge-detetion.
    // This will be replaced every frame if edge detection is enabled with real edge map data via OpenCV.
    glGenTextures(1, &edgeMaskTex);
    glBindTexture(GL_TEXTURE_2D, edgeMaskTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,   GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,   GL_CLAMP_TO_EDGE);
    unsigned char zero = 0;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &zero);
    glBindTexture(GL_TEXTURE_2D, 0);

    ensureProgram();
    ensureVaoVbo();

    // Bind once & describe attributes once (we'll only rewrite vertex data later)
    vertex_array.bind();
    vertext_buffer.bind();
    glEnableVertexAttribArray(position_attribute);
    glEnableVertexAttribArray(uv_attribute);
    glVertexAttribPointer(position_attribute, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, x)));
    glVertexAttribPointer(uv_attribute,  2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, u)));
    vertext_buffer.release();
    vertex_array.release();
}

void VideoWidget::resizeGL(int w, int h) {
    const float dpr = devicePixelRatio();
    glViewport(0, 0, int(w * dpr), int(h * dpr));
}

void VideoWidget::paintGL() {
    glClearColor(background_color.redF(), background_color.greenF(), background_color.blueF(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Only upload on new frame
    if (has_pending.exchange(false)) {
        const int   w   = pending_width;
        const int   h   = pending_height;
        const int   bpp = pending_bpp;
        const int   srcStride = pending_stride;
        const auto* src = reinterpret_cast<const unsigned char*>(byte_array_staging.constData());

        if (w > 0 && h > 0 && src) {
            // Choose upload params from bpp
            GLint  internalFormat = GL_RGBA8;
            GLenum format         = GL_RGBA;
            GLenum type           = GL_UNSIGNED_BYTE;
            int    bytesPerPixel  = 4;
            SwizzleMode wantSwizzle = SwizzleMode::DefaultRGBA;

            switch (bpp) {
                case 8:
                    internalFormat = GL_R8;
                    format         = GL_RED;
                    type           = GL_UNSIGNED_BYTE;
                    bytesPerPixel  = 1;
                    wantSwizzle    = SwizzleMode::RedToRGB;
                    break;
                case 16:
                    internalFormat = GL_R16;
                    format         = GL_RED;
                    type           = GL_UNSIGNED_SHORT;
                    bytesPerPixel  = 2;
                    wantSwizzle    = SwizzleMode::RedToRGB;
                    break;
                case 24:
                    internalFormat = GL_RGBA8;
                    format         = GL_RGB;
                    type           = GL_UNSIGNED_BYTE;
                    bytesPerPixel  = 3;
                    wantSwizzle    = SwizzleMode::DefaultRGBA;
                    break;
                case 32:
                    internalFormat = GL_RGBA8;
                    format         = GL_RGBA;
                    type           = GL_UNSIGNED_BYTE;
                    bytesPerPixel  = 4;
                    wantSwizzle    = SwizzleMode::DefaultRGBA;
                    break;
                default:
                    bytesPerPixel = 0; // Unknown Format: bail
                    break;
            }

            if (bytesPerPixel > 0) {
                glBindTexture(GL_TEXTURE_2D, gl_texture);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

                #ifdef GL_UNPACK_ROW_LENGTH
                const GLint rowLenPixels = (bytesPerPixel > 0) ? (srcStride / bytesPerPixel) : 0;
                glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLenPixels);
                #endif

                const bool sizeChanged   = (w != texture_width) || (h != texture_height);
                const bool formatChanged = (internalFormat != current_internal_format) ||
                                           (format         != current_format) ||
                                           (type           != current_type);

                if (sizeChanged || formatChanged) {
                    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, src);
                    texture_width = w; texture_height = h;
                    current_internal_format = internalFormat;
                    current_format         = format;
                    current_type           = type;
                    current_bpp            = bpp;
                } else {
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, format, type, src);
                }

                // Update swizzle only when necessary
                setSwizzleIfNeeded(wantSwizzle);

                #ifdef GL_UNPACK_ROW_LENGTH
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                #endif

                glBindTexture(GL_TEXTURE_2D, 0);
                frame_width = w; frame_height = h;
            }
        }
    }

    if (!gl_texture || frame_width <= 0 || frame_height <= 0) return;

    // Fit image into window while preserving aspect ratio
    const float W = float(width());
    const float H = float(height());
    const float texAspect = float(frame_width) / float(frame_height);
    const float winAspect = W / H;
    float dstW, dstH;
    if (winAspect >= texAspect) {
        dstH = H;
        dstW = dstH * texAspect;
    } else {
        dstW = W;
        dstH = dstW / texAspect;
    }
    const float dstX = (W - dstW) * 0.5f;
    const float dstY = (H - dstH) * 0.5f;

    updateQuad(dstX, dstY, dstW, dstH);

    program_shader->bind();
    vertex_array.bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    program_shader->setUniformValue(sampler_uniform, 0);
    // Bind edge mask on texture unit 1 and provide overlay color/alpha
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, edgeMaskTex);
    program_shader->setUniformValue(edge_mask_uniform, 1);
    // Neon blue (possibly implement QT element to change color?)
    program_shader->setUniformValue(edge_color_uniform, QVector3D(0.00f, 0.65f, 1.0f));
    // Only show overlay when edge detection is enabled: alpha = 1.0 (showing) else 0.0 (transparent)
    const float alpha = edge_detect_enabled.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    program_shader->setUniformValue(edge_alpha_uniform, alpha);
    // Draw quad and cleanup
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    // Restore GL state: unbind textures on both units
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    vertex_array.release();
    program_shader->release();

    // Explicitly ensure shader is unbound
    glUseProgram(0);

    // Draw shapes on top of textures if in ROI mode
    if (roiZoomEnabled.load(std::memory_order_relaxed) && shapeParams.isValid) {
        drawShapesOverlay(dstX, dstY, dstW, dstH);
    }
}


void VideoWidget::updateFrameFromBitmap(CameraLibrary::Bitmap* bmp) {
    if (!bmp) return;

    const int w = bmp->PixelWidth();
    const int h = bmp->PixelHeight();
    const int bpp = bmp->GetBitsPerPixel();
    const int srcStride = bmp->ByteSpan();
    const unsigned char* src = bmp->GetBits();
    if (w <= 0 || h <= 0 || !src) return;

    const int bytesPerPixel = (bpp == 8) ? 1 : (bpp == 16) ? 2 : (bpp == 24) ? 3 : 4;
    const size_t required = size_t(srcStride) * size_t(h);

    byte_array_staging.resize(int(required));
    const int dstStride = srcStride;

    // Relaxed is sufficient because these flags are not used for synchronization
	const bool edgeDetectEnabled = edge_detect_enabled.load(std::memory_order_relaxed);
	const bool roiEnabled = roiZoomEnabled.load(std::memory_order_relaxed);

	// grouping checks to avoid redundant cv::Mat conversions
    if (edgeDetectEnabled || roiEnabled) { 

        // Convert source to appropriate cv::Mat based on bpp
        cv::Mat sourceMat;
        // Wrap source buffer in a cv::Mat. Treating it as read-only; OpenCV never writes to this buffer.
        if (bpp == 8) {
            sourceMat = cv::Mat(h, w, CV_8UC1, const_cast<unsigned char*>(src), srcStride);
        }
        else if (bpp == 16) {
            sourceMat = cv::Mat(h, w, CV_16UC1, const_cast<unsigned char*>(src), srcStride);
        }
        else if (bpp == 24) {
            sourceMat = cv::Mat(h, w, CV_8UC3, const_cast<unsigned char*>(src), srcStride);
        }
        else if (bpp == 32) {
            sourceMat = cv::Mat(h, w, CV_8UC4, const_cast<unsigned char*>(src), srcStride);
        }

        // Convert to 8-bit grayscale for edge detection
        cv::Mat gray;
        if (bpp == 16) {
            sourceMat.convertTo(gray, CV_8U, 1.0 / 256.0);
        }
        else if (bpp == 24) {
            cv::cvtColor(sourceMat, gray, cv::COLOR_BGR2GRAY);
        }
        else if (bpp == 32) {
            cv::cvtColor(sourceMat, gray, cv::COLOR_BGRA2GRAY);
        }
        else {
            gray = sourceMat;
        }

        if (roiEnabled) {
            // find and organize ROIs into mat (no shapes)
            cv::Mat combined = applyRoiZoomToFrame(bmp->GetBits(), gray, w, h, srcStride);
            if (!combined.empty()) {
                // Run edge detection on clean quadrant-ized (no shapes) image
                if (edgeDetectEnabled) {
                    cv::Mat combinedCopy = combined.clone();
                    cv::Mat resizedCombined;
                    cv::resize(combinedCopy, resizedCombined, cv::Size(w, h));
                    applyEdgeDetection(resizedCombined, w, h); // true = use ROI mask
                }

                // Resize and copy combined image (WITHOUT shapes) to staging buffer
                cv::Mat resizedFinal;
                cv::resize(combined, resizedFinal, cv::Size(w, h));
                for (int y = 0; y < h; ++y) {
                    unsigned char* d = reinterpret_cast<unsigned char*>(byte_array_staging.data()) + y * dstStride;
                    std::memcpy(d, resizedFinal.ptr(y), w);
                }
            }
        }
        else {
            // No ROI zoom - copy original source and optionally detect edges
            for (int row = 0; row < h; ++row) {
                const unsigned char* s = src + size_t(row) * size_t(srcStride);
                unsigned char* d = reinterpret_cast<unsigned char*>(byte_array_staging.data()) + size_t(row) * size_t(dstStride);
                std::memcpy(d, s, size_t(dstStride));
            }

            if (edgeDetectEnabled && !gray.empty()) {
                applyEdgeDetection(gray, w, h);
            }
        }
    }

    else {
        for (int row = 0; row < h; ++row) {
            const unsigned char* s = src + size_t(row) * size_t(srcStride);
            unsigned char* d = reinterpret_cast<unsigned char*>(byte_array_staging.data()) + size_t(row) * size_t(dstStride);
            std::memcpy(d, s, size_t(dstStride));
        }
    }

    pending_width = w;
    pending_height = h;
    pending_bpp = bpp;
    pending_stride = srcStride;
    has_pending.store(true, std::memory_order_release);

    // Schedule a repaint on the GL thread
    requestUpdate();
}

void VideoWidget::ensureProgram() {
    if (program_shader) return;

    program_shader = std::make_unique<QOpenGLShaderProgram>();

    // Portable shader (GLSL 1.20 / ES 2.0)
    const char* vs =
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "#endif\n"
        "attribute vec2 aPos;\n"
        "attribute vec2 aUV;\n"
        "varying vec2 vUV;\n"
        "void main(){ vUV=aUV; gl_Position=vec4(aPos,0.0,1.0); }\n";

    const char* fs =
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "#endif\n"
        "uniform sampler2D uTex;\n"
        "uniform sampler2D uEdgeMask;\n"
        "uniform vec3 uEdgeColor;\n"
        "uniform float uEdgeAlpha;\n"
        "varying vec2 vUV;\n"
        "void main(){\n"
        "  vec4 base = texture2D(uTex, vUV);\n"
        "  float mask = texture2D(uEdgeMask, vUV).r;\n"
        "  vec3 blended = mix(base.rgb, uEdgeColor, mask * uEdgeAlpha);\n"
        "  gl_FragColor = vec4(blended, base.a);\n"
        "}\n";

    program_shader->addShaderFromSourceCode(QOpenGLShader::Vertex,   vs);
    program_shader->addShaderFromSourceCode(QOpenGLShader::Fragment, fs);
    program_shader->link();

    position_attribute = program_shader->attributeLocation("aPos");
    uv_attribute  = program_shader->attributeLocation("aUV");
    sampler_uniform  = program_shader->uniformLocation("uTex");
    edge_mask_uniform = program_shader->uniformLocation("uEdgeMask");
    edge_color_uniform = program_shader->uniformLocation("uEdgeColor");
    edge_alpha_uniform = program_shader->uniformLocation("uEdgeAlpha");
}

void VideoWidget::ensureVaoVbo() {
    if (!vertex_array.isCreated()) vertex_array.create();
    if (!vertext_buffer.isCreated()) {
        vertext_buffer.create();
        vertext_buffer.bind();
        vertext_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
        // allocate space for 4 vertices
        vertext_buffer.allocate(sizeof(Vertex) * 4);
        vertext_buffer.release();
    }
}

void VideoWidget::updateQuad(float dstX, float dstY, float dstW, float dstH) {
    const float W = float(width());
    const float H = float(height());

    const float xL = toNdcX(dstX,        W);
    const float xR = toNdcX(dstX+dstW,   W);
    const float yT = toNdcY(dstY,        H);
    const float yB = toNdcY(dstY+dstH,   H);
    const float v0 = 1.0f;
    const float v1 = 0.0f;

    Vertex quad[4] = {
        { xL, yB, 0.0F, v0 },
        { xR, yB, 1.0F, v0 },
        { xL, yT, 0.0F, v1 },
        { xR, yT, 1.0F, v1 },
    };

    vertex_array.bind();
    vertext_buffer.bind();
    vertext_buffer.write(0, quad, sizeof(quad));
    vertext_buffer.release();
    vertex_array.release();
}

void VideoWidget::setSwizzleIfNeeded(SwizzleMode want) {
    if (want == swizzle_mode) return;

    glBindTexture(GL_TEXTURE_2D, gl_texture);
    if (want == SwizzleMode::RedToRGB) {
        const GLint swizzle[4] = { GL_RED, GL_RED, GL_RED, GL_ONE };
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    } else {
        const GLint swizzle[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    swizzle_mode = want;
}

void VideoWidget::setNewZoomValue(float value) {
    if (value > 0) {
        this->ROIZoomScale = value;
    }
}

/// <summary>
/// Focuses the view onto the outermost four markers and center marker by zooming into each region
/// </summary>
/// <param name="src">Source bitmap data</param>
/// <param name="gray">Converted gray mat from bitmap source</param>
/// <param name="w">image width</param>
/// <param name="h">image height</param>
/// <param name="stride">Bitmap stride</param>
/// <returns>Combined image WITHOUT shapes (for edge detection), shapes are drawn afterward</returns>
cv::Mat VideoWidget::applyRoiZoomToFrame(unsigned char* src, cv::Mat& gray, int w, int h, int stride) {

    // some constants defining the viewing area
    const int diamondW = 600;
    const int diamondH = 0.8 * diamondW;    // approximation from Git issue screenshot

    int combinedW = gray.cols + diamondW;   // original image width + diamond width
    int combinedH = gray.rows + diamondH;   // original image height + diamond height

    cv::Mat combined(combinedH, combinedW, gray.type(), cv::Scalar(0));

    int quadW = combinedW / 2;
    int quadH = combinedH / 2;

    int imgCenterX = combined.cols / 2;
    int imgCenterY = combined.rows / 2;

    int diamondX = imgCenterX - (diamondW / 2);
    int diamondY = imgCenterY - (diamondH / 2);

    std::vector<cv::Point> diamondPts{
        {diamondW / 2, 0},
        {diamondW - 1, diamondH / 2},
        {diamondW / 2, diamondH - 1},
        {0, diamondH / 2}
    };

    cv::Mat edges;
    cv::Canny(gray, edges, canny_low_threshold, canny_high_threshold, canny_kernel_size);

    if (!edges.empty()) {
        auto rois = extractROIs(gray, edges, roi_extraction_margin, roi_max_count);

        if (!rois.empty()) {

            // Apply EWM smoothing to prevent glitching when subject moves
            if (!prev_roi_centroids.empty() && prev_roi_centroids.size() == rois.size()) {
                for (size_t i = 0; i < rois.size(); ++i) {

                    // Find closest previous centroid to current ROI
                    float minDistSq = std::numeric_limits<float>::max();
                    int bestMatch = -1;

                    for (size_t j = 0; j < prev_roi_centroids.size(); ++j) {
                        float dx = rois[i].centroid.x - prev_roi_centroids[j].x;
                        float dy = rois[i].centroid.y - prev_roi_centroids[j].y;
                        float distSq = dx * dx + dy * dy;
                        if (distSq < minDistSq) {
                            minDistSq = distSq;
                            bestMatch = static_cast<int>(j);
                        }
                    }

                    // Apply exponential smoothing if match found and close enough
                    if (bestMatch >= 0 && minDistSq < centroid_matching_threshold) {
                        rois[i].centroid.x = centroid_smoothing_alpha * rois[i].centroid.x +
                                             (1.0f - centroid_smoothing_alpha) * prev_roi_centroids[bestMatch].x;
                        rois[i].centroid.y = centroid_smoothing_alpha * rois[i].centroid.y +
                                             (1.0f - centroid_smoothing_alpha) * prev_roi_centroids[bestMatch].y;
                    }
                }
            }

            // Update cache for next frame
            prev_roi_centroids.clear();
            for (const auto& roi : rois) {
                prev_roi_centroids.push_back(roi.centroid);
            }

            // Find center-most ROI
            cv::Point2f imageCenter(gray.cols / 2.0f, gray.rows / 2.0f);
            
            int centerIdx = -1;
            float minDistSq = std::numeric_limits<float>::max();

            for (size_t i = 0; i < rois.size(); ++i) {
                cv::Point2f c = rois[i].centroid;
                float dx = c.x - imageCenter.x;
                float dy = c.y - imageCenter.y;
                float distSq = dx * dx + dy * dy;
                if (distSq < minDistSq) {
                    minDistSq = distSq;
                    centerIdx = static_cast<int>(i);
                }
            }

            // Place remaining ROIs in quadrants
            for (size_t i = 0; i < rois.size(); ++i) {

                VideoWidget::RoiInfo& roi = rois[i];

                //skip center ROI
                if (static_cast<int>(i) == centerIdx) { 
                    continue; 
                }

                int col = (roi.centroid.x > imageCenter.x) ? 1 : 0;
                int row = (roi.centroid.y > imageCenter.y) ? 1 : 0;

                int x = col * (quadW);
                int y = row * (quadH);

                cv::Mat cropped = zoomCrop(gray, roi.centroid, ROIZoomScale);
                cv::Mat resized;
                cv::resize(cropped, resized, cv::Size(quadW, quadH));

                resized.copyTo(combined(cv::Rect(x, y, quadW, quadH)));
            }

            // Place center ROI in diamond area with mask
            if (centerIdx >= 0) {
                VideoWidget::RoiInfo& roi = rois[centerIdx];

                // Crop the center region of the original image
                int cropSize = std::min(gray.cols, gray.rows) / 2;

                int cropX = (gray.cols - cropSize) / 2;
                int cropY = (gray.rows - cropSize) / 2;

                cv::Mat cropped = zoomCrop(gray, roi.centroid, ROIZoomScale);

                cv::Mat centerResized;
                cv::resize(cropped, centerResized, cv::Size(diamondW, diamondH));

                cv::Mat diamondMask = cv::Mat::zeros(diamondH, diamondW, CV_8UC1);
                cv::fillPoly(diamondMask, std::vector<std::vector<cv::Point>>{diamondPts}, cv::Scalar(255));

                centerResized.copyTo(combined(cv::Rect(diamondX, diamondY, diamondW, diamondH)), diamondMask);
            }
        }

        // Save parameters for drawing shapes AFTER edge detection
        shapeParams.quadW = quadW;
        shapeParams.quadH = quadH;
        shapeParams.imgCenterX = imgCenterX;
        shapeParams.imgCenterY = imgCenterY;
        shapeParams.diamondX = diamondX;
        shapeParams.diamondY = diamondY;
        shapeParams.diamondW = diamondW;
        shapeParams.diamondH = diamondH;
        shapeParams.combinedW = combinedW;
        shapeParams.combinedH = combinedH;
        shapeParams.diamondPts.clear();
        for (auto& p : diamondPts) {
            shapeParams.diamondPts.push_back(cv::Point(p.x + diamondX, p.y + diamondY));
        }
        shapeParams.isValid = true;

        return combined;
    }

    // If no edges detected, return empty Mat
    shapeParams.isValid = false;
    return cv::Mat();
}

/// <summary>
/// Produces a zoomed crop of the source image centered at 'center' point
/// </summary>
/// <param name="src">Source image data</param>
/// <param name="center">Center point to zoom and crop about</param>
/// <param name="zoom">Zoom scale</param>
/// <returns>Mat of cropped/zoomed image area</returns>
cv::Mat VideoWidget::zoomCrop(const cv::Mat& src, const cv::Point& center, float zoom) {
    // Determine the square side length
    int side = static_cast<int>(std::min(src.cols, src.rows) / zoom);

    // Compute top-left corner to center the crop
    int cropX = center.x - side / 2;
    int cropY = center.y - side / 2;

    // Clamp to image bounds
    cropX = std::max(0, std::min(cropX, src.cols - side));
    cropY = std::max(0, std::min(cropY, src.rows - side));

    return src(cv::Rect(cropX, cropY, side, side)).clone();
}

/// <summary>
/// Given a grayscale image and its edge map, extracts largest N marker ROIs based on detected contours
/// </summary>
/// <param name="gray"></param>
/// <param name="edges"></param>
/// <param name="margin"></param>
/// <param name="maxROIs"></param>
/// <returns></returns>
std::vector<RoiInfo> VideoWidget::extractROIs(const cv::Mat& gray, const cv::Mat& edges, int margin, size_t maxROIs) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<RoiInfo> rois;
    for (auto& c : contours) {
        cv::Rect r = cv::boundingRect(c);
        r.x = std::max(0, r.x - margin);
        r.y = std::max(0, r.y - margin);
        r.width = std::min(gray.cols - r.x, r.width + 1 * margin);
        r.height = std::min(gray.rows - r.y, r.height + 1 * margin);

        cv::Moments m = cv::moments(c);
        cv::Point2f centroid(float(m.m10 / m.m00), float(m.m01 / m.m00));

        double area = cv::contourArea(c);
        double perimeter = cv::arcLength(c, true);

		double circularity = 0.0;

        if (perimeter > 0 && area > 10.0) {
            circularity = 100 * (4.0 * CV_PI * area) / (perimeter * perimeter);
        }
        rois.push_back({ circularity, r, centroid });
    }

    // Keep top N largest by area
    std::sort(rois.begin(), rois.end(), [](const RoiInfo& a, const RoiInfo& b) {
        return a.rect.area() > b.rect.area();
        });
    if (rois.size() > maxROIs) {
        rois.resize(maxROIs);
    }
    return rois;
}

/// <summary>
/// Draws shapes as OpenGL overlays on top of rendered textures
/// </summary>
void VideoWidget::drawShapesOverlay(float dstX, float dstY, float dstW, float dstH) {
    if (!shapeParams.isValid) return;

    const float W = float(width());
    const float H = float(height());

    // Coordinate mapping: Combined space → Frame space → Screen space → NDC
    // 1. Combined image (combinedW x combinedH) was resized to (frame_width x frame_height)
    // 2. Frame is fitted into screen rect (dstX, dstY, dstW, dstH)

    const float resizeScaleX = float(frame_width) / float(shapeParams.combinedW);
    const float resizeScaleY = float(frame_height) / float(shapeParams.combinedH);
    const float screenScaleX = dstW / float(frame_width);
    const float screenScaleY = dstH / float(frame_height);

    // Convert from combined image coords to screen coords
    auto toScreenX = [&](float x) { return dstX + x * resizeScaleX * screenScaleX; };
    auto toScreenY = [&](float y) { return dstY + y * resizeScaleY * screenScaleY; };

    // Set up OpenGL for line drawing
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);

    // Color: gray (80, 80, 80) normalized to 0-1
    glColor4f(80.0f/255.0f, 80.0f/255.0f, 80.0f/255.0f, 1.0f);

    // Draw quadrant dividers (use imgCenter from combined space)
    glBegin(GL_LINES);

    // Vertical center line
    float x = toScreenX(shapeParams.imgCenterX);
    glVertex2f(toNdcX(x, W), toNdcY(toScreenY(0), H));
    glVertex2f(toNdcX(x, W), toNdcY(toScreenY(shapeParams.combinedH), H));

    // Horizontal center line
    float y = toScreenY(shapeParams.imgCenterY);
    glVertex2f(toNdcX(toScreenX(0), W), toNdcY(y, H));
    glVertex2f(toNdcX(toScreenX(shapeParams.combinedW), W), toNdcY(y, H));

    glEnd();

    // Draw diamond outline
    if (!shapeParams.diamondPts.empty()) {
        glBegin(GL_LINE_LOOP);
        for (const auto& pt : shapeParams.diamondPts) {
            float sx = toScreenX(pt.x);
            float sy = toScreenY(pt.y);
            glVertex2f(toNdcX(sx, W), toNdcY(sy, H));
        }
        glEnd();
    }

    // Draw tick marks
    const float largeTickLenPx = 0.66f * shapeParams.diamondH;
    const float smallTickLenPx = 0.4f * largeTickLenPx;
    const float tickHSeparationPx = shapeParams.quadW / 4.0f;
    const float tickVSeparationPx = shapeParams.quadH / 4.0f;

    const float centerX = shapeParams.imgCenterX;
    const float centerY = shapeParams.imgCenterY;
    const float diamondLeft = centerX - shapeParams.diamondW / 2.0f;
    const float diamondRight = centerX + shapeParams.diamondW / 2.0f;
    const float diamondTop = centerY - shapeParams.diamondH / 2.0f;
    const float diamondBottom = centerY + shapeParams.diamondH / 2.0f;

    glBegin(GL_LINES);

    // Small tick marks (at diamond edges)
    // Left (vertical)
    glVertex2f(toNdcX(toScreenX(diamondLeft), W), toNdcY(toScreenY(centerY - smallTickLenPx / 2), H));
    glVertex2f(toNdcX(toScreenX(diamondLeft), W), toNdcY(toScreenY(centerY + smallTickLenPx / 2), H));
    // Right (vertical)
    glVertex2f(toNdcX(toScreenX(diamondRight), W), toNdcY(toScreenY(centerY - smallTickLenPx / 2), H));
    glVertex2f(toNdcX(toScreenX(diamondRight), W), toNdcY(toScreenY(centerY + smallTickLenPx / 2), H));
    // Top (horizontal)
    glVertex2f(toNdcX(toScreenX(centerX - smallTickLenPx / 2), W), toNdcY(toScreenY(diamondTop), H));
    glVertex2f(toNdcX(toScreenX(centerX + smallTickLenPx / 2), W), toNdcY(toScreenY(diamondTop), H));
    // Bottom (horizontal)
    glVertex2f(toNdcX(toScreenX(centerX - smallTickLenPx / 2), W), toNdcY(toScreenY(diamondBottom), H));
    glVertex2f(toNdcX(toScreenX(centerX + smallTickLenPx / 2), W), toNdcY(toScreenY(diamondBottom), H));

    // Large tick marks (further out)
    // Left (vertical)
    glVertex2f(toNdcX(toScreenX(diamondLeft - tickHSeparationPx), W), toNdcY(toScreenY(centerY - largeTickLenPx / 2), H));
    glVertex2f(toNdcX(toScreenX(diamondLeft - tickHSeparationPx), W), toNdcY(toScreenY(centerY + largeTickLenPx / 2), H));
    // Right (vertical)
    glVertex2f(toNdcX(toScreenX(diamondRight + tickHSeparationPx), W), toNdcY(toScreenY(centerY - largeTickLenPx / 2), H));
    glVertex2f(toNdcX(toScreenX(diamondRight + tickHSeparationPx), W), toNdcY(toScreenY(centerY + largeTickLenPx / 2), H));
    // Top (horizontal)
    glVertex2f(toNdcX(toScreenX(centerX - largeTickLenPx / 2), W), toNdcY(toScreenY(diamondTop - tickVSeparationPx), H));
    glVertex2f(toNdcX(toScreenX(centerX + largeTickLenPx / 2), W), toNdcY(toScreenY(diamondTop - tickVSeparationPx), H));
    // Bottom (horizontal)
    glVertex2f(toNdcX(toScreenX(centerX - largeTickLenPx / 2), W), toNdcY(toScreenY(diamondBottom + tickVSeparationPx), H));
    glVertex2f(toNdcX(toScreenX(centerX + largeTickLenPx / 2), W), toNdcY(toScreenY(diamondBottom + tickVSeparationPx), H));

    glEnd();

    // Restore OpenGL state
    glDisable(GL_BLEND);
    glLineWidth(1.0f);
}

void VideoWidget::applyEdgeDetection(cv::Mat& gray, int w, int h)
{
    cv::Mat smoothed;
    cv::GaussianBlur(gray, smoothed, cv::Size(3, 3), 1.0);

    cv::Mat edges;
    const double lowThreshold = 100.0;
    const double ratio = 3.0;
    const int kernel_size = 3;
    cv::Canny(smoothed, edges, lowThreshold, lowThreshold * ratio, kernel_size);

    // make this windows's GL context current
    makeCurrent();

    // Ensure edges data is contiguous for GL upload
    cv::Mat edgesC = edges.clone();
        glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, edgeMaskTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#ifdef GL_UNPACK_ROW_LENGTH
    const GLint rowLen = int(edgesC.step / edgesC.elemSize());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLen);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, edgesC.data);
#ifdef GL_UNPACK_ROW_LENGTH
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    
    doneCurrent();
}