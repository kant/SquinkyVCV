
/** MinBLEP VCO with subharmonics.
 * based on the Fundamental VCO 1.0
 */

#pragma once

#ifndef _MSC_VER 
#include "simd.h"
#include "dsp/minblep.hpp"
#include "dsp/approx.hpp"
#include "dsp/filter.hpp"

using namespace rack;		// normally I don't like "using", but this is third party code...
extern bool _logvco;

// Accurate only on [0, 1]
template <typename T>
T sin2pi_pade_05_7_6(T x) {
	x -= 0.5f;
	return (T(-6.28319) * x + T(35.353) * simd::pow(x, 3) - T(44.9043) * simd::pow(x, 5) + T(16.0951) * simd::pow(x, 7))
	       / (1 + T(0.953136) * simd::pow(x, 2) + T(0.430238) * simd::pow(x, 4) + T(0.0981408) * simd::pow(x, 6));
}

template <typename T>
T sin2pi_pade_05_5_4(T x) {
	x -= 0.5f;
	return (T(-6.283185307) * x + T(33.19863968) * simd::pow(x, 3) - T(32.44191367) * simd::pow(x, 5))
	       / (1 + T(1.296008659) * simd::pow(x, 2) + T(0.7028072946) * simd::pow(x, 4));
}

// TODO: move out of global scope
template <typename T>
T expCurve(T x) {
	return (3 + x * (-13 + 5 * x)) / (3 + 2 * x);
}

/**
 * T is the sample type (usually float or float_4)
 * I is the integer type (usually int or int32_4)
 */
template <int OVERSAMPLE, int QUALITY, typename T, typename I>
struct VoltageControlledOscillator {


   // void setSubDivisor(int div);
    T sub() {
		//printf("sub() returning %s\n", toStr(subValue).c_str());
		return subValue;
	}


    // Below here is from Fundamental VCO
	bool analog = false;
	bool soft = false;
	bool syncEnabled = false;
	// For optimizing in serial code
	int _channels = 0;

	T lastSyncValue = 0.f;
	T phase = 0.f;
	T subPhase = 0;			// like phase, but for the subharmonic
	T freq;
	T pulseWidth = 0.5f;
	T syncDirection = 1.f;

	rack::dsp::TRCFilter<T> sqrFilter;

	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sqrMinBlep;
	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sawMinBlep;
	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> triMinBlep;
	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sinMinBlep;

	T sqrValue = 0.f;
	T sawValue = 0.f;
	T triValue = 0.f;
	T sinValue = 0.f;
	I subCounter = 1;
	I subDivisionAmount = 100;
	T subValue = 0.f;
	T subFreq;			// freq /subdivamount

	int debugCtr = 0;
	int index=-1;		// just for debugging

	void setupSub(int channels, T pitch, I subDivisor)
	{
	//	printf("\n********* in setup sub index = %d\n", index);
		assert(index >= 0);
		static int printCount = 20;		// turn off printing

		freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30) / 1073741824;
		_channels = channels;
		assert(channels >= 0 && channels <= 4);
		simd_assertGT(subDivisor, int32_4(0));
		simd_assertLE(subDivisor, int32_4(32));
		debugCtr = 0;
		subDivisionAmount = subDivisor;
		subCounter = ifelse( subCounter < 1, 1, subCounter);
		subFreq = freq / subDivisionAmount;
		
#ifndef NDEBUG
		if (printCount < 10) {
			printf(" setSub %d sub=(%s)\n", index, toStr(subDivisionAmount).c_str());
			printf(" freq = %s, subFreq=%s\n", toStr(freq).c_str(), toStr(subFreq).c_str());
			printf(" phase = %s, subPhase=%s\n", toStr(phase).c_str(), toStr(subPhase).c_str());
			printf(" channels = %d\n", channels);
		}
#endif
		
