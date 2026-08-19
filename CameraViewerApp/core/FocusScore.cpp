#include "core/FocusScore.h"

#include <algorithm>

namespace focus {

FocusScoreModel::FocusScoreModel(FocusConfig config)
	: mConfig(config),
	  mObservedSharp(config.lapStdSharp),
	  mObservedBlur(config.lapStdBlur) {}

bool FocusScoreModel::FrameTooDark(double brightestPixel) const {
	return brightestPixel < mConfig.minImageBrightness;
}

double FocusScoreModel::MaskThreshold(double brightestPixel) const {
	return brightestPixel * mConfig.markerThreshRatio;
}

bool FocusScoreModel::TooFewMarkerPixels(int brightPixelCount) const {
	return brightPixelCount < mConfig.minMaskPixels;
}

double FocusScoreModel::Update(double lapStd) {
	// Adjust bounds based on newest values. EWM smoothing provides some
	// resistance against sudden spikes.
	if (lapStd > mObservedSharp) {
		mObservedSharp += mConfig.boundsAlpha * (lapStd - mObservedSharp);
	}
	if (lapStd < mObservedBlur) {
		mObservedBlur += mConfig.boundsAlpha * (lapStd - mObservedBlur);
	}

	// Floor the span so a narrow or inverted range cannot blow up the divide.
	const double range = std::max(mObservedSharp - mObservedBlur, 1.0);

	const double rawScore =
		std::clamp((lapStd - mObservedBlur) / range, 0.0, 1.0);

	mSmoothedScore =
		mConfig.ewmAlpha * rawScore + (1.0 - mConfig.ewmAlpha) * mSmoothedScore;

	return mSmoothedScore;
}

double FocusScoreModel::Decay() {
	mSmoothedScore = (1.0 - mConfig.ewmAlpha) * mSmoothedScore;
	return mSmoothedScore;
}

void FocusScoreModel::Reset() {
	mSmoothedScore = 0.0;
	mObservedSharp = mConfig.lapStdSharp;
	mObservedBlur = mConfig.lapStdBlur;
}

}  // namespace focus
