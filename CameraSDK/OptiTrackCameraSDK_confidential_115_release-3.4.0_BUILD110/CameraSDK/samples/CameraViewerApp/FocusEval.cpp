#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <optional>
#include <memory>
#include <vector>
#include <cstdlib>

#include <QApplication>
#include <QMetaObject>

#include "cameralibrary.h"

namespace CameraLibrary {

	float EvaluateBitmapFocus(CameraLibrary::Bitmap* bmp) {
		return 1.0f; // placeholder
	}
}

