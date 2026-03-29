#include "QtVideoWidget.h"
#include <algorithm>
#include <cstring>
// OpenCV for simple image processing (edge detection)
#include <opencv2/imgproc.hpp>
#include <opencv2/core/cvdef.h>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>
#include <bitmap.h>
#include <GL/gl.h>
#include <cstddef>
#include <memory>
#include <qopenglext.h>
#include <qsurfaceformat.h>
#include <qvectornd.h>
#include <qopenglbuffer.h>
#include <qopenglshaderprogram.h>
#include <qopenglwindow.h>
#include <QPainter>
#include <QImage>
#include <QFont>
#include <QThread>
#include <QMouseEvent>
#include <QFontMetrics>
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
    if (roiLabelsTex) glDeleteTextures(1, &roiLabelsTex);
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
    
    // Update and composite circle markers overlay on top of the frame
    // (disabled when ROI zoom is active)
    {
        const bool roiEnabled = roiZoomEnabled.load(std::memory_order_relaxed);
        const bool circleEnabled = circleDetectionEnabled.load(std::memory_order_relaxed);
        
        std::lock_guard<std::mutex> lock(circleMarkersMutex);
        if (!roiEnabled && circleEnabled && !detectedCircleMarkers.empty()) {
            updateCircleMarkersTexture();
            
            // Composite circle markers using simple blend
            if (circleMarkersTex != 0) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_TEXTURE_2D);
                
                glBindTexture(GL_TEXTURE_2D, circleMarkersTex);
                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                glOrtho(0, width(), 0, height(), -1, 1);
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadIdentity();
                
                float glYTop = height() - dstY;
                float glYBottom = height() - (dstY + dstH);
                
                glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(dstX, glYBottom);              // bottom-left
                glTexCoord2f(1, 0); glVertex2f(dstX + dstW, glYBottom);       // bottom-right
                glTexCoord2f(1, 1); glVertex2f(dstX + dstW, glYTop);          // top-right
                glTexCoord2f(0, 1); glVertex2f(dstX, glYTop);                 // top-left
                glEnd();
                
                glPopMatrix();
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
                glMatrixMode(GL_MODELVIEW);
                
                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_BLEND);
                glDisable(GL_TEXTURE_2D);
            }
        }
    }
    // Draw detected circle markers for error checking (simple overlay)
    drawCircleMarkers(dstX, dstY, dstW, dstH);

    // Draw crosshair at the last clicked pixel
    if (clickedPixel.x >= 0 && clickedPixel.y >= 0 && frame_width > 0 && frame_height > 0) {
        const float sx = dstX + (float(clickedPixel.x) / float(frame_width)) * dstW;
        const float sy = dstY + (float(clickedPixel.y) / float(frame_height)) * dstH;

        constexpr float kCrossRadius = 10.0f;  // crosshair half-length
        constexpr float kCircleRadius = 12.0f; // circle radius
        constexpr int kCircleSegments = 20;    // smoothness

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(1.5f);
        glColor4f(0.0f, 1.0f, 1.0f, 1.0f); // yellow

        // Set up 2D orthographic projection (Y-down)
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, W, H, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // --- Draw crosshair ---
        glBegin(GL_LINES);
        glVertex2f(sx - kCrossRadius, sy);
        glVertex2f(sx + kCrossRadius, sy);
        glVertex2f(sx, sy - kCrossRadius);
        glVertex2f(sx, sy + kCrossRadius);
        glEnd();

        // --- Draw circle ---
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < kCircleSegments; ++i) {
            float theta = 2.0f * 3.1415926f * float(i) / float(kCircleSegments);
            float x = kCircleRadius * cosf(theta);
            float y = kCircleRadius * sinf(theta);
            glVertex2f(sx + x, sy + y);
        }
        glEnd();

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);

        glDisable(GL_BLEND);

        // Debug label: "(px, py) [Quadrant]"
        // Rendered via QImage->texture to avoid QPainter(this)/raw-GL state conflicts.
        static const char* kQuadrantNames[] = { "TL", "TR", "BL", "BR", "Center" };
        const char* quadName = (clickedQuadrant >= 0 && clickedQuadrant <= 4)
                               ? kQuadrantNames[clickedQuadrant] : "?";
        QString label = QString("(%1, %2) [%3]").arg(clickedPixel.x).arg(clickedPixel.y).arg(quadName);

        QFont font;
        font.setPointSize(8);
        font.setBold(true);
        QFontMetrics fm(font);
        const int pad  = 3;
        const int imgW = fm.horizontalAdvance(label) + pad * 2;
        const int imgH = fm.height() + pad * 2;

        QImage labelImg(imgW, imgH, QImage::Format_RGBA8888);
        labelImg.fill(qRgba(0, 0, 0, 180));
        {
            QPainter p(&labelImg);
            p.setFont(font);
            p.setPen(QColor(0, 255, 255));
            p.drawText(pad, pad + fm.ascent(), label);
        }
        QImage glLabel = labelImg.mirrored(false, false);

        GLuint labelTex = 0;
        glGenTextures(1, &labelTex);
        glBindTexture(GL_TEXTURE_2D, labelTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgW, imgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, glLabel.bits());

        // Position label to the right of the circle; nudge left if it would clip
        int tx = int(sx) + int(kCircleRadius) + 4;
        int ty = int(sy) - imgH / 2;
        if (tx + imgW > int(W)) tx = int(sx) - int(kCircleRadius) - imgW - 4;
        if (ty < 0) ty = 0;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, W, H, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(tx,        ty);
        glTexCoord2f(1, 0); glVertex2f(tx + imgW, ty);
        glTexCoord2f(1, 1); glVertex2f(tx + imgW, ty + imgH);
        glTexCoord2f(0, 1); glVertex2f(tx,        ty + imgH);
        glEnd();

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);

        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glDeleteTextures(1, &labelTex);
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

