#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../Source/PluginProcessor.h"
#include <cmath>
#include <cstdio>

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("FAIL: %s\n", msg); ++failures; } else std::printf ("ok:   %s\n", msg); } while (0)

static void setParam (DrumForgeAudioProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    constexpr double sr = 48000.0;
    constexpr int bs = 512;

    // 1) Estabilidad con parámetros extremos y ruido: sin NaN/Inf, salida acotada
    {
        DrumForgeAudioProcessor p;
        p.setPlayConfigDetails (2, 2, sr, bs);
        p.prepareToPlay (sr, bs);
        for (auto* id : { "layerBlend", "attack", "sustain", "subBody", "mudCut", "saturation", "airCrisp", "mix" })
            setParam (p, id, 100.0f);
        setParam (p, "output", 12.0f);
        setParam (p, "clipMode", 1.0f);
        juce::Random rnd (1);
        juce::AudioBuffer<float> b (2, bs);
        juce::MidiBuffer midi;
        bool bad = false;
        for (int blk = 0; blk < 200; ++blk)
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i)
                b.setSample (c, i, (i % 200 == 0 ? 1.0f : 0.0f) + rnd.nextFloat() * 0.5f - 0.25f);
            p.processBlock (b, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i)
            { const float s = b.getSample (c, i); if (! std::isfinite (s) || std::abs (s) > 1.0f) bad = true; }
        }
        CHECK (! bad, "salida finita y dentro de [-1,1] con parametros extremos");
    }

    // 2) Mix=0, output=0 dB, sin delta: la salida es la entrada retardada por la latencia
    {
        DrumForgeAudioProcessor p;
        p.setPlayConfigDetails (2, 2, sr, bs);
        p.prepareToPlay (sr, bs);
        setParam (p, "mix", 0.0f);
        setParam (p, "output", 0.0f);
        const int lat = p.getLatencySamples();
        juce::AudioBuffer<float> b (2, bs);
        juce::MidiBuffer midi;
        double maxErr = 0.0; long n0 = 0;
        auto sig = [] (long n) { return 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 440.0 * (double) n / sr); };
        for (int blk = 0; blk < 60; ++blk)
        {
            for (int i = 0; i < bs; ++i) { b.setSample (0, i, sig (n0 + i)); b.setSample (1, i, sig (n0 + i)); }
            p.processBlock (b, midi);
            if (blk > 20)
                for (int i = 0; i < bs; ++i)
                    { const double e = std::abs ((double) b.getSample (0, i) - (double) sig (n0 + i - lat)); if (e > maxErr) maxErr = e; }
            n0 += bs;
        }
        std::printf ("      latencia=%d muestras, error max=%g\n", lat, maxErr);
        CHECK (lat == 960, "latencia reportada = 20 ms");
        CHECK (maxErr < 1.0e-4, "mix=0 -> senal seca retardada sin alteracion");
    }

    // 3) Bypass: buffer intacto
    {
        DrumForgeAudioProcessor p;
        p.setPlayConfigDetails (2, 2, sr, bs);
        p.prepareToPlay (sr, bs);
        setParam (p, "bypass", 1.0f);
        juce::AudioBuffer<float> b (2, bs), ref (2, bs);
        juce::MidiBuffer midi;
        juce::Random rnd (7);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) { auto v = rnd.nextFloat() - 0.5f; b.setSample (c, i, v); ref.setSample (c, i, v); }
        p.processBlock (b, midi);
        bool same = true;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) if (b.getSample (c, i) != ref.getSample (c, i)) same = false;
        CHECK (same, "bypass no modifica el audio");
    }

    // 4) Un golpe de bombo produce señal y medidores > 0
    {
        DrumForgeAudioProcessor p;
        p.setPlayConfigDetails (2, 2, sr, bs);
        p.prepareToPlay (sr, bs);
        juce::AudioBuffer<float> b (2, bs);
        juce::MidiBuffer midi;
        float peak = 0;
        for (int blk = 0; blk < 40; ++blk)
        {
            b.clear();
            if (blk == 2) for (int i = 0; i < 200; ++i) { auto v = 0.9f * std::exp (-i / 60.0f); b.setSample (0, i, v); b.setSample (1, i, v); }
            p.processBlock (b, midi);
            { const float m = b.getMagnitude (0, bs); if (m > peak) peak = m; }
        }
        CHECK (peak > 0.05f, "el golpe produce salida audible");
        CHECK (p.getPeakIn() >= 0.0f && p.getPeakOut() >= 0.0f, "medidores validos");
    }

    // 5) Estado: guardar/restaurar
    {
        DrumForgeAudioProcessor a, b2;
        setParam (a, "attack", 77.0f);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b2.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::abs (b2.apvts.getRawParameterValue ("attack")->load() - 77.0f) < 0.01f, "save/restore del estado");
    }

    std::printf (failures == 0 ? "\nTODAS LAS PRUEBAS OK\n" : "\n%d PRUEBAS FALLARON\n", failures);
    return failures == 0 ? 0 : 1;
}