		// Let's keep sub from overflowing by
		// setting freq of unused ones to zero
		for (int i = 0; i < 4; ++i) {
			if (i >= channels) {
				subFreq[i] = 0;
				subPhase[i] = 0;
				if (printCount < 10) {
					printf("set freq to zero on vco %d\n", i);
				}
			}
		}
		if (printCount < 10) {
			printf("**** leaving setupSub\n\n");
		}
		++printCount;
	}

	void setPulseWidth(T pulseWidth) {
		const float pwMin = 0.01f;
		this->pulseWidth = simd::clamp(pulseWidth, pwMin, 1.f - pwMin);
	}

	void process(float deltaTime, T syncValue) {
		// Advance phase
		T deltaPhase = simd::clamp(freq * deltaTime, 1e-6f, 0.35f);
		T deltaSubPhase = simd::clamp(subFreq * deltaTime, 1e-6f, 0.35f);

		if (soft) {
			// Reverse direction
			deltaPhase *= syncDirection;
		}
		else {
			// Reset back to forward
			syncDirection = 1.f;
		}

		phase += deltaPhase;
		// Wrap phase
		phase -= simd::floor(phase);

		// we don't wrap this phase - the sync does it
		subPhase += deltaSubPhase;

		const float overflow = 2;
#ifndef NDEBUG
		if (subPhase[0] > overflow || subPhase[1] > overflow || subPhase[2] > overflow || subPhase[3] > overflow) {
			printf("\nsubPhase overflow sample %d\n", debugCtr);
			printf(" subPhase = %s\n", toStr(subPhase).c_str());
			printf("regular phase = %s\n", toStr(phase).c_str());
			printf(" delta sub = %s\n", toStr(deltaSubPhase).c_str());
			printf(" delta regular = %s\n", toStr(deltaPhase).c_str());

			printf(" div = %s\n", toStr(subDivisionAmount).c_str());
			printf(" ctr = %s\n", toStr(subCounter).c_str());
			printf("channels = %d\n", _channels);
			
			
		//	subPhase = 0;			// ULTRA HACK
		//	printf("ovf forcing to 0\n");
			fflush(stdout);
		//	assert(false);
		}
#endif

		//simd_assertLT(subPhase, T(overflow));
 

		// Jump sqr when crossing 0, or 1 if backwards
		T wrapPhase = (syncDirection == -1.f) & 1.f;
		T wrapCrossing = (wrapPhase - (phase - deltaPhase)) / deltaPhase;
		int wrapMask = simd::movemask((0 < wrapCrossing) & (wrapCrossing <= 1.f));
		if (wrapMask) {
			for (int i = 0; i < _channels; i++) {
				if (wrapMask & (1 << i)) {
					// ok, this VCO here is wrapping
					T mask = simd::movemaskInverse<T>(1 << i);
					float p = wrapCrossing[i] - 1.f;
					T x = mask & (2.f * syncDirection);
					sqrMinBlep.insertDiscontinuity(p, x);
				}
			}
		}
		++debugCtr;

		// Jump sqr when crossing `pulseWidth`
		T pulseCrossing = (pulseWidth - (phase - deltaPhase)) / deltaPhase;
		int pulseMask = simd::movemask((0 < pulseCrossing) & (pulseCrossing <= 1.f));
		if (pulseMask) {
			for (int i = 0; i < _channels; i++) {
				if (pulseMask & (1 << i)) {
					T mask = simd::movemaskInverse<T>(1 << i);
					float p = pulseCrossing[i] - 1.f;
					T x = mask & (-2.f * syncDirection);
					sqrMinBlep.insertDiscontinuity(p, x);
				}
			}
		}

		// Jump saw when crossing 0.5	
		T halfCrossing = (0.5f - (phase - deltaPhase)) / deltaPhase;	
		int halfMask = simd::movemask((0 < halfCrossing) & (halfCrossing <= 1.f));
		if (_logvco) {
			printf("phase=%f, dp=%f hCross = %f hm=%d\n", phase[0], deltaPhase[0], halfCrossing[0], halfMask);
		}
		if (_channels <= 0) {
			printf("channels %d in vco %d\n",_channels, index); fflush(stdout);
			assert(_channels > 0);
		}
		if (halfMask) {
			for (int i = 0; i < _channels; i++) {
				if (_logvco) {
					printf("i=%d, <<=%d and=%d\n", i, 1 << i,  (halfMask & (1 << i)));
				}
				if (halfMask & (1 << i)) {
					T mask = simd::movemaskInverse<T>(1 << i);
					float p = halfCrossing[i] - 1.f;
					T x = mask & (-2.f * syncDirection);
					sawMinBlep.insertDiscontinuity(p, x);
					if (_logvco) {
						printf("** insert disc(%f, %f)\n", p, x[0]);
					}
					assertGT(subCounter[i], 0);
					subCounter[i]--;
					if (subCounter[i] == 0) {
						subCounter[i] = subDivisionAmount[i];
						// printf("sub[%d] rolled over, set counter to %d\n", index, subCounter[i]);
#ifndef NDEBUG
						if (_logvco) {
							printf("subPhase[%d] hit %f, will reset 0 delta = %f\n", i, subPhase[i], deltaSubPhase[i]);
							printf("  delta phase = %f phase = %f\n", deltaPhase[i], phase[i]);
							printf("  sample %d i=%d\n", debugCtr, i);
							printf("  all subPhase: %s\n", toStr(subPhase).c_str());
							printf("  all phase(trigger): %s\n", toStr(subPhase).c_str());
							printf(" all freq = %s\n", toStr(freq).c_str());
							printf(" all sub freq = %s\n", toStr(subFreq).c_str());
							printf(" all deltaPhase = %s\n", toStr(deltaPhase).c_str());
						}
#endif
						subPhase[i] = 0;
#ifndef NDEBUG
						if (_logvco) {
							printf(" leaving reset sub phase %d with subPhase =%s\n", i, toStr(subPhase).c_str());
						}
#endif
					}
				}
			}
		}

		// Detect sync
		// Might be NAN or outside of [0, 1) range
		if (syncEnabled) {
			T deltaSync = syncValue - lastSyncValue;
			T syncCrossing = -lastSyncValue / deltaSync;
			lastSyncValue = syncValue;
			T sync = (0.f < syncCrossing) & (syncCrossing <= 1.f) & (syncValue >= 0.f);
			int syncMask = simd::movemask(sync);
			if (syncMask) {
				if (soft) {
					syncDirection = simd::ifelse(sync, -syncDirection, syncDirection);
				}
				else {
					T newPhase = simd::ifelse(sync, (1.f - syncCrossing) * deltaPhase, phase);
					// Insert minBLEP for sync
					for (int i = 0; i < _channels; i++) {
						if (syncMask & (1 << i)) {
							T mask = simd::movemaskInverse<T>(1 << i);
							float p = syncCrossing[i] - 1.f;
							T x;
							x = mask & (sqr(newPhase) - sqr(phase));
							sqrMinBlep.insertDiscontinuity(p, x);
							x = mask & (saw(newPhase) - saw(phase));
							sawMinBlep.insertDiscontinuity(p, x);
							x = mask & (tri(newPhase) - tri(phase));
							triMinBlep.insertDiscontinuity(p, x);
							x = mask & (sin(newPhase) - sin(phase));
							sinMinBlep.insertDiscontinuity(p, x);
						}
					}
					phase = newPhase;
				}
			}
		}

		// Square
		sqrValue = sqr(phase);
		sqrValue += sqrMinBlep.process();

		if (analog) {
			sqrFilter.setCutoffFreq(20.f * deltaTime);
			sqrFilter.process(sqrValue);
			sqrValue = sqrFilter.highpass() * 0.95f;
		}

		// Saw
		sawValue = saw(phase);
		sawValue += sawMinBlep.process();

		subValue = saw(subPhase);
		//printf("subValue = %s\n", toStr(subValue).c_str());

		// Tri
		triValue = tri(phase);
		triValue += triMinBlep.process();

		// Sin
		sinValue = sin(phase);
		sinValue += sinMinBlep.process();
	}

	T sin(T phase) {
		T v;
		if (analog) {
			// Quadratic approximation of sine, slightly richer harmonics
			T halfPhase = (phase < 0.5f);
			T x = phase - simd::ifelse(halfPhase, 0.25f, 0.75f);
			v = 1.f - 16.f * simd::pow(x, 2);
			v *= simd::ifelse(halfPhase, 1.f, -1.f);
		}
		else {
			v = sin2pi_pade_05_5_4(phase);
			// v = sin2pi_pade_05_7_6(phase);
			// v = simd::sin(2 * T(M_PI) * phase);
		}
		return v;
	}
	T sin() {
		return sinValue;
	}

	T tri(T phase) {
		T v;
		if (analog) {
			T x = phase + 0.25f;
			x -= simd::trunc(x);
			T halfX = (x >= 0.5f);
			x *= 2;
			x -= simd::trunc(x);
			v = expCurve(x) * simd::ifelse(halfX, 1.f, -1.f);
		}
		else {
			v = 1 - 4 * simd::fmin(simd::fabs(phase - 0.25f), simd::fabs(phase - 1.25f));
		}
		return v;
	}
	T tri() {
		return triValue;
	}

	T saw(T phase) {
		T v;
		T x = phase + 0.5f;
		x -= simd::trunc(x);
		if (analog) {
			v = -expCurve(x);
		}
		else {
			v = 2 * x - 1;
		}
		return v;
	}
	T saw() {
		return sawValue;
	}

	T sqr(T phase) {
		T v = simd::ifelse(phase < pulseWidth, 1.f, -1.f);
		return v;
	}
	T sqr() {
		return sqrValue;
	}

	T light() {
		return simd::sin(2 * T(M_PI) * phase);
	}
};

#endif