/*
 Mouse button down event handler for QtVideoWidget

 Left click specifies a marker/contour location, right click clears the selection of a particular quadrant.
 If no marker is specified, default behavior is to automatically select a contour in the quadrant.

 Some conversion between click coordinates and original image pixels is required:
 Conversion: Widget px -> frame px (undoes window letterbox scaling) -> original px (undoing ROI zoom + resize, using mapClickToImageCoords)
*/
void VideoWidget::mousePressEvent(QMouseEvent* event)
{
    bool clearSelection = (event->button() == Qt::RightButton);

    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton)
        return;

    // click position in widget pixels
    QPoint c = event->pos(); 

	// Convert widget pixel coordinates to frame pixel coordinates by undoing the letterbox scaling.
    // The camera frame is drawn letterboxed (aspect-ratio preserved) inside the widget, which causes black bars to appear.
    // Compute the scale factor and the top-left offset of the drawn image rect.
    int w = width();
    int h = height();
    float scale = std::min(float(w) / float(frame_width), float(h) / float(frame_height));
    int drawW = int(frame_width * scale);
    int drawH = int(frame_height * scale);

    int offsetX = (w - drawW) / 2;
    int offsetY = (h - drawH) / 2;

    // Ignore clicks outside the video area.
    if (c.x() < offsetX || c.x() >= offsetX + drawW ||
        c.y() < offsetY || c.y() >= offsetY + drawH) {
        return;
    }

    // Convert widget pixel to frame pixel by removing the letterbox offset and undoing the scale.
    int px = (c.x() - offsetX) / scale;
    int py = (c.y() - offsetY) / scale;

    // Determine which quadrant was clicked
    // Check the center diamond first since it overlaps the 4x quadrant grid, then fall
    // back to a simple quadrant split for the four corner slots.
    int quadrant = -1;

    if (roiZoomEnabled.load(std::memory_order_relaxed) && shapeParams.isValid
        && shapeParams.combinedW > 0 && shapeParams.combinedH > 0) {
        // Remap the frame-pixel click into combined-image coordinates so we can test
        // it against the diamond polygon (which lives in combined-image space).
        float cx = float(px) * float(shapeParams.combinedW) / float(frame_width);
        float cy = float(py) * float(shapeParams.combinedH) / float(frame_height);
        if (cv::pointPolygonTest(shapeParams.diamondPts, cv::Point2f(cx, cy), false) >= 0.0f) {
            quadrant = 4; // point lies inside center diamond, select quadrant 4
        }
    }

    if (quadrant < 0) {
        // Point not in the diamond, now check the four quadrants.
        // Slots are numbered: 0=TL, 1=TR, 2=BL, 3=BR  (col + row * 2).
        int col = (px > frame_width  / 2) ? 1 : 0;
        int row = (py > frame_height / 2) ? 1 : 0;
        quadrant = col + row * 2;
    }

    // Retain the original framep px location for drawing the crosshair overlay.
    clickedPixel    = cv::Point(px, py);
    clickedQuadrant = quadrant;
    
    /*
        Finally, convert frame pixels to original image pixels
        When ROI zoom is active each panel shows a zoomed crop of the original image, resized to fill the quadrant.  
        mapClickToImageCoords reverses that transform to produce the position of the clicked point in the original (un-zoomed) camera image.
    
        On right-click we skip this and store point (-1,-1) to clear the pinned marker instead.
    */

    cv::Point2f imagePt;
    cv::Point newPoint;

    if (clearSelection) {
        imagePt = cv::Point2f(float(px), float(py));
        newPoint = cv::Point(-1, -1);
    }
    else {
        imagePt = mapClickToImageCoords(px, py, quadrant);
        newPoint = cv::Point(int(imagePt.x), int(imagePt.y));
    }

    quadrantClickPositions[quadrant] = newPoint;

    qDebug("Clicked pixel: (%d, %d) -> image: (%d, %d) in quadrant %d", px, py, newPoint.x, newPoint.y, quadrant);
    emit pixelClicked(newPoint.x, newPoint.y, quadrant);
    requestUpdate();
}

