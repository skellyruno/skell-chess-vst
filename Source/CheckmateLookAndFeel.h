#pragma once
#include <JuceHeader.h>

// Original, vector-only knob treatment. No bitmaps or allocations in paint.
class CheckmateLookAndFeel final : public juce::LookAndFeel_V4
{
  public:
    void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                          float position, float startAngle, float endAngle,
                          juce::Slider &slider) override
    {
        if (width <= 12 || height <= 12) return;
        const float cx = x + width * .5f;
        const float cy = y + height * .5f;
        const float r = std::max(6.f, std::min(float(width), float(height)) * .5f - 9.f);
        const float angle = startAngle + position * (endAngle - startAngle);
        const auto accent = slider.isEnabled() ? juce::Colour(0xffc9eb85) : juce::Colour(0xff536457);
        const auto ring = juce::Colour(0xff33404b);

        juce::Path backgroundArc, valueArc;
        backgroundArc.addCentredArc(cx, cy, r, r, 0.f, startAngle, endAngle, true);
        valueArc.addCentredArc(cx, cy, r, r, 0.f, startAngle, angle, true);
        g.setColour(ring);
        g.strokePath(backgroundArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        if (position > .0001f)
        {
            g.setColour(accent.withAlpha(.23f));
            g.strokePath(valueArc, juce::PathStrokeType(6.f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
            g.setColour(accent);
            g.strokePath(valueArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        const auto face = juce::Rectangle<float>(cx - r + 6, cy - r + 6,
                                                  (r - 6) * 2, (r - 6) * 2);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff36404a), cx, face.getY(),
                                               juce::Colour(0xff10171f), cx, face.getBottom(), false));
        g.fillEllipse(face);
        g.setColour(juce::Colour(0xff55616d).withAlpha(.55f));
        g.drawEllipse(face.reduced(.5f), 1.f);
        const float inner = r * .27f;
        const float outer = r * .62f;
        juce::Path pointer;
        pointer.startNewSubPath(cx + std::sin(angle) * inner,
                                cy - std::cos(angle) * inner);
        pointer.lineTo(cx + std::sin(angle) * outer,
                       cy - std::cos(angle) * outer);
        g.setColour(juce::Colour(0xff101820).withAlpha(.7f));
        g.strokePath(pointer, juce::PathStrokeType(4.2f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        g.setColour(accent);
        g.strokePath(pointer, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        g.fillEllipse(cx - 2.f, cy - 2.f, 4.f, 4.f);
    }
};
