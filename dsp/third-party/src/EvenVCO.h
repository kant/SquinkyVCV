/**
 * This file contains a modified version of EvenVCO.cpp, from the
 * Befaco repo. See LICENSE-dist.txt for full license info.
 */

// Need to make this compile in MS tools for unit tests
#if defined(_MSC_VER)
#define __attribute__(x)
#endif

#pragma warning (push)
#pragma warning ( disable: 4244 )

#include "dsp/minblep.hpp"
#include "dsp/filter.hpp"
#include "AudioMath.h"
#include "ObjectCache.h"


using namespace rack;

/**
#define _EVEN
#define _TRI
#define _SAW
#define _SQ
#define _SIN
*/


/**
 * Before optimization, cpu = 43
 * turn off tri = 42
 * turn off tri and even, 38
 * turn off tri, even, and sin 21
 * turn off tri, even, sin, sqr, saw, 17
 * everything off and no exp, 3
 * even only, no exp or sin 6.7
 * -----------------------------
 * My first version:
 * everything on - 19.6
 * even only - 17.1
 * saw only - 11.4
 *
 * dispatch saw version
 * pitch calc, but no waveform - 6.7
 * saw - 9.7
 * even/sin - 16
 * sin only - 15.9 (let's try putting back std::sin
 */
template <class TBase>
struct EvenVCO : TBase
{
    EvenVCO(struct Module * module);
    EvenVCO();

    enum ParamIds
    {
        OCTAVE_PARAM,
        TUNE_PARAM,
        PWM_PARAM,
        NUM_PARAMS
    };
    enum InputIds
    {
        PITCH1_INPUT,
        PITCH2_INPUT,
        FM_INPUT,
        PWM_INPUT,
        NUM_INPUTS
    };
    enum OutputIds
    {
        TRI_OUTPUT,
        SINE_OUTPUT,
        EVEN_OUTPUT,
        SAW_OUTPUT,
        SQUARE_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds
    {
        NUM_LIGHTS
    };

    float phase = 0.0;
    /** The outputs */
    float tri = 0.0;

    std::shared_ptr<LookupTableParams<float>> sinLookup;
    std::shared_ptr<LookupTableParams<float>> expLookup;

    /** Whether we are past the pulse width already */
    bool halfPhase = false;

    MinBLEP<16> triSquareMinBLEP;
    MinBLEP<16> triMinBLEP;
    MinBLEP<16> sineMinBLEP;
    MinBLEP<16> doubleSawMinBLEP;
    MinBLEP<16> sawMinBLEP;
    MinBLEP<16> squareMinBLEP;

    void step() override;
    void step_even(float deltaPhase);
    void step_saw(float deltaPhase);
    void step_sin(float deltaPhase);
    void step_old();
    void initialize();
    void zeroOutputsExcept(int except);
    int dispatcher = 0;
    int loopCounter = 0;

    float _freq = 0;   // for testing
};

template <class TBase>
inline EvenVCO<TBase>::EvenVCO() : TBase()
{
    initialize();
}

template <class TBase>
inline EvenVCO<TBase>::EvenVCO(struct Module * module) : TBase(module)
{
    initialize();
}


template <class TBase>
inline void EvenVCO<TBase>::initialize()
{
    triSquareMinBLEP.minblep = rack::minblep_16_32;
    triSquareMinBLEP.oversample = 32;
    triMinBLEP.minblep = minblep_16_32;
    triMinBLEP.oversample = 32;
    sineMinBLEP.minblep = minblep_16_32;
    sineMinBLEP.oversample = 32;
    doubleSawMinBLEP.minblep = minblep_16_32;
    doubleSawMinBLEP.oversample = 32;
    sawMinBLEP.minblep = minblep_16_32;
    sawMinBLEP.oversample = 32;
    squareMinBLEP.minblep = minblep_16_32;
    squareMinBLEP.oversample = 32;

    sinLookup = ObjectCache<float>::getSinLookup();
    // get reference to table of 2 ** x
    // TODO: this table gives up at low freq - fix that
    expLookup = ObjectCache<float>::getExp2();
}

template <class TBase>
void EvenVCO<TBase>::zeroOutputsExcept(int except)
{
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        if (i != except) {
            // if we do even, we do sin at same time
            if ((i == SINE_OUTPUT) && (except == EVEN_OUTPUT)) {
            } else {
                TBase::outputs[i].value = 0;
            }
        }
    }
}