// Reverses a single zoomCrop+resize step for one display panel.
//
// When ROI zoom is active, each quadrant shows a small square crop of the original image
// that has been zoomed in and then stretched to fill the panel (panelW x panelH pixels).
// A user click at panel-local position (lx, ly) therefore corresponds to some point
// inside that original crop window — this function works out which one.
//
//   prevCentroid — the zoom center used when the panel was rendered (in original image coords)
//   lx, ly       — click position relative to the top-left corner of the panel (combined-image coords)
//   panelW/H     — pixel dimensions of the panel in the combined image
//
// Returns: Corresponding point in original image coordinates
cv::Point2f VideoWidget::inverseZoomCrop(cv::Point2f prevCentroid, float lx, float ly, int panelW, int panelH) const {

    // Reproduce the same crop window that zoomCrop used when rendering this panel.
    // side = the square crop size in the original image (smaller = more zoomed in).
    int side = static_cast<int>(std::min(frame_width, frame_height) / ROIZoomScale);

    // Clamp the crop origin so the window doesn't go outside the image (same logic as zoomCrop).
    int cropX = std::max(0, std::min(int(prevCentroid.x) - side / 2, frame_width  - side));
    int cropY = std::max(0, std::min(int(prevCentroid.y) - side / 2, frame_height - side));

    // The panel stretches the crop window over (panelW x panelH) pixels, so divide by
    // that scale to convert panel-local pixels back to original-image pixels.
    return { cropX + lx * float(side) / float(panelW),
             cropY + ly * float(side) / float(panelH) };
}

// Converts a click at frame-pixel position (px, py) back to original image coordinates.
//
// When ROI zoom is active, what the user sees on screen is NOT the raw camera image.
// The view is a combined image made of five zoomed panels that has then been rescaled to fit
// the frame dimensions. A click therefore lands somewhere inside one of those panels,
// and we need to undo two layers of scaling to find where the user actually clicked in
// the original camera image:
//
//   Layer 1: frame to combined image  (the combined image was resized to frame dimensions)
//   Layer 2: combined image to original image  (each panel is a zoomed crop, via inverseZoomCrop)
//
// If ROI zoom is not active, or the clicked quadrant has never acquired a marker (no zoom
// center to invert from), the frame-pixel coordinates are returned unchanged.
cv::Point2f VideoWidget::mapClickToImageCoords(int px, int py, int quadrant) const {
    if (!roiZoomEnabled.load(std::memory_order_relaxed)
            || !shapeParams.isValid
            || shapeParams.combinedW <= 0 || shapeParams.combinedH <= 0)
        return cv::Point2f(float(px), float(py));

    // Layer 1: rescale frame pixels → combined-image pixels.
    float cx = float(px) * float(shapeParams.combinedW) / float(frame_width);
    float cy = float(py) * float(shapeParams.combinedH) / float(frame_height);

    // Layer 2: subtract the panel's top-left origin within the combined image, then
    // call inverseZoomCrop to undo the zoom+resize for that specific panel.
    if (quadrant == 4 && quadrantSlots[4].hasTrack) {
        // Center diamond panel: its top-left is at (diamondX, diamondY) in combined-image space.
        float lx = cx - float(shapeParams.diamondX);
        float ly = cy - float(shapeParams.diamondY);
        return inverseZoomCrop(quadrantSlots[4].centroid, lx, ly,
                               shapeParams.diamondW, shapeParams.diamondH);
    }
    if (quadrant >= 0 && quadrant <= 3 && quadrantSlots[quadrant].hasTrack) {
        // Corner quadrant panels are tiled: slot (col, row) starts at (col*quadW, row*quadH).
        int col = quadrant % 2;
        int row = quadrant / 2;
        float lx = cx - float(col * shapeParams.quadW);
        float ly = cy - float(row * shapeParams.quadH);
        return inverseZoomCrop(quadrantSlots[quadrant].centroid, lx, ly,
                               shapeParams.quadW, shapeParams.quadH);
    }

    return cv::Point2f(float(px), float(py));
}

// Refines a slot's zoom-center to the sub-pixel-accurate center reported by the
// CircleMarkerDetector, rather than using the coarser contour centroid from extractROIs.
//
// The circle detector runs a separate, more precise detection pass and stores its
// results in detectedCircleMarkers.  This function searches that list for the circle
// closest to the current quadrant centroid that also falls within the quadrant's visible crop
// window, then returns its center.  If no such circle exists the original centroid is
// returned unchanged, so the quadrant continues to use the contour-based position.
//
// The crop window constraint prevents snapping to a circle from a different marker that
// happens to be closer in absolute image distance but is not actually visible in this panel.
cv::Point2f VideoWidget::snapCentroidToCircle(cv::Point2f centroid) {
    // Reproduce the crop window size from zoomCrop so we know the maximum distance a
    // circle can be from the centroid and still be visible inside the zoomed panel.
    const int side        = static_cast<int>(std::min(frame_width, frame_height) / ROIZoomScale);
    const float maxDistSq = float(side / 2) * float(side / 2); // squared to avoid sqrt

    std::lock_guard<std::mutex> lock(circleMarkersMutex);

    float bestDistSq = maxDistSq; // reject any circle farther away than the crop window half-side
    const CircleMarkerDetector::CircleMarker* best = nullptr;

    for (const auto& marker : detectedCircleMarkers) {
        float dx  = marker.center.x - centroid.x;
        float dy  = marker.center.y - centroid.y;
        float dSq = dx * dx + dy * dy;
        if (dSq < bestDistSq) {
            bestDistSq = dSq;
            best       = &marker;
        }
    }

    return best ? best->center : centroid;
}

