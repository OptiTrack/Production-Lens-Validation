#include "QtVideoWidget.h"
#include <algorithm>
#include <cstring>
// OpenCV for simple image processing (edge detection)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <QVector3D>

// Specialized GL Viewer for displaying bitmaps from a camera

using namespace CameraLibrary;

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
    glActiveTexture(GL_TEXTURE0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
    vertex_array.release();
    program_shader->release();
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
    // Relaxed is sufficient because this flag is not used for synchronization
    // Edge detection is only applicable for 8pp and 16bpp modes
    const bool shouldApplyEdges = edge_detect_enabled.load(std::memory_order_relaxed) && (bpp == 8 || bpp == 16);
    if (shouldApplyEdges) {
        // Convert source to appropriate cv::Mat based on bpp
        cv::Mat sourceMat;
        // Wrap source buffer in a cv::Mat. Treating it as read-only; OpenCV never writes to this buffer.
        if (bpp == 8) {
            sourceMat = cv::Mat(h, w, CV_8UC1, const_cast<unsigned char*>(src), srcStride);
        } else if (bpp == 16) {
            sourceMat = cv::Mat(h, w, CV_16UC1, const_cast<unsigned char*>(src), srcStride);
        }

        // Convert to 8-bit grayscale for edge detection
        cv::Mat gray;
        if (bpp == 16) {
            sourceMat.convertTo(gray, CV_8U, 1.0 / 256.0);
        } else {
            gray = sourceMat;
        }
        // Always copy the original source bytes into the staging buffer for the base texture
        for (int row = 0; row < h; ++row) {
            const unsigned char* s = src + size_t(row) * size_t(srcStride);
            unsigned char* d = reinterpret_cast<unsigned char*>(byte_array_staging.data()) + size_t(row) * size_t(dstStride);
            std::memcpy(d, s, size_t(dstStride));
        }

        // Then compute and upload the edge mask if we successfully formed a grayscale image
        if (!gray.empty()) {
            applyEdgeDetection(gray, w, h, srcStride);
        }
    } else {
        for (int row = 0; row < h; ++row) {
            const unsigned char* s = src + size_t(row) * size_t(srcStride);
            unsigned char*       d = reinterpret_cast<unsigned char*>(byte_array_staging.data()) + size_t(row) * size_t(dstStride);
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

void VideoWidget::applyEdgeDetection(cv::Mat& gray, int w, int h, int srcStride)
{
    cv::Mat smoothed;
    cv::GaussianBlur(gray, smoothed, cv::Size(3, 3), 1.0);

    cv::Mat edges;
    const double lowThreshold = 50.0;
    const double highThreshold = lowThreshold * 3.0;
    const int kernel_size = 3;
    cv::Canny(smoothed, edges, lowThreshold, highThreshold, kernel_size);

    // Ensure edges data is contiguous for GL upload
    cv::Mat edgesC = edges.clone();
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
}

