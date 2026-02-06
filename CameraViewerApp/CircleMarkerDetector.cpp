#include "CircleMarkerDetector.h"

CircleMarkerDetector::CircleMarkerDetector()
    : m_params()
{
}

CircleMarkerDetector::CircleMarkerDetector(const DetectionParams& params)
    : m_params(params)
{
}

cv::Mat CircleMarkerDetector::ConvertBitmapToMat(CameraLibrary::Bitmap* bmp)
{
    if (!bmp) {
        throw std::runtime_error("Null bitmap passed to CircleMarkerDetector");
    }

    int width = bmp->PixelWidth();
    int height = bmp->PixelHeight();
    int bpp = bmp->GetBitsPerPixel();
    unsigned char* bits = bmp->GetBits();

    int cvType = (bpp == 8) ? CV_8UC1 :
                 (bpp == 24) ? CV_8UC3 :
                 (bpp == 32) ? CV_8UC4 : -1;

    if (cvType == -1) {
        throw std::runtime_error("Unsupported bitmap color depth for circle detection");
    }

    cv::Mat mat(height, width, cvType, const_cast<unsigned char*>(bits));
    return mat.clone(); // Clone to ensure ownership
}

std::vector<CircleMarkerDetector::CircleMarker> CircleMarkerDetector::DetectCircles(CameraLibrary::Bitmap* bmp)
{
    cv::Mat mat = ConvertBitmapToMat(bmp);
    return DetectCirclesFromMat(mat);
}

std::vector<CircleMarkerDetector::CircleMarker> CircleMarkerDetector::DetectCirclesFromMat(const cv::Mat& mat)
{
    // Convert to grayscale
    cv::Mat gray;
    if (mat.channels() == 3) {
        cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
    }
    else if (mat.channels() == 4) {
        cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);
    }
    else {
        gray = mat;
    }

    cv::Mat blurred;
    cv::medianBlur(gray, blurred, 5);

    // Detect circles using Hough Circle Transform
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(
        blurred,
        circles,
        cv::HOUGH_GRADIENT,
        m_params.dp,
        m_params.minDist,
        m_params.param1,
        m_params.param2,
        static_cast<int>(m_params.minRadius),
        static_cast<int>(m_params.maxRadius)
    );

    m_lastDetectionCount = static_cast<int>(circles.size());

    // Convert raw detection results to CircleMarker objects
    std::vector<CircleMarker> result;
    for (const auto& circle : circles) {
        CircleMarker marker;
        marker.center = cv::Point2f(circle[0], circle[1]);
        marker.radius = circle[2];
        marker.isValid = true;
        result.push_back(marker);
    }

    return result;
}

void CircleMarkerDetector::SetDetectionParams(const DetectionParams& params)
{
    m_params = params;
}