// Builds the "combined image" that is displayed when ROI zoom mode is active.
//
// OVERVIEW
// ---------
// The combined image is a single cv::Mat that tiles five zoomed views of the camera frame:
//
//   ┌────────────┬────────────┐
//   │  TL (0)    │  TR (1)    │
//   │        ◇ center ◇       │
//   │  BL (2)    │  BR (3)    │
//   └────────────┴────────────┘
//
// Each corner panel (quadrants 0-3) is a zoomed crop centered on the outermost marker dot
// in that corner of the original image. The center diamond panel (quadrant 4) shows the
// center marker. Zooming in lets the user inspect circularity and focus quality up close.
//
// QUADRANT NUMBERING
// -------------------
// Quadrants are indexed as  col + row * 2:
//   0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right, 4 = center diamond
//
// PER-QUADRANT STATE  (quadrantSlots[])
// --------------------------------------
// Each quadrant carries:
//   centroid    — the zoom center in original image coordinates (updated every frame)
//   circularity — how circular the tracked marker contour is (0=line, 1=perfect circle)
//   hasTrack    — whether the quadrant has ever acquired a marker this session
//
// RETURNS
// ---------
// The combined image WITHOUT any overlay shapes drawn on it.
// Shapes (grid lines, diamond border, circularity labels) are drawn afterward in drawShapesOverlay() so that edge
// detection sees clean pixel data.  
// 
// Returns an empty Mat if no edges were found.
cv::Mat VideoWidget::applyRoiZoomToFrame(unsigned char* src, cv::Mat& gray, int w, int h, int stride) {
    const int diamondW = 600;
    const int diamondH = 0.8 * diamondW;    // aspect ratio from reference screenshot

    int combinedW = gray.cols + diamondW;
    int combinedH = gray.rows + diamondH;

    cv::Mat combined(combinedH, combinedW, gray.type(), cv::Scalar(0)); // black canvas

    int quadW = combinedW / 2;  // width of each corner panel
    int quadH = combinedH / 2;  // height of each corner panel

    // Center of the combined canvas — also the center of the diamond panel.
    int imgCenterX = combined.cols / 2;
    int imgCenterY = combined.rows / 2;

    // Top-left corner of the diamond panel rect within the combined image.
    int diamondX = imgCenterX - (diamondW / 2);
    int diamondY = imgCenterY - (diamondH / 2);

    // Diamond polygon vertices in diamond-local coordinates (origin = diamondX, diamondY).
    // Used both for masking the rendered crop and for hit-testing mouse clicks.
    std::vector<cv::Point> diamondPts{
        {diamondW / 2, 0},           // top
        {diamondW - 1, diamondH / 2},// right
        {diamondW / 2, diamondH - 1},// bottom
        {0, diamondH / 2}            // left
    };

    // --- Detect marker candidates via edge detection ---
    // Run Canny on the grayscale frame to find edges, then extractROIs groups those
    // edges into individual marker blobs with bounding rects, centroids, and circularity.
    cv::Mat edges;
    cv::Canny(gray, edges, canny_low_threshold, canny_high_threshold, canny_kernel_size);

    if (!edges.empty()) {
        auto rois = extractROIs(gray, edges, roi_extraction_margin, roi_max_count);

        if (!rois.empty()) {
            shapeParams.roiLabels.clear();

            cv::Point2f imageCenter(gray.cols / 2.0f, gray.rows / 2.0f);

            // --- Assign ROIs to quadrants ---
            // Quadrant 4 (center diamond): the ROI whose centroid is closest to the image center.
            int centerIdx = -1;
            {
                float minDistSq = std::numeric_limits<float>::max();
                for (size_t i = 0; i < rois.size(); ++i) {
                    float dx = rois[i].centroid.x - imageCenter.x;
                    float dy = rois[i].centroid.y - imageCenter.y;
                    float d = dx*dx + dy*dy;
                    
                    if (d < minDistSq) { 
                        minDistSq = d; 
                        centerIdx = static_cast<int>(i); 
                    }
                }
            }

            // Quadrants 0-3 (corner panels): bin every other ROI into the quadrant that contains its centroid.  
            // The center marker (centerIdx) is excluded from this pool.
            std::array<std::vector<size_t>, 4> slotCandidates;
            for (size_t i = 0; i < rois.size(); ++i) {
                if (static_cast<int>(i) == centerIdx) continue;
                int col = (rois[i].centroid.x > imageCenter.x) ? 1 : 0;
                int row = (rois[i].centroid.y > imageCenter.y) ? 1 : 0;
                slotCandidates[col + row * 2].push_back(i);
            }

            /*
                --- Select the best candidate for the four square quadrants and update centroid ---
            
                Priority order for choosing which candidate to zoom into:
                1. Manual pin  — user clicked a specific marker; pick the ROI closest to that point.
                2. Active track — quadrant has a marker, so pick the candidate closest to the
                                 last-known centroid (frame-to-frame continuity).
                                 If nothing is close enough the track is considered lost.
                3. First acquisition / track lost — pick the candidate closest to this quadrant's
                                 corner of the image (i.e. the outermost marker in that corner).
            
                Once a candidate is chosen, the centroid is updated via exponential moving average
                (EMA) to smooth jitter. A manual pin bypasses EMA and locks the centroid directly.
            */
            int id = 0;
            for (int slot = 0; slot < 4; ++slot) {

                const auto& cands = slotCandidates[slot];

                // No markers detected in this quadrant this frame, quadrant is blank
                if (cands.empty()) {
                    quadrantSlots[slot].hasTrack = false;
                    continue;
                }

                int chosen = -1;
                const cv::Point2f& clickPos = quadrantClickPositions[slot];

                // Priority 1: manual pin — find the candidate nearest the clicked point.
                if (clickPos.x != -1 && clickPos.y != -1) {
                    float minDist = std::numeric_limits<float>::max();
                    for (size_t idx : cands) {
                        float dx = rois[idx].centroid.x - clickPos.x;
                        float dy = rois[idx].centroid.y - clickPos.y;
                        float d = dx*dx + dy*dy;
                        if (d < minDist) { 
                            minDist = d; 
                            chosen = static_cast<int>(idx); 
                        }
                    }
                } 

                // Priority 2: active track - find the candidate nearest the previous centroid.
                else if (quadrantSlots[slot].hasTrack) {
                    float minDist = std::numeric_limits<float>::max();
                    for (size_t idx : cands) {
                        float dx = rois[idx].centroid.x - quadrantSlots[slot].centroid.x;
                        float dy = rois[idx].centroid.y - quadrantSlots[slot].centroid.y;
                        float d = dx*dx + dy*dy;
                        if (d < minDist) { minDist = d; chosen = static_cast<int>(idx); }
                    }
                    if (minDist >= centroid_matching_threshold) chosen = -1; // too far - track lost
                }

                // Priority 3: no track — pick the candidate closest to this slot's image corner
                if (chosen < 0) {
                    int col = slot % 2;
                    int row = slot / 2;
                    cv::Point2f corner(float(col * gray.cols), float(row * gray.rows));
                    float minDist = std::numeric_limits<float>::max();
                    for (size_t idx : cands) {
                        float dx = rois[idx].centroid.x - corner.x;
                        float dy = rois[idx].centroid.y - corner.y;
                        float d = dx*dx + dy*dy;
                        if (d < minDist) { minDist = d; chosen = static_cast<int>(idx); }
                    }
                }

                // Update this slot's state with the chosen candidate.
                auto& s = quadrantSlots[slot];
                if (quadrantClickPositions[slot].x != -1 && quadrantClickPositions[slot].y != -1) {
                    // Manual pin: lock centroid directly to the clicked image coordinate.
                    s.centroid.x = quadrantClickPositions[slot].x;
                    s.centroid.y = quadrantClickPositions[slot].y;
                } else {
                    if (s.hasTrack) {
                        // EMA smoothing: blend the new detection toward the previous centroid.
                        // alpha (centroid_smoothing_alpha) controls how quickly the centroid
                        // follows movement — lower = smoother but slower to react.
                        s.centroid.x = centroid_smoothing_alpha * rois[chosen].centroid.x
                            + (1.0f - centroid_smoothing_alpha) * s.centroid.x;
                        s.centroid.y = centroid_smoothing_alpha * rois[chosen].centroid.y
                            + (1.0f - centroid_smoothing_alpha) * s.centroid.y;
                    } else {
                        // First acquisition: snap directly with no blending.
                        s.centroid = rois[chosen].centroid;
                    }
                }
                s.hasTrack = true;
                // Record circularity from the chosen candidate so the displayed score
                // always corresponds to the marker this slot is actually zoomed into.
                s.circularity = rois[chosen].circularity;

                // Optional refinement: if the CircleMarkerDetector is running, replace the
                // contour centroid with its more precise circle center for a tighter zoom.
                // Only done when the user has not manually pinned this slot.
                if (quadrantClickPositions[slot].x == -1
                        && circleDetectionEnabled.load(std::memory_order_relaxed)) {
                    s.centroid = snapCentroidToCircle(s.centroid);
                }
            }

            // --- Render corner panels into the combined image ---
            for (int slot = 0; slot < 4; ++slot) {
                if (!quadrantSlots[slot].hasTrack) continue;
                int col = slot % 2;
                int row = slot / 2;
                int x = col * quadW; // top-left x of this panel in the combined image
                int y = row * quadH; // top-left y of this panel in the combined image

                // Crop a square region around the tracked centroid and stretch it to panel size.
                cv::Mat cropped = zoomCrop(gray, quadrantSlots[slot].centroid, ROIZoomScale);
                cv::Mat resized;
                cv::resize(cropped, resized, cv::Size(quadW, quadH));
                resized.copyTo(combined(cv::Rect(x, y, quadW, quadH)));

                // Record the panel center and circularity so drawShapesOverlay can label it.
                shapeParams.roiLabels.push_back({
                    cv::Point2f(float(x + quadW / 2), float(y + quadH / 2)),
                    quadrantSlots[slot].circularity,
                    id
                });
                id++;
            }

            // --- Update and render the center diamond panel (slot 4) ---
            if (centerIdx >= 0) {
                auto& s = quadrantSlots[4];

                // Same priority order as corner slots:
                //   1. Manual click overrides everything.
                //   2. EMA toward detected centroid when already tracking.
                //   3. Hard-snap on first acquisition.
                if (quadrantClickPositions[4].x != -1 && quadrantClickPositions[4].y != -1) {
                    s.centroid.x = quadrantClickPositions[4].x;
                    s.centroid.y = quadrantClickPositions[4].y;
                } else if (s.hasTrack) {
                    s.centroid.x = centroid_smoothing_alpha * rois[centerIdx].centroid.x
                        + (1.0f - centroid_smoothing_alpha) * s.centroid.x;
                    s.centroid.y = centroid_smoothing_alpha * rois[centerIdx].centroid.y
                        + (1.0f - centroid_smoothing_alpha) * s.centroid.y;
                } else {
                    s.centroid = rois[centerIdx].centroid;
                }
                s.hasTrack = true;

                // Circularity: when the user has manually pinned this slot, find the ROI whose
                // centroid is closest to the clicked point so we report the score for the selected marker.
                {
                    int circularityIdx = centerIdx;
                    const cv::Point2f& clickPos4 = quadrantClickPositions[4];
                    if (clickPos4.x != -1 && clickPos4.y != -1) {
                        float minDistSq = std::numeric_limits<float>::max();
                        for (size_t i = 0; i < rois.size(); ++i) {
                            float dx = rois[i].centroid.x - clickPos4.x;
                            float dy = rois[i].centroid.y - clickPos4.y;
                            float d = dx*dx + dy*dy;
                            if (d < minDistSq) { minDistSq = d; circularityIdx = static_cast<int>(i); }
                        }
                    }
                    s.circularity = rois[circularityIdx].circularity;
                }

                // Optional circle-detector refinement (same as corner slots).
                if (quadrantClickPositions[4].x == -1
                        && circleDetectionEnabled.load(std::memory_order_relaxed)) {
                    s.centroid = snapCentroidToCircle(s.centroid);
                }

                // Crop and resize the center marker view, then copy it into the combined image
                // through a diamond-shaped mask so only the diamond area is filled.
                cv::Mat cropped = zoomCrop(gray, s.centroid, ROIZoomScale);
                cv::Mat centerResized;
                cv::resize(cropped, centerResized, cv::Size(diamondW, diamondH));

                cv::Mat diamondMask = cv::Mat::zeros(diamondH, diamondW, CV_8UC1);
                cv::fillPoly(diamondMask, std::vector<std::vector<cv::Point>>{diamondPts}, cv::Scalar(255));
                centerResized.copyTo(combined(cv::Rect(diamondX, diamondY, diamondW, diamondH)), diamondMask);

                shapeParams.roiLabels.push_back({
                    cv::Point2f(float(diamondX + diamondW / 2), float(diamondY + diamondH / 2)),
                    s.circularity,
                    id
                });
            } else {
                quadrantSlots[4].hasTrack = false;
            }
        }

        // Cache all layout geometry needed by drawShapesOverlay() and mapClickToImageCoords().
        // These values are derived from the combined image dimensions computed above and must
        // stay in sync with what was actually rendered this frame.
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

        // Translate diamond vertices from diamond-local coords to combined-image coords
        // so pointPolygonTest in mousePressEvent can use them directly.
        shapeParams.diamondPts.clear();
        for (auto& p : diamondPts) {
            shapeParams.diamondPts.push_back(cv::Point(p.x + diamondX, p.y + diamondY));
        }
        shapeParams.isValid = true;

        return combined;
    }

    // No edges found this frame — signal that the combined image is not valid.
    shapeParams.isValid = false;
    shapeParams.roiLabels.clear();
    return cv::Mat();
}