template <class TBase>
inline void EvenVCO<TBase>::step_even(float deltaPhase)
{
    float oldPhase = phase;
    phase += deltaPhase;

    if (oldPhase < 0.5 && phase >= 0.5) {
        // printf("doing blep\n");
        float crossing = -(phase - 0.5) / deltaPhase;
        doubleSawMinBLEP.jump(crossing, -2.0);
    }

    // Reset phase if at end of cycle
    if (phase >= 1.0) {
        phase -= 1.0;
        float crossing = -phase / deltaPhase;
        doubleSawMinBLEP.jump(crossing, -2.0);
        //halfPhase = false;
    }


    //sine = -cosf(2*AudioMath::Pi * phase);
    // TODO: phase, amp, etc right?
    // want cosine, but only have sine lookup
    float adjPhase = phase + .25f;
    if (adjPhase >= 1) {
        adjPhase -= 1;
    }
    const float sine = -LookupTable<float>::lookup(*sinLookup, adjPhase, true);


    float doubleSaw = (phase < 0.5) ? (-1.0 + 4.0*phase) : (-1.0 + 4.0*(phase - 0.5));
    doubleSaw += doubleSawMinBLEP.shift();
    const float even = 0.55 * (doubleSaw + 1.27 * sine);

    TBase::outputs[SINE_OUTPUT].value = 5.0*sine;
    TBase::outputs[EVEN_OUTPUT].value = 5.0*even;
}

template <class TBase>
inline void EvenVCO<TBase>::step_saw(float deltaPhase)
{
  //  float oldPhase = phase;
    phase += deltaPhase;

    // Reset phase if at end of cycle
    if (phase >= 1.0) {
        phase -= 1.0;
        float crossing = -phase / deltaPhase;
        sawMinBLEP.jump(crossing, -2.0);

        // saw only doesn't care
        //halfPhase = false;
    }

    float saw = -1.0 + 2.0*phase;
    saw += sawMinBLEP.shift();
    TBase::outputs[SAW_OUTPUT].value = 5.0*saw;
}

template <class TBase>
inline void EvenVCO<TBase>::step_sin(float deltaPhase)
{
    float oldPhase = phase;
    phase += deltaPhase;

    // Reset phase if at end of cycle
    if (phase >= 1.0) {
        phase -= 1.0;
    }

    // want cosine, but only have sine lookup
    float adjPhase = phase + .25f;
    if (adjPhase >= 1) {
        adjPhase -= 1;
    }

    const float sine = -LookupTable<float>::lookup(*sinLookup, adjPhase, true);
    TBase::outputs[SINE_OUTPUT].value = 5.0*sine;
}

template <class TBase>
void EvenVCO<TBase>::step()
{
    if (--loopCounter < 0) {
        loopCounter = 16;

#if 0
        const bool doSaw = TBase::outputs[SAW_OUTPUT].active;
        const bool doEven = TBase::outputs[EVEN_OUTPUT].active;
        const bool doTri = TBase::outputs[TRI_OUTPUT].active;
        const bool doSq = TBase::outputs[SQUARE_OUTPUT].active;
        const bool doSin = TBase::outputs[SINE_OUTPUT].active;
#else
        // TEPLORARY: just for hacking
        const bool doSaw = false;
        const bool doEven = true;
        const bool doTri = false;
        const bool doSq = false;
        const bool doSin = true;
#endif

        if (doSaw && !doEven && !doTri && !doSq && !doSin) {
            dispatcher = SAW_OUTPUT;
            zeroOutputsExcept(SAW_OUTPUT);
        } else if (!doSaw && doEven && !doTri && !doSq) {
            dispatcher = EVEN_OUTPUT;
            zeroOutputsExcept(EVEN_OUTPUT);
        } else if (!doSaw && !doEven && !doTri && !doSq && doSin) {
            dispatcher = SINE_OUTPUT;
            zeroOutputsExcept(SINE_OUTPUT);
        } else {
            assert(false);
        }
    }


    // Compute frequency, pitch is 1V/oct
    float pitch = 1.0 + roundf(TBase::params[OCTAVE_PARAM].value) + TBase::params[TUNE_PARAM].value / 12.0;
    pitch += TBase::inputs[PITCH1_INPUT].value + TBase::inputs[PITCH2_INPUT].value;
    pitch += TBase::inputs[FM_INPUT].value / 4.0;

    // float freq = 261.626 * powf(2.0, pitch);
    // TODO: pass in false
    // TODO: mul by 261?
    _freq = LookupTable<float>::lookup(*expLookup, pitch, true);
    printf("mine: pitch = %f exp = %f\n", pitch, _freq);
    _freq *= 261.626f;
    _freq = clamp(_freq, 0.0f, 20000.0f);
    // printf("pitch = %f, freq = %f\n", pitch, freq);

    // Advance phase
    float deltaPhase = clamp(_freq * TBase::engineGetSampleTime(), 1e-6f, 0.5f);

    /* Idea: just pass in deltaPhase, let everyone to all the calcs themselves
    */
    switch (dispatcher) {
        case SAW_OUTPUT:
            step_saw(deltaPhase);
            break;
        case EVEN_OUTPUT:
            step_even(deltaPhase);
            break;
        case SINE_OUTPUT:
            step_sin(deltaPhase);
            break;
        default:
            assert(false);
    }
}



