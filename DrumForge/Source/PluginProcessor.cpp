#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// ---------------- Biquad ----------------
void Biquad::set (double nb0, double nb1, double nb2, double a0, double na1, double na2) noexcept
{
    b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb2 / a0);
    a1 = (float) (na1 / a0); a2 = (float) (na2 / a0);
}

void Biquad::setPeaking (double sr, double f, double q, double gainDb) noexcept
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = juce::MathConstants<double>::twoPi * f / sr;
    const double c = std::cos (w), al = std::sin (w) / (2.0 * q);
    set (1.0 + al * A, -2.0 * c, 1.0 - al * A, 1.0 + al / A, -2.0 * c, 1.0 - al / A);
}

void Biquad::setHighShelf (double sr, double f, double q, double gainDb) noexcept
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = juce::MathConstants<double>::twoPi * f / sr;
    const double c = std::cos (w), al = std::sin (w) / (2.0 * q), t = 2.0 * std::sqrt (A) * al;
    set (A * ((A + 1) + (A - 1) * c + t), -2.0 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t),
         (A + 1) - (A - 1) * c + t, 2.0 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
}

void Biquad::setHighpass (double sr, double f, double q) noexcept
{
    const double w = juce::MathConstants<double>::twoPi * f / sr;
    const double c = std::cos (w), al = std::sin (w) / (2.0 * q);
    set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2.0 * c, 1 - al);
}

// ---------------- Processor ----------------
DrumForgeAudioProcessor::DrumForgeAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    pMode = raw ("mode");           pDelta = raw ("deltaListen"); pSolo = raw ("soloLayer");
    pLayerType = raw ("layerType"); pBlend = raw ("layerBlend");  pTone = raw ("layerTone");
    pPreDelay = raw ("preDelay");   pAttack = raw ("attack");     pSustain = raw ("sustain");
    pSubBody = raw ("subBody");     pMud = raw ("mudCut");        pSat = raw ("saturation");
    pAir = raw ("airCrisp");        pClipMode = raw ("clipMode"); pOutput = raw ("output");
    pMix = raw ("mix");             pBypass = raw ("bypass");
}

juce::AudioProcessorValueTreeState::ParameterLayout DrumForgeAudioProcessor::createParameterLayout()
{
    using F = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<C> ("mode", "Mode", juce::StringArray { "808", "Kick", "Snare", "Hat" }, 1));
    p.push_back (std::make_unique<B> ("deltaListen", "Delta Listen", false));
    p.push_back (std::make_unique<B> ("soloLayer", "Solo Layer", false));
    p.push_back (std::make_unique<C> ("layerType", "Layer Type",
                 juce::StringArray { "Click", "Pre-Snap", "White Noise", "Vinyl Crunch", "Sub Thump" }, 0));
    p.push_back (std::make_unique<F> ("layerBlend", "Layer Blend", 0.0f, 100.0f, 35.0f));
    p.push_back (std::make_unique<F> ("layerTone", "Layer Tone", 0.0f, 100.0f, 50.0f));
    p.push_back (std::make_unique<F> ("preDelay", "Pre-Delay", -20.0f, 20.0f, 0.0f));
    p.push_back (std::make_unique<F> ("attack", "Attack", 0.0f, 100.0f, 40.0f));
    p.push_back (std::make_unique<F> ("sustain", "Sustain", 0.0f, 100.0f, 50.0f));
    p.push_back (std::make_unique<F> ("subBody", "Sub/Body", 0.0f, 100.0f, 30.0f));
    p.push_back (std::make_unique<F> ("mudCut", "Mud Cut", 0.0f, 100.0f, 20.0f));
    p.push_back (std::make_unique<F> ("saturation", "Saturation", 0.0f, 100.0f, 25.0f));
    p.push_back (std::make_unique<F> ("airCrisp", "Air/Crisp", 0.0f, 100.0f, 20.0f));
    p.push_back (std::make_unique<C> ("clipMode", "Clip Mode", juce::StringArray { "Soft", "Hard" }, 0));
    p.push_back (std::make_unique<F> ("output", "Output", -24.0f, 12.0f, 0.0f));
    p.push_back (std::make_unique<F> ("mix", "Mix", 0.0f, 100.0f, 100.0f));
    p.push_back (std::make_unique<B> ("bypass", "Bypass", false));
    return { p.begin(), p.end() };
}