// Extracts a square crop from `src` centered on point `center` and zoomed in by `zoom` amount.
//
// A zoom of 1.0 would crop the entire shorter dimension of the image (no zoom).
// A zoom of 2.0 crops half that area, making objects appear twice as large when the
// result is stretched back to the same display size.  Higher zoom = tighter crop.
//
// The crop is always square (side = min(src width, src height) / zoom) so that
// stretching it to a rectangular panel doesn't add extra distortion beyond the aspect ratio.
cv::Mat VideoWidget::zoomCrop(const cv::Mat& src, const cv::Point2f& center, float zoom) {
    int side = static_cast<int>(std::min(src.cols, src.rows) / zoom);

    // Place the crop window so `center` is in the middle of it.
    int cropX = center.x - side / 2;
    int cropY = center.y - side / 2;

    // Clamp so the window stays fully inside the image.
    cropX = std::max(0, std::min(cropX, src.cols - side));
    cropY = std::max(0, std::min(cropY, src.rows - side));

    return src(cv::Rect(cropX, cropY, side, side)).clone();
}

/// <summary> 
/// Finds all markers in the frame and returns them as a list of RoiInfo structs.
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
        r.width  = std::min(gray.cols - r.x, r.width  + 1 * margin);
        r.height = std::min(gray.rows - r.y, r.height + 1 * margin);

        cv::Moments m = cv::moments(c);
        cv::Point2f centroid(float(m.m10 / m.m00), float(m.m01 / m.m00));

        // Only calculate circularity when circle detection is enabled
        // Use ellipse-fitting method for consistency with CircleMarkerDetector (0-1 scale)
        double circularity = 0.0;
        if (circleDetectionEnabled.load(std::memory_order_relaxed)) {
            if (c.size() >= 5) {
                try {
                    cv::RotatedRect ellipse = cv::fitEllipse(c);
                    float majorAxis = std::max(ellipse.size.width, ellipse.size.height) / 2.0f;
                    float minorAxis = std::min(ellipse.size.width, ellipse.size.height) / 2.0f;
                    if (majorAxis > 0) {
                        circularity = std::clamp(static_cast<double>(minorAxis / majorAxis), 0.0, 1.0);
                    }
                } catch (...) {
                    circularity = 0.0;
                }
            }
        }
        rois.push_back({ circularity, r, centroid });
    }

    // Sort largest-area first so that the most prominent markers appear at the front of
    // the list, then truncate to the caller's requested cap.
    std::sort(rois.begin(), rois.end(), [](const RoiInfo& a, const RoiInfo& b) {
        return a.rect.area() > b.rect.area();
    });
    if (rois.size() > maxROIs)
        rois.resize(maxROIs);

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

    // Draw ROI circularity labels as a QPainter texture overlay
    if (!shapeParams.roiLabels.empty()) {
        QImage overlay(width(), height(), QImage::Format_RGBA8888);
        overlay.fill(Qt::transparent);

        {
            QPainter painter(&overlay);
            painter.setRenderHint(QPainter::Antialiasing, true);
            QFont font;
            font.setPointSize(8);
            font.setBold(true);
            painter.setFont(font);
            QFontMetrics fm(font);
            QColor cyanColor(0, 255, 255);

            for (const auto& lbl : shapeParams.roiLabels) {
                float sx = toScreenX(lbl.combinedPos.x);
                float sy = toScreenY(lbl.combinedPos.y);

                QString circularityText =
                    QString("c:%1%\nID:%2")
                    .arg(lbl.circularity * 100.0f, 0, 'f', 1)
                    .arg(lbl.id);

                QRect textRect = fm.boundingRect(
                    QRect(0, 0, 200, 100),  // max bounds
                    Qt::TextWordWrap,
                    circularityText
                );

                int tx = static_cast<int>(sx) - textRect.width() / 2;
                int ty = static_cast<int>(sy) + 16;

                QRect drawRect(tx, ty, textRect.width(), textRect.height());

                painter.fillRect(drawRect.adjusted(-2, -2, 2, 2), QColor(0, 0, 0, 160));
                painter.setPen(cyanColor);

                painter.drawText(drawRect, Qt::AlignCenter, circularityText);
            }
        }

        // Flip vertically: QPainter y=0 is top, GL y=0 is bottom
        QImage flipped = overlay.flipped(Qt::Vertical);

        if (roiLabelsTex != 0) {
            glDeleteTextures(1, &roiLabelsTex);
            roiLabelsTex = 0;
        }
        glGenTextures(1, &roiLabelsTex);
        glBindTexture(GL_TEXTURE_2D, roiLabelsTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width(), height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, flipped.bits());

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        glColor4f(1.f, 1.f, 1.f, 1.f); // reset color so texture isn't modulated by gray line color

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, width(), 0, height(), -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glBegin(GL_QUADS);
        glTexCoord2f(0.f, 0.f); glVertex2i(0,        0);
        glTexCoord2f(1.f, 0.f); glVertex2i(width(),  0);
        glTexCoord2f(1.f, 1.f); glVertex2i(width(),  height());
        glTexCoord2f(0.f, 1.f); glVertex2i(0,        height());
        glEnd();

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);

        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
    }
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

