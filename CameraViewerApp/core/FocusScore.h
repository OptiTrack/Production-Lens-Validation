#pragma once

// ---------------------------------------------------------------------------
// Platform-independent focus scoring math.
//
// This header intentionally depends on nothing but the C++ standard library:
// no Qt, no OpenCV, and no Camera SDK. That keeps the scoring logic buildable
// and unit-testable on any platform (see /tests), while the image processing
// that produces the inputs stays in FocusEval.cpp alongside OpenCV.
//
// FocusEvaluator measures marker-edge sharpness as the standard deviation of a
// Laplacian, taken over a mask of bright pixels. That raw figure has no fixed
// scale: it depends on the lens, the camera and the exposure. FocusScoreModel
// turns it into a 0..1 score by tracking the blurriest and sharpest values it
// has seen this session and reporting where the current frame sits between
// them.
// ---------------------------------------------------------------------------

namespace focus {

/// Tuning constants for the focus score. The defaults are the values the
/// application shipped with; tests construct their own to pin down behaviour.
struct FocusConfig {
	/// Starting guess for the blurriest Laplacian stddev worth reporting.
	double lapStdBlur = 15.0;
	/// Starting guess for a fully sharp Laplacian stddev.
	double lapStdSharp = 200.0;
	/// How fast the observed bounds move towards a new extreme (0..1).
	/// Small values stop one transient frame from rescaling the readout.
	double boundsAlpha = 0.05;
	/// Exponentially weighted mean factor applied to the reported score (0..1).
	/// Also the rate at which the score decays when there is no signal.
	double ewmAlpha = 0.2;
	/// Minimum number of bright pixels before a frame is worth measuring.
	int minMaskPixels = 3;
	/// Bright-pixel mask cut-off, as a fraction of the brightest pixel.
	double markerThreshRatio = 0.5;
	/// A frame whose brightest pixel is below this cannot hold a marker.
	double minImageBrightness = 30.0;
};

/// Converts raw Laplacian standard deviations into a smoothed 0..1 focus score.
///
/// Not thread safe by itself: FocusEvaluator owns a mutex and serialises the
/// calls, which keeps the locking policy in one place.
class FocusScoreModel {
public:
	explicit FocusScoreModel(FocusConfig config = FocusConfig{});

	// --- frame gating -------------------------------------------------------
	// Cheap predicates over image statistics, kept here so every threshold
	// lives in one place and can be tested without an image.

	/// True when the frame is too dark to contain a marker at all.
	bool FrameTooDark(double brightestPixel) const;

	/// Intensity cut-off for the bright-pixel mask of a frame.
	double MaskThreshold(double brightestPixel) const;

	/// True when too little of the frame is bright enough to measure.
	bool TooFewMarkerPixels(int brightPixelCount) const;

	// --- scoring ------------------------------------------------------------

	/// Folds one frame's Laplacian stddev into the score: widens the observed
	/// bounds if this frame is a new extreme, rescales onto 0..1, and applies
	/// the exponentially weighted mean. Returns the new smoothed score.
	double Update(double lapStd);

	/// Applies one step of decay for a frame with no usable signal (too dark,
	/// or too few bright pixels). Returns the new smoothed score.
	double Decay();

	/// Returns to the cold-start state: no score, bounds back to their
	/// configured starting values. Used when the video mode changes.
	void Reset();

	// --- observation --------------------------------------------------------

	double SmoothedScore() const { return mSmoothedScore; }
	double ObservedSharp() const { return mObservedSharp; }
	double ObservedBlur() const { return mObservedBlur; }
	const FocusConfig& Config() const { return mConfig; }

private:
	FocusConfig mConfig;
	double mSmoothedScore = 0.0;

	// Per-session adaptive bounds. Sharp grows when exceeded, blur shrinks
	// when undershot.
	double mObservedSharp;
	double mObservedBlur;
};

}  // namespace focus
