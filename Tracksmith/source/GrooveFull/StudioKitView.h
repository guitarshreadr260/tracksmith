#pragma once
#include "PluginProcessor.h"
#include <GrooveArtwork.h>
namespace TracksmithGrooveFull {

class StudioKitView final : public juce::Component
{
public:
    explicit StudioKitView (AvrxGrooveProcessor& processor) : owner (processor)
    { artwork = juce::ImageCache::getFromMemory (TracksmithGrooveArtwork::groovestudiokit_png, TracksmithGrooveArtwork::groovestudiokit_pngSize); setTooltipText(); }
    void selectDrum (int drum) { selected = drum; repaint(); }
    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff151719)); g.fillRoundedRectangle (bounds, 7);
        const auto area = pictureBounds();
        if (artwork.isValid()) g.drawImage (artwork, area, juce::RectanglePlacement::fillDestination);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xbb070809), 0, 0, juce::Colours::transparentBlack, 0, 80, false)); g.fillRect (0, 0, getWidth(), 80);
        g.setColour (juce::Colour (0xffe2d5c0)); g.setFont (11); g.drawText ("ACOUSTIC STUDIO  /  INTERACTIVE KIT", 16, 10, getWidth() - 32, 22, juce::Justification::centredLeft);
        for (int drum = 0; drum < 9; ++drum)
        {
            const auto point = hotspot (drum); const bool active = drum == selected;
            g.setColour (juce::Colour (active ? 0xffd8b87d : 0xaa16191d)); g.fillEllipse (point.x - 12, point.y - 12, 24, 24);
            g.setColour (juce::Colour (active ? 0xfff5e3bc : 0xffaca497)); g.drawEllipse (point.x - 12, point.y - 12, 24, 24, 1);
            g.setColour (active ? juce::Colour (0xff17191c) : juce::Colours::white); g.setFont (10); g.drawText (juce::String (drum + 1), static_cast<int> (point.x - 12), static_cast<int> (point.y - 12), 24, 24, juce::Justification::centred);
        }
        g.setColour (juce::Colour (0xee111315)); g.fillRect (0, getHeight() - 36, getWidth(), 36);
        g.setColour (juce::Colour (0xffd8b87d)); g.setFont (12); g.drawText (GrooveGenerator::names[static_cast<size_t> (selected)], 16, getHeight() - 31, 160, 25, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffb4b3b1)); g.setFont (10); g.drawText ("Click to audition  /  double-click for samples & shaping", 180, getHeight() - 31, getWidth() - 196, 25, juce::Justification::centredRight);
        g.setColour (juce::Colour (0xff44403a)); g.drawRoundedRectangle (bounds.reduced (0.5f), 7, 0.8f);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        int nearest = 0; float distance = 1.0e9f;
        for (int drum = 0; drum < 9; ++drum) { const auto d = hotspot (drum).getDistanceFrom (e.position); if (d < distance) { distance = d; nearest = drum; } }
        selected = nearest; if (onSelect) onSelect (nearest); owner.auditionPad (nearest); repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onEdit) onEdit (selected); }
    std::function<void (int)> onSelect, onEdit;
private:
    juce::Rectangle<float> pictureBounds() const { return { 1, 1, getWidth() - 2.0f, getHeight() - 2.0f }; }
    juce::Point<float> hotspot (int drum) const
    {
        // Positions are measured against the embedded studio artwork.
        constexpr std::array<juce::Point<float>, 9> points { juce::Point<float> { .5f, .67f }, { .28f, .48f }, { .12f, .32f }, { .17f, .38f }, { .42f, .34f }, { .58f, .34f }, { .76f, .55f }, { .26f, .18f }, { .78f, .22f } };
        const auto area = pictureBounds();
        // Fill/crop transform matching RectanglePlacement::fillDestination.
        const float scale = juce::jmax (area.getWidth() / artwork.getWidth(), area.getHeight() / artwork.getHeight());
        const float w = artwork.getWidth() * scale, h = artwork.getHeight() * scale;
        return { area.getCentreX() - w / 2 + points[static_cast<size_t> (drum)].x * w, area.getCentreY() - h / 2 + points[static_cast<size_t> (drum)].y * h };
    }
    void setTooltipText() { setName ("Interactive acoustic drum kit"); }
    AvrxGrooveProcessor& owner;
    juce::Image artwork;
    int selected = 0;
};
}
