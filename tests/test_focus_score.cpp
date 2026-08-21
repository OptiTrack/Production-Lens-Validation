// Unit tests for the platform-independent focus scoring math.
//
// FocusEvaluator measures marker-edge sharpness as the standard deviation of a
// Laplacian over a mask of bright pixels. These tests cover what happens to
// that raw number afterwards: the frame gates that reject unusable frames, the
// adaptive bounds that give the score a scale, the clamping and the smoothing.

#include "core/FocusScore.h"
#include "test_support.h"

using focus::FocusConfig;
using focus::FocusScoreModel;

namespace {

// The midpoint of the default 15..200 bound range, so the raw score is 0.5.
constexpr double kMidRangeLapStd = 107.5;

}  // namespace

// --- frame gating -----------------------------------------------------------

TEST(dark_frames_are_rejected_below_the_brightness_floor) {
	const FocusScoreModel model;

	CHECK_TRUE(model.FrameTooDark(0.0));
	CHECK_TRUE(model.FrameTooDark(29.9));
	CHECK_FALSE(model.FrameTooDark(30.0));  // the floor itself is usable
	CHECK_FALSE(model.FrameTooDark(255.0));
}

TEST(mask_threshold_is_a_fraction_of_the_brightest_pixel) {
	const FocusScoreModel model;

	CHECK_NEAR(model.MaskThreshold(200.0), 100.0, 1e-12);
	CHECK_NEAR(model.MaskThreshold(60.0), 30.0, 1e-12);
}

TEST(frames_with_too_few_bright_pixels_are_rejected) {
	const FocusScoreModel model;

	CHECK_TRUE(model.TooFewMarkerPixels(0));
	CHECK_TRUE(model.TooFewMarkerPixels(2));
	CHECK_FALSE(model.TooFewMarkerPixels(3));
	CHECK_FALSE(model.TooFewMarkerPixels(5000));
}

// --- cold start -------------------------------------------------------------

TEST(model_starts_with_configured_bounds_and_no_score) {
	const FocusScoreModel model;

	CHECK_NEAR(model.ObservedBlur(), 15.0, 1e-12);
	CHECK_NEAR(model.ObservedSharp(), 200.0, 1e-12);
	CHECK_EQ(model.SmoothedScore(), 0.0);
}

// --- scoring ----------------------------------------------------------------

TEST(mid_range_sharpness_scores_half_before_smoothing) {
	FocusScoreModel model;

	// Raw score is 0.5; the first frame only contributes ewmAlpha of it.
	CHECK_NEAR(model.Update(kMidRangeLapStd), 0.1, 1e-12);
	CHECK_NEAR(model.SmoothedScore(), 0.1, 1e-12);
}

TEST(a_steady_scene_converges_on_its_raw_score) {
	FocusScoreModel model;

	for (int i = 0; i < 200; ++i) {
		model.Update(kMidRangeLapStd);
	}

	// Smoothing is a lag, not a ceiling: it settles on the true value.
	CHECK_NEAR(model.SmoothedScore(), 0.5, 1e-6);
}

TEST(sharpness_inside_the_bounds_leaves_them_alone) {
	FocusScoreModel model;

	model.Update(kMidRangeLapStd);

	CHECK_NEAR(model.ObservedBlur(), 15.0, 1e-12);
	CHECK_NEAR(model.ObservedSharp(), 200.0, 1e-12);
}

TEST(a_sharper_frame_widens_the_upper_bound_only_gradually) {
	FocusScoreModel model;

	// 100 above the upper bound moves it by boundsAlpha of the gap, not to the
	// new peak, so one transient frame cannot rescale the whole readout.
	const double score = model.Update(300.0);

	CHECK_NEAR(model.ObservedSharp(), 205.0, 1e-12);
	CHECK_NEAR(model.ObservedBlur(), 15.0, 1e-12);
	CHECK_NEAR(score, 0.2, 1e-12);  // raw score clamped to 1.0, then smoothed
}