#if 1
template <class TBase>
void EvenVCO<TBase>::step_old()
{
  //  dispatch = &EvenVCO<TBase>::step_even;
    const bool doEven = TBase::outputs[EVEN_OUTPUT].active;
    const bool doTri = TBase::outputs[TRI_OUTPUT].active;
    const bool doSaw = TBase::outputs[SAW_OUTPUT].active;
    const bool doSq = TBase::outputs[SQUARE_OUTPUT].active;
    const bool doSin = TBase::outputs[SINE_OUTPUT].active;

    // Compute frequency, pitch is 1V/oct
    float pitch = 1.0 + roundf(TBase::params[OCTAVE_PARAM].value) + TBase::params[TUNE_PARAM].value / 12.0;
    pitch += TBase::inputs[PITCH1_INPUT].value + TBase::inputs[PITCH2_INPUT].value;
    pitch += TBase::inputs[FM_INPUT].value / 4.0;


   // float freq = 261.626 * powf(2.0, pitch);
    // TODO: pass in false
    float freq = LookupTable<float>::lookup(*expLookup, pitch, true);
    freq = clamp(freq, 0.0f, 20000.0f);
   // printf("pitch = %f, freq = %f\n", pitch, freq);


    // Advance phase
    float deltaPhase = clamp(freq * TBase::engineGetSampleTime(), 1e-6f, 0.5f);
    float oldPhase = phase;
    phase += deltaPhase;

    if (oldPhase < 0.5 && phase >= 0.5) {
       // printf("doing blep\n");
        float crossing = -(phase - 0.5) / deltaPhase;

        // TODO: can we turn this off?
        triSquareMinBLEP.jump(crossing, 2.0);
        if (doEven) {
            doubleSawMinBLEP.jump(crossing, -2.0);
        }
    }

        // Pulse width
    float pw;
    if (doSq) {
        pw = TBase::params[PWM_PARAM].value + TBase::inputs[PWM_INPUT].value / 5.0;
        const float minPw = 0.05f;
        pw = rescale(clamp(pw, -1.0f, 1.0f), -1.0f, 1.0f, minPw, 1.0f - minPw);

        if (!halfPhase && phase >= pw) {

            float crossing = -(phase - pw) / deltaPhase;
            squareMinBLEP.jump(crossing, 2.0);

            halfPhase = true;
        }
    }

    // Reset phase if at end of cycle
    if (phase >= 1.0) {
        phase -= 1.0;
        float crossing = -phase / deltaPhase;
        triSquareMinBLEP.jump(crossing, -2.0);
        if (doEven) {
            doubleSawMinBLEP.jump(crossing, -2.0);
        }
        if (doSq) {
            squareMinBLEP.jump(crossing, -2.0);
        }
        if (doSaw) {
            sawMinBLEP.jump(crossing, -2.0);
        }
        halfPhase = false;
    }

    // Outputs
    if (doTri) {
        float triSquare = (phase < 0.5) ? -1.0 : 1.0;
        triSquare += triSquareMinBLEP.shift();

        // Integrate square for triangle
        tri += 4.0 * triSquare * freq * TBase::engineGetSampleTime();
        tri *= (1.0 - 40.0 * TBase::engineGetSampleTime());
    }

    float sine = 0;
    float even = 0;
    float saw = 0;
    if (doSin || doEven) {
        //sine = -cosf(2*AudioMath::Pi * phase);
        // TODO: phase, amp, etc right?

        // want cosine, but only have sine lookup
        float adjPhase = phase + .25f;
        if (adjPhase >= 1) {
            adjPhase -= 1;
        }
        sine = -LookupTable<float>::lookup(*sinLookup, adjPhase, true);
    }
    if (doEven) {
        float doubleSaw = (phase < 0.5) ? (-1.0 + 4.0*phase) : (-1.0 + 4.0*(phase - 0.5));
        doubleSaw += doubleSawMinBLEP.shift();
        even = 0.55 * (doubleSaw + 1.27 * sine);
    }
    if (doSaw) {
        saw = -1.0 + 2.0*phase;
        saw += sawMinBLEP.shift();
    }
    if (doSq) {
        float square = (phase < pw) ? -1.0 : 1.0;
        square += squareMinBLEP.shift();
        TBase::outputs[SQUARE_OUTPUT].value = 5.0*square;
    } else {
        TBase::outputs[SQUARE_OUTPUT].value = 0;
    }

    // Set outputs
    TBase::outputs[TRI_OUTPUT].value = doTri ? 5.0*tri : 0;
    TBase::outputs[SINE_OUTPUT].value = 5.0*sine;
    TBase::outputs[EVEN_OUTPUT].value = 5.0*even;
    TBase::outputs[SAW_OUTPUT].value = 5.0*saw;
}
#endif


#pragma warning (pop)