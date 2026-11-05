#include "PluginEditor.h"

DrumForgeAudioProcessorEditor::DrumForgeAudioProcessorEditor (DrumForgeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (800, 600);

    auto rotary = [this] (juce::Slider& s, juce::Colour col, const juce::String& suffix)
    {
        s.setLookAndFeel (&modernLnF);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 16);
        s.setTextValueSuffix (suffix);
        s.setNumDecimalPlacesToDisplay (1);
        s.setColour (juce::Slider::rotarySliderFillColourId, col);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff1c212c));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        addAndMakeVisible (s);
    };

    const juce::Colour orange (0xfff97316), amber (0xfffbbf24);
    rotary (blendSlider, orange, " %");     rotary (toneSlider, orange, " %");  rotary (preDelaySlider, amber, " ms");
    rotary (attackSlider, orange, " %");    rotary (sustainSlider, amber, " %");
    rotary (subBodySlider, juce::Colour (0xffea580c), " %");  rotary (mudCutSlider, juce::Colour (0xffeab308), " %");
    rotary (satSlider, juce::Colour (0xffef4444), " %");      rotary (airSlider, orange, " %");
    rotary (outSlider, orange, " dB");      rotary (mixSlider, amber, " %");

    modeBox.addItemList ({ "808", "Kick", "Snare", "Hat" }, 1);
    layerTypeBox.addItemList ({ "Click", "Pre-Snap", "White Noise", "Vinyl Crunch", "Sub Thump" }, 1);
    clipModeBox.addItemList ({ "Soft Clip", "Hard Clip" }, 1);
    for (auto* c : { &modeBox, &layerTypeBox, &clipModeBox }) addAndMakeVisible (c);
    for (auto* b : { &deltaBtn, &soloLayerBtn, &bypassBtn }) addAndMakeVisible (b);

    auto& v = audioProcessor.apvts;
    modeAtt      = std::make_unique<ComboBoxAttachment> (v, "mode", modeBox);
    deltaAtt     = std::make_unique<ButtonAttachment>   (v, "deltaListen", deltaBtn);
    soloLayerAtt = std::make_unique<ButtonAttachment>   (v, "soloLayer", soloLayerBtn);
    layerTypeAtt = std::make_unique<ComboBoxAttachment> (v, "layerType", layerTypeBox);
    blendAtt     = std::make_unique<SliderAttachment>   (v, "layerBlend", blendSlider);
    toneAtt      = std::make_unique<SliderAttachment>   (v, "layerTone", toneSlider);
    preDelayAtt  = std::make_unique<SliderAttachment>   (v, "preDelay", preDelaySlider);
    attackAtt    = std::make_unique<SliderAttachment>   (v, "attack", attackSlider);
    sustainAtt   = std::make_unique<SliderAttachment>   (v, "sustain", sustainSlider);
    subBodyAtt   = std::make_unique<SliderAttachment>   (v, "subBody", subBodySlider);
    mudCutAtt    = std::make_unique<SliderAttachment>   (v, "mudCut", mudCutSlider);
    satAtt       = std::make_unique<SliderAttachment>   (v, "saturation", satSlider);
    airAtt       = std::make_unique<SliderAttachment>   (v, "airCrisp", airSlider);
    clipModeAtt  = std::make_unique<ComboBoxAttachment> (v, "clipMode", clipModeBox);
    outAtt       = std::make_unique<SliderAttachment>   (v, "output", outSlider);
    mixAtt       = std::make_unique<SliderAttachment>   (v, "mix", mixSlider);
    bypassAtt    = std::make_unique<ButtonAttachment>   (v, "bypass", bypassBtn);

    startTimerHz (30);
}

DrumForgeAudioProcessorEditor::~DrumForgeAudioProcessorEditor()
{
    stopTimer();
    for (auto* s : { &blendSlider, &toneSlider, &preDelaySlider, &attackSlider, &sustainSlider, &subBodySlider,
                     &mudCutSlider, &satSlider, &airSlider, &outSlider, &mixSlider })
        s->setLookAndFeel (nullptr);
}

void DrumForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121214));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff18181c), 0.0f, 0.0f, juce::Colour (0xff121214), 0.0f, 65.0f, false));
    g.fillRect (0, 0, getWidth(), 65);
    g.setColour (juce::Colour (0xff1c202a));
    g.drawHorizontalLine (65, 0.0f, (float) getWidth());

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("DRUMFORGE", 22, 16, 180, 24, juce::Justification::left);
    g.setColour (juce::Colour (0xfff97316));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("TRANSIENT DESIGNER & HARMONIC FORGE", 24, 38, 220, 16, juce::Justification::left);

    auto drawCard = [&g] (float x, float y, float w, float h, const juce::String& title, juce::Colour accent)
    {
        g.setColour (juce::Colour (0xff18181c));
        g.fillRoundedRectangle (x, y, w, h, 8.0f);
        g.setColour (juce::Colour (0xff212530));
        g.drawRoundedRectangle (x, y, w, h, 8.0f, 1.2f);
        g.setColour (accent);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (title, (int) x + 12, (int) y + 8, 150, 20, juce::Justification::left);
        g.setColour (juce::Colour (0xff1c202a));
        g.drawHorizontalLine ((int) y + 30, x, x + w);
    };
    drawCard (20, 240, 240, 310, "SNAP & LAYER", juce::Colour (0xfff97316));
    drawCard (280, 240, 240, 310, "PUNCH & WEIGHT", juce::Colour (0xfffbbf24));
    drawCard (540, 240, 240, 310, "DRIVE & CLIP", juce::Colour (0xffef4444));

    // Osciloscopio / visualizador de transitorios
    const juce::Rectangle<float> scope (20.0f, 75.0f, 760.0f, 100.0f);
    g.setColour (juce::Colour (0xff07080a));
    g.fillRoundedRectangle (scope, 6.0f);
    g.setColour (juce::Colour (0xff212530));
    g.drawRoundedRectangle (scope, 6.0f, 1.0f);
    g.setColour (juce::Colour (0xff15181f));
    g.drawHorizontalLine ((int) scope.getCentreY(), scope.getX() + 4.0f, scope.getRight() - 4.0f);

    auto drawTrace = [&] (bool out, juce::Colour col, float thickness)
    {
        juce::Path path;
        const int w0 = audioProcessor.getScopeWritePos();
        const float sx = (scope.getWidth() - 16.0f) / (float) (DrumForgeAudioProcessor::scopeSize - 1);
        for (int k = 0; k < DrumForgeAudioProcessor::scopeSize; ++k)
        {
            const int idx = (w0 + k) % DrumForgeAudioProcessor::scopeSize;
            const float s = out ? audioProcessor.getScopeOut (idx) : audioProcessor.getScopeIn (idx);
            const float px = scope.getX() + 8.0f + sx * (float) k;
            const float py = scope.getCentreY() - s * (scope.getHeight() * 0.45f);
            if (k == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
        }
        g.setColour (col);
        g.strokePath (path, juce::PathStrokeType (thickness));
    };
    drawTrace (false, juce::Colour (0xff10b981).withAlpha (0.7f), 1.2f);
    drawTrace (true,  juce::Colour (0xfff97316), 1.6f);

    // Medidores
    const float inM = audioProcessor.getPeakIn(), outM = audioProcessor.getPeakOut(), clipM = audioProcessor.getClipReduction();
    g.setColour (juce::Colour (0xff10b981));
    g.fillRect (40.0f, 205.0f, juce::jlimit (0.0f, 180.0f, inM * 180.0f), 6.0f);
    g.setColour (juce::Colour (0xfff97316));
    g.fillRect (280.0f, 205.0f, juce::jlimit (0.0f, 180.0f, outM * 180.0f), 6.0f);
    g.setColour (juce::Colour (0xffef4444));
    g.fillRect (540.0f, 205.0f, juce::jlimit (0.0f, 180.0f, clipM * 250.0f), 6.0f);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("IN PEAK", 40, 190, 80, 14, juce::Justification::left);
    g.drawText ("OUT PEAK", 280, 190, 80, 14, juce::Justification::left);
    g.drawText ("CLIPPER GR", 540, 190, 90, 14, juce::Justification::left);

    // Etiquetas de perillas
    auto label = [&] (juce::Slider& s, const juce::String& t, int dy)
    {
        g.drawText (t, s.getX(), s.getY() + dy, s.getWidth(), 12, juce::Justification::centred);
    };
    label (blendSlider, "BLEND", -12);  label (toneSlider, "TONE", -12);  label (preDelaySlider, "PRE-DELAY", -12);
    label (attackSlider, "ATTACK", -12); label (sustainSlider, "SUSTAIN", -12);
    label (subBodySlider, "SUB/BODY", -12); label (mudCutSlider, "MUD CUT", -12);
    label (satSlider, "SATURATION", -12); label (airSlider, "AIR/CRISP", -12);
    label (outSlider, "OUTPUT", -8);  label (mixSlider, "MIX", -8);
}

void DrumForgeAudioProcessorEditor::resized()
{
    modeBox.setBounds (280, 18, 100, 26);
    deltaBtn.setBounds (400, 20, 75, 24);
    bypassBtn.setBounds (680, 20, 95, 24);

    soloLayerBtn.setBounds (150, 246, 100, 22);
    layerTypeBox.setBounds (32, 280, 216, 24);
    blendSlider.setBounds (30, 340, 70, 75);
    toneSlider.setBounds (105, 340, 70, 75);
    preDelaySlider.setBounds (180, 340, 70, 75);

    attackSlider.setBounds (290, 300, 65, 75);
    sustainSlider.setBounds (365, 300, 65, 75);
    subBodySlider.setBounds (440, 300, 65, 75);
    mudCutSlider.setBounds (365, 410, 75, 80);

    satSlider.setBounds (550, 310, 70, 75);
    airSlider.setBounds (625, 310, 70, 75);
    clipModeBox.setBounds (700, 335, 70, 24);

    outSlider.setBounds (480, 12, 65, 52);
    mixSlider.setBounds (560, 12, 65, 52);
}