void VideoWidget::drawCircleMarkers(float dstX, float dstY, float dstW, float dstH) {
    // Disable circle marker rendering when ROI zoom is enabled or circle detection is not enabled
    const bool roiEnabled = roiZoomEnabled.load(std::memory_order_relaxed);
    const bool circleEnabled = circleDetectionEnabled.load(std::memory_order_relaxed);
    
    if (roiEnabled || !circleEnabled) {
        return;
    }

    // Regenerate texture from current markers before drawing
    {
        std::lock_guard<std::mutex> lock(circleMarkersMutex);
        
        if (!detectedCircleMarkers.empty()) {
            updateCircleMarkersTexture();
        } else {
            // Clear texture if no markers
            if (circleMarkersTex != 0) {
                glDeleteTextures(1, &circleMarkersTex);
                circleMarkersTex = 0;
            }
        }
    }
    
    // If no texture, nothing to draw
    if (circleMarkersTex == 0) {
        return;
    }

    // Bind and render the circle markers texture as an overlay
    // Similar to how edge mask is rendered
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, circleMarkersTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
}

void VideoWidget::updateCircleMarkersTexture() {
    if (frame_width <= 0 || frame_height <= 0) {
        return;
    }
    
    if (detectedCircleMarkers.empty()) {
        // Clear the texture if no markers
        if (circleMarkersTex != 0) {
            glDeleteTextures(1, &circleMarkersTex);
            circleMarkersTex = 0;
        }
        return;
    }

    try {
        if (frame_width > 4096 || frame_height > 4096) {
            qWarning("[videowidget] Frame dimensions too large for marker texture: %d x %d", frame_width, frame_height);
            return;
        }

        // Create RGBA image with transparent background
        QImage overlay(frame_width, frame_height, QImage::Format_RGBA8888);
        if (overlay.isNull()) {
            qWarning("[videowidget] Failed to create QImage for circle markers");
            return;
        }
        
        overlay.fill(qRgba(0, 0, 0, 0));  // Transparent background

        {
            QPainter painter(&overlay);
            if (!painter.isActive()) {
                qWarning("[videowidget] Failed to create painter for circle markers");
                return;
            }
            
            painter.setRenderHint(QPainter::Antialiasing, true);

            // Set font for circularity labels
            QFont font = painter.font();
            font.setPointSize(8);
            font.setBold(true);
            painter.setFont(font);

            // Cyan color for markers and text (light blue)
            QColor cyanColor(0, 255, 255);
            QPen circlePen(cyanColor, 2);
            circlePen.setCapStyle(Qt::RoundCap);
            painter.setPen(circlePen);

            for (const auto& marker : detectedCircleMarkers) {
                int sx = static_cast<int>(marker.center.x);
                int sy = static_cast<int>(marker.center.y);
                int sr = static_cast<int>(marker.radius);

                if (sx < 0 || sy < 0 || sx >= frame_width || sy >= frame_height) {
                    continue;
                }

                QString circularityText =
                    QString("c:%1%\nID:%2")
                    .arg(marker.circularity * 100.0f, 0, 'f', 1)
                    .arg(marker.id);

                QFontMetrics fm(painter.font());

                QRect textRect = fm.boundingRect(
                    QRect(0, 0, 200, 100),
                    Qt::TextWordWrap,
                    circularityText
                );

                int tx = sx - textRect.width() / 2;
                int ty = sy + sr + 16;

                QRect drawRect(tx, ty, textRect.width(), textRect.height());

                painter.fillRect(drawRect.adjusted(-2, -2, 2, 2), QColor(0, 0, 0, 180));
                painter.setPen(cyanColor);
                painter.drawText(drawRect, Qt::AlignCenter, circularityText);
                painter.setPen(circlePen);
            }
        }

        // Convert QImage to GL texture
        QImage glImage = overlay.convertToFormat(QImage::Format_RGBA8888);
        if (glImage.isNull()) {
            qWarning("[videowidget] Failed to convert image format for GL");
            return;
        }
        
        glImage = glImage.mirrored(false, true);

        // Create or update GL texture
        if (circleMarkersTex != 0) {
            glDeleteTextures(1, &circleMarkersTex);
        }

        glGenTextures(1, &circleMarkersTex);
        glBindTexture(GL_TEXTURE_2D, circleMarkersTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame_width, frame_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, glImage.bits());
        glBindTexture(GL_TEXTURE_2D, 0);
        
    } catch (const std::exception& e) {
        qWarning("[videowidget] Exception in updateCircleMarkersTexture: %s", e.what());
        if (circleMarkersTex != 0) {
            glDeleteTextures(1, &circleMarkersTex);
            circleMarkersTex = 0;
        }
    } catch (...) {
        qWarning("[videowidget] Unknown exception in updateCircleMarkersTexture");
        if (circleMarkersTex != 0) {
            glDeleteTextures(1, &circleMarkersTex);
            circleMarkersTex = 0;
        }
    }
}