TEST(a_blurrier_frame_lowers_the_lower_bound_only_gradually) {
	FocusScoreModel model;

	const double score = model.Update(5.0);

	CHECK_NEAR(model.ObservedBlur(), 14.5, 1e-12);
	CHECK_NEAR(model.ObservedSharp(), 200.0, 1e-12);
	CHECK_EQ(score, 0.0);  // below the blur bound clamps to zero
}

TEST(scores_stay_within_the_unit_range) {
	FocusScoreModel model;

	for (int i = 0; i < 400; ++i) {
		const double score = model.Update(1.0e6);
		CHECK_LE(score, 1.0);
		CHECK_TRUE(score >= 0.0);
	}

	CHECK_NEAR(model.SmoothedScore(), 1.0, 1e-6);
}

TEST(a_narrow_bound_range_cannot_blow_up_the_divide) {
	// Bounds closer together than the floor of 1.0.
	FocusConfig config;
	config.lapStdBlur = 10.0;
	config.lapStdSharp = 10.5;
	FocusScoreModel model(config);

	const double score = model.Update(11.0);

	CHECK_TRUE(score >= 0.0);
	CHECK_LE(score, 1.0);
	CHECK_NEAR(score, 0.2, 1e-12);
}

// --- loss of signal ---------------------------------------------------------

TEST(decay_fades_the_score_when_a_frame_has_no_signal) {
	FocusScoreModel model;
	model.Update(kMidRangeLapStd);  // 0.1

	CHECK_NEAR(model.Decay(), 0.08, 1e-12);
	CHECK_NEAR(model.Decay(), 0.064, 1e-12);
}

TEST(sustained_decay_approaches_zero_without_going_negative) {
	FocusScoreModel model;
	for (int i = 0; i < 200; ++i) {
		model.Update(kMidRangeLapStd);
	}

	for (int i = 0; i < 400; ++i) {
		const double score = model.Decay();
		CHECK_TRUE(score >= 0.0);
	}

	CHECK_LE(model.SmoothedScore(), 1e-9);
}

TEST(decay_leaves_the_learned_bounds_intact) {
	FocusScoreModel model;
	model.Update(300.0);  // widens the upper bound to 205

	model.Decay();

	CHECK_NEAR(model.ObservedSharp(), 205.0, 1e-12);
	CHECK_NEAR(model.ObservedBlur(), 15.0, 1e-12);
}

// --- reset ------------------------------------------------------------------

TEST(reset_returns_the_model_to_a_cold_start) {
	FocusScoreModel model;
	model.Update(300.0);  // moves the upper bound
	model.Update(5.0);    // moves the lower bound
	CHECK_TRUE(model.SmoothedScore() > 0.0);

	model.Reset();

	CHECK_EQ(model.SmoothedScore(), 0.0);
	CHECK_NEAR(model.ObservedSharp(), 200.0, 1e-12);
	CHECK_NEAR(model.ObservedBlur(), 15.0, 1e-12);
}

// --- configuration ----------------------------------------------------------

TEST(smoothing_can_be_disabled_through_the_config) {
	FocusConfig config;
	config.ewmAlpha = 1.0;  // report the raw score with no lag
	FocusScoreModel model(config);

	CHECK_NEAR(model.Update(kMidRangeLapStd), 0.5, 1e-12);

	// With no smoothing there is nothing to fade either.
	CHECK_EQ(model.Decay(), 0.0);
}

TEST(config_is_reported_back_for_the_ui) {
	FocusConfig config;
	config.minImageBrightness = 50.0;
	config.markerThreshRatio = 0.25;
	const FocusScoreModel model(config);

	CHECK_NEAR(model.Config().minImageBrightness, 50.0, 1e-12);
	CHECK_TRUE(model.FrameTooDark(49.0));
	CHECK_NEAR(model.MaskThreshold(200.0), 50.0, 1e-12);
}
