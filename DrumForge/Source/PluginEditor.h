#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class ModernRotaryLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float pos, float startAngle, float endAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = startAngle + pos * (endAngle - startAngle);
        auto lineW = 3.5f;
        auto arcR = radius - lineW * 0.5f;
        auto c = bounds.getCentre();

        g.setColour (juce::Colour (0xff090b10));
        g.fillEllipse (bounds.reduced (3.0f));
        g.setColour (juce::Colour (0xff1e232f));
        g.drawEllipse (bounds.reduced (3.0f), 1.2f);

        juce::Path bg;
        bg.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff181b24));
        g.strokePath (bg, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, toAngle, true);
        g.setColour (fill);
        g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path p;
        p.addRoundedRectangle (-lineW * 0.5f, -radius + 4.0f, lineW, radius * 0.45f, 1.5f);
        p.applyTransform (juce::AffineTransform::rotation (toAngle).translated (c.x, c.y));
        g.fillPath (p);
    }
};

class DrumForgeAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DrumForgeAudioProcessorEditor (DrumForgeAudioProcessor&);
    ~DrumForgeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }

    DrumForgeAudioProcessor& audioProcessor;
    ModernRotaryLookAndFeel modernLnF;

    juce::ComboBox modeBox, layerTypeBox, clipModeBox;
    juce::ToggleButton deltaBtn { "DELTA" }, soloLayerBtn { "SOLO LAYER" }, bypassBtn { "BYPASS" };
    juce::Slider blendSlider, toneSlider, preDelaySlider;
    juce::Slider attackSlider, sustainSlider, subBodySlider, mudCutSlider;
    juce::Slider satSlider, airSlider, outSlider, mixSlider;

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ComboBoxAttachment> modeAtt, layerTypeAtt, clipModeAtt;
    std::unique_ptr<ButtonAttachment> deltaAtt, soloLayerAtt, bypassAtt;
    std::unique_ptr<SliderAttachment> blendAtt, toneAtt, preDelayAtt, attackAtt, sustainAtt,
                                      subBodyAtt, mudCutAtt, satAtt, airAtt, outAtt, mixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumForgeAudioProcessorEditor)
};