bool DrumForgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void DrumForgeAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    const int size = juce::nextPowerOfTwo ((int) (0.05 * sr) + 8);   // > 40 ms
    delayL.assign ((size_t) size, 0.0f);
    delayR.assign ((size_t) size, 0.0f);
    delayMask = size - 1;
    delayWritePos = 0;

    snapMax = (int) (0.05 * sr);
    scopeDecim = juce::jmax (1, (int) (sr / 1800.0));
    scopeCount = 0;

    envFast = envSlow = tsFastL = tsSlowL = tsFastR = tsSlowR = 0.0f;
    snapActive = false; transientCooldown = 0; subPhase = subEnv = 0.0f;
    for (auto* b : { &mudL, &mudR, &airL, &airR, &modeL, &modeR }) { *b = Biquad(); }

    for (auto* s : { &sBlend, &sTone, &sAttack, &sSustain, &sSub, &sSat, &sMix })
        s->reset (sr, 0.02);
    sGain.reset (sr, 0.02);
    sBlend.setCurrentAndTargetValue (*pBlend * 0.01f);
    sTone.setCurrentAndTargetValue (*pTone * 0.01f);
    sAttack.setCurrentAndTargetValue (*pAttack * 0.01f);
    sSustain.setCurrentAndTargetValue (*pSustain * 0.01f);
    sSub.setCurrentAndTargetValue (*pSubBody * 0.01f);
    sSat.setCurrentAndTargetValue (*pSat * 0.01f);
    sMix.setCurrentAndTargetValue (*pMix * 0.01f);
    sGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    setLatencySamples ((int) std::lround (0.020 * sr));   // lookahead nominal 20 ms
}

void DrumForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || delayMask == 0)
        return;

    if (pBypass->load() > 0.5f)
    {
        peakInMeter = peakOutMeter = clipReductionMeter = 0.0f;
        return;
    }

    const int mode        = (int) pMode->load();
    const bool deltaListen = pDelta->load() > 0.5f;
    const bool soloLayer   = pSolo->load() > 0.5f;
    const int layerType    = (int) pLayerType->load();
    const bool hardClip    = (int) pClipMode->load() == 1;
    const float mudAmt     = pMud->load() * 0.01f;
    const float airAmt     = pAir->load() * 0.01f;

    sBlend.setTargetValue (pBlend->load() * 0.01f);
    sTone.setTargetValue (pTone->load() * 0.01f);
    sAttack.setTargetValue (pAttack->load() * 0.01f);
    sSustain.setTargetValue (pSustain->load() * 0.01f);
    sSub.setTargetValue (pSubBody->load() * 0.01f);
    sSat.setTargetValue (pSat->load() * 0.01f);
    sMix.setTargetValue (pMix->load() * 0.01f);
    sGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    {
        Biquad t; t.setPeaking (sr, 320.0, 1.2, -mudAmt * 12.0);
        for (auto* b : { &mudL, &mudR }) { b->b0 = t.b0; b->b1 = t.b1; b->b2 = t.b2; b->a1 = t.a1; b->a2 = t.a2; }
        Biquad a; a.setHighShelf (sr, mode == 3 ? 9000.0 : 8000.0, 0.707, airAmt * 12.0);
        for (auto* b : { &airL, &airR }) { b->b0 = a.b0; b->b1 = a.b1; b->b2 = a.b2; b->a1 = a.a1; b->a2 = a.a2; }
        Biquad m;
        if (mode == 0)      m.setPeaking (sr, 45.0, 1.4, 3.0);
        else if (mode == 1) m.setPeaking (sr, 75.0, 1.2, 2.5);
        else if (mode == 2) m.setPeaking (sr, 210.0, 1.0, 2.0);
        else                m.setHighpass (sr, 250.0, 0.707);
        for (auto* b : { &modeL, &modeR }) { b->b0 = m.b0; b->b1 = m.b1; b->b2 = m.b2; b->a1 = m.a1; b->a2 = m.a2; }
    }

    const float fastA = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
    const float slowA = 1.0f - std::exp (-1.0f / (0.040f * (float) sr));
    const float tsFastA = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));
    const float tsSlowA = 1.0f - std::exp (-1.0f / (0.050f * (float) sr));
    const float subFreq = mode == 0 ? 42.0f : mode == 1 ? 55.0f : 70.0f;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float subInc = subFreq * twoPi / (float) sr;
    const int delaySamples = (int) (juce::jlimit (0.0f, 40.0f, 20.0f - pPreDelay->load()) * 0.001f * (float) sr);
    const int cooldownLen = (int) (0.075 * sr);
    const int preSnapLen = (int) (0.01 * sr);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    float maxIn = 0, maxOut = 0, maxClip = 0;

    for (int i = 0; i < n; ++i)
    {
        const float blend = sBlend.getNextValue(), tone = sTone.getNextValue();
        const float atk = sAttack.getNextValue(), sus = sSustain.getNextValue();
        const float sub = sSub.getNextValue(), sat = sSat.getNextValue();
        const float mix = sMix.getNextValue(), gain = sGain.getNextValue();

        const float inL = L[i], inR = R[i];
        const float inPeak = std::max (std::abs (inL), std::abs (inR));
        maxIn = std::max (maxIn, inPeak);

        delayL[(size_t) delayWritePos] = inL;
        delayR[(size_t) delayWritePos] = inR;
        const int rIdx = (delayWritePos - delaySamples) & delayMask;
        delayWritePos = (delayWritePos + 1) & delayMask;
        const float dryL = delayL[(size_t) rIdx], dryR = delayR[(size_t) rIdx];

        // Detección de transitorios sobre la señal sin retardo
        envFast += fastA * (inPeak - envFast);
        envSlow += slowA * (inPeak - envSlow);
        if (transientCooldown > 0) --transientCooldown;
        else
        {
            const float diff = envFast - envSlow;
            if (diff > 0.06f && envFast > 0.08f)
            {
                snapActive = true; snapPhase = 0; snapAge = 0;
                snapEnergy = juce::jlimit (0.2f, 1.2f, diff * 3.5f);
                transientCooldown = cooldownLen;
                subEnv = snapEnergy;
            }
        }

        float layer = 0.0f;
        if (snapActive)
        {
            ++snapAge;
            const float age = (float) snapAge / (float) snapMax;
            if (age >= 1.0f) snapActive = false;
            else
            {
                const float dec = (1.0f - age) * (1.0f - age);
                switch (layerType)
                {
                    case 0: snapPhase += (800.0f + (1.0f - age) * 3500.0f) * (0.5f + tone) * twoPi / (float) sr;
                            layer = std::sin (snapPhase) * dec * snapEnergy; break;
                    case 1: snapPhase += (2200.0f + tone * 2000.0f) * twoPi / (float) sr;
                            layer = snapAge < preSnapLen ? std::sin (snapPhase) * (1.0f - age) * snapEnergy : 0.0f; break;
                    case 2: layer = whiteNoise() * dec * snapEnergy * (0.2f + tone * 0.8f); break;
                    case 3: { const float nz = whiteNoise();
                              layer = std::abs (nz) > 0.65f ? (nz > 0 ? 1.0f : -1.0f) * dec * snapEnergy * 0.8f : 0.0f; break; }
                    default: snapPhase += (50.0f + (1.0f - age) * 120.0f) * (0.6f + tone * 0.8f) * twoPi / (float) sr;
                             layer = std::sin (snapPhase) * dec * snapEnergy * 1.3f; break;
                }
                if (snapPhase > 1.0e4f) snapPhase = std::fmod (snapPhase, twoPi);
            }
        }

        // Transient shaper
        tsFastL += tsFastA * (std::abs (dryL) - tsFastL);  tsSlowL += tsSlowA * (std::abs (dryL) - tsSlowL);
        tsFastR += tsFastA * (std::abs (dryR) - tsFastR);  tsSlowR += tsSlowA * (std::abs (dryR) - tsSlowR);
        float procL = dryL * (1.0f + (tsFastL - tsSlowL) * (atk - 0.4f) * 3.0f + tsSlowL * (sus - 0.5f) * 1.8f);
        float procR = dryR * (1.0f + (tsFastR - tsSlowR) * (atk - 0.4f) * 3.0f + tsSlowR * (sus - 0.5f) * 1.8f);

        if (sub > 0.01f && subEnv > 0.001f)
        {
            subPhase += subInc;
            if (subPhase > twoPi) subPhase -= twoPi;
            const float s = std::sin (subPhase) * subEnv * sub * 0.9f;
            subEnv *= 0.9997f;
            procL += s; procR += s;
        }

        if (soloLayer) { procL = procR = layer * blend; }
        else           { procL += layer * blend; procR += layer * blend; }

        procL = modeL.process (procL);  procR = modeR.process (procR);
        if (mudAmt > 0.01f) { procL = mudL.process (procL); procR = mudR.process (procR); }

        if (sat > 0.01f)
        {
            const float drive = 1.0f + sat * 3.5f, norm = 1.0f / (1.0f + sat * 0.7f);
            procL = std::tanh (procL * drive) * norm;
            procR = std::tanh (procR * drive) * norm;
        }

        procL = airL.process (procL);  procR = airR.process (procR);

        float cL, cR;
        if (hardClip) { cL = juce::jlimit (-0.98f, 0.98f, procL); cR = juce::jlimit (-0.98f, 0.98f, procR); }
        else
        {
            auto soft = [] (float x) { const float a = std::abs (x);
                                       return a <= 0.6f ? x : std::copysign (0.6f + 0.4f * std::tanh ((a - 0.6f) / 0.4f), x); };
            cL = soft (procL); cR = soft (procR);
        }
        maxClip = std::max (maxClip, std::max (std::abs (procL) - std::abs (cL), std::abs (procR) - std::abs (cR)));

        float oL, oR;
        if (deltaListen) { oL = (cL - dryL) * gain; oR = (cR - dryR) * gain; }
        else             { oL = (dryL * (1.0f - mix) + cL * mix) * gain; oR = (dryR * (1.0f - mix) + cR * mix) * gain; }

        oL = juce::jlimit (-1.0f, 1.0f, oL);  oR = juce::jlimit (-1.0f, 1.0f, oR);
        L[i] = oL; R[i] = oR;
        maxOut = std::max (maxOut, std::max (std::abs (oL), std::abs (oR)));

        if (++scopeCount >= scopeDecim)
        {
            scopeCount = 0;
            const int w = scopeWrite.load (std::memory_order_relaxed);
            scopeIn[(size_t) w].store (juce::jlimit (-1.0f, 1.0f, inL), std::memory_order_relaxed);
            scopeOut[(size_t) w].store (oL, std::memory_order_relaxed);
            scopeWrite.store ((w + 1) % scopeSize, std::memory_order_relaxed);
        }
    }

    peakInHold = std::max (peakInHold * 0.85f, maxIn);
    peakOutHold = std::max (peakOutHold * 0.85f, maxOut);
    clipHold = std::max (clipHold * 0.75f, maxClip);
    peakInMeter.store (peakInHold);  peakOutMeter.store (peakOutHold);  clipReductionMeter.store (clipHold);
}

void DrumForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DrumForgeAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* DrumForgeAudioProcessor::createEditor()
{
    return new DrumForgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumForgeAudioProcessor();
}
