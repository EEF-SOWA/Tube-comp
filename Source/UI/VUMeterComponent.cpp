#include "VUMeterComponent.h"

#include <array>
#include <cmath>

namespace
{
    constexpr float needleMinDeg = -48.0f;
    constexpr float needleMaxDeg = 48.0f;
    constexpr float minVu = -20.0f;
    constexpr float maxVu = 3.0f;
    constexpr float maxGainReductionDb = 24.0f;

    struct ScaleMark
    {
        float normalised;
        const char* label;
    };

    const std::array<ScaleMark, 5> vuScale {{
        { 0.0f, "-20" },
        { 10.0f / 23.0f, "-10" },
        { 15.0f / 23.0f, "-5" },
        { 20.0f / 23.0f, "0" },
        { 1.0f, "+3" }
    }};

    const std::array<ScaleMark, 6> gainReductionScale {{
        { 0.0f, "0" },
        { std::log1p (1.0f) / std::log1p (maxGainReductionDb), "1" },
        { std::log1p (2.0f) / std::log1p (maxGainReductionDb), "2" },
        { std::log1p (4.0f) / std::log1p (maxGainReductionDb), "4" },
        { std::log1p (8.0f) / std::log1p (maxGainReductionDb), "8" },
        { std::log1p (20.0f) / std::log1p (maxGainReductionDb), "20" }
    }};
}
VUMeterComponent::VUMeterComponent (juce::String meterLabel) : label (std::move (meterLabel))
{
    // 60 Hz so the needle moves on the display refresh instead of stepping.
    startTimerHz (60);
    setTooltip ("Level meter: 0 VU = -18 dBFS. Gain reduction scale: 0-24 dB.");
}

VUMeterComponent::~VUMeterComponent()
{
    stopTimer();
}

void VUMeterComponent::setDisplayMode (DisplayMode newMode) noexcept
{
    if (displayMode == newMode)
        return;

    displayMode = newMode;
    repaint();
}

void VUMeterComponent::advanceNeedleState (float& position, float& velocity,
                                           float target, float dt) noexcept
{
    dt = juce::jlimit (1.0f / 240.0f, 1.0f / 20.0f, dt);
    constexpr float riseStiffness = 336.0f;
    constexpr float fallStiffness = 252.0f;
    constexpr float damping = 26.0f;
    const float error = target - position;
    const float stiffness = error >= 0.0f ? riseStiffness : fallStiffness;
    velocity += (error * stiffness - velocity * damping) * dt;
    position += velocity * dt;
    if (! std::isfinite (position) || ! std::isfinite (velocity))
    {
        position = target;
        velocity = 0.0f;
        return;
    }
    position = juce::jlimit (-0.03f, 1.03f, position);
    velocity = juce::jlimit (-20.0f, 20.0f, velocity);
}

float VUMeterComponent::levelDbFsToVuNormalised (float levelDbFs) noexcept
{
    // A VU scale describes level relative to the chosen alignment, not dBFS.
    // At the studio alignment used here, -18 dBFS lands exactly at 0 VU.
    const float vu = levelDbFs - zeroVuDbFs;
    return juce::jlimit (0.0f, 1.0f, juce::jmap (vu, minVu, maxVu, 0.0f, 1.0f));
}

float VUMeterComponent::gainReductionDbToNormalised (float gainReductionDb) noexcept
{
    const float bounded = juce::jlimit (0.0f, maxGainReductionDb, gainReductionDb);
    // Dense marks at 0-4 dB make normal vari-mu gain riding legible; higher
    // reductions still fit on the same physical arc without a long dead zone.
    return std::log1p (bounded) / std::log1p (maxGainReductionDb);
}

void VUMeterComponent::timerCallback()
{
    const float valueDb = levelProvider ? levelProvider() : -100.0f;
    const float target01 = displayMode == DisplayMode::level
        ? levelDbFsToVuNormalised (valueDb)
        : gainReductionDbToNormalised (valueDb);

    // Damped spring rather than an exponential follower: a moving-coil VU has
    // mass, so it swings a little past the mark and settles. The integration
    // uses the real frame interval; a fixed 60 Hz step changes the overshoot
    // whenever the host paints at another rate.
    const double nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    float dt = lastPhysicsTimeSeconds > 0.0
        ? static_cast<float> (nowSeconds - lastPhysicsTimeSeconds)
        : 1.0f / 60.0f;
    lastPhysicsTimeSeconds = nowSeconds;
    dt = juce::jlimit (1.0f / 240.0f, 1.0f / 20.0f, dt);

    advanceNeedleState (displayedValue01, needleVelocity, target01, dt);

    // A real movement comes fully to rest at silence. Snapping only inside a
    // sub-pixel window prevents a permanent last-bit flutter.
    if (std::abs (target01 - displayedValue01) < 0.0005f
        && std::abs (needleVelocity) < 0.01f)
    {
        displayedValue01 = target01;
        needleVelocity = 0.0f;
    }

    repaint();
}

void VUMeterComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.5f);
    if (bounds.getWidth() < 12.0f || bounds.getHeight() < 12.0f)
        return;

    const float corner = juce::jmin (9.0f, bounds.getHeight() * 0.12f);

    // Cast shadow. The panel light is upper-left everywhere on this faceplate,
    // so the meter throws its shadow down and to the right.
    for (int i = 0; i < 5; ++i)
    {
        const float t = static_cast<float> (i) / 4.0f;
        g.setColour (juce::Colours::black.withAlpha (0.16f * (1.0f - t)));
        g.fillRoundedRectangle (bounds.translated (1.2f + static_cast<float> (i) * 0.9f,
                                                   2.0f + static_cast<float> (i) * 1.1f)
                                      .expanded (static_cast<float> (i) * 0.7f),
                                corner);
    }

    // Black instrument case. Real VU cans are glossy black bakelite with a
    // lit top edge and a near-black bottom, which is a stronger read than the
    // chrome ring this used to draw.
    {
        juce::ColourGradient bezel (juce::Colour (0xff4b4a46), bounds.getX(), bounds.getY(),
                                    juce::Colour (0xff070707), bounds.getRight(),
                                    bounds.getBottom(), true);
        g.setGradientFill (bezel);
        g.fillRoundedRectangle (bounds, corner);
    }
    g.setColour (juce::Colours::white.withAlpha (0.20f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), corner, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    // Dial window: cream face on top, black mask across the bottom where the
    // movement and pivot screw live. The window is landscape, as on the 670.
    auto face = bounds.reduced (juce::jmax (6.0f, bounds.getWidth() * 0.05f),
                                 juce::jmax (5.0f, bounds.getHeight() * 0.07f));
    auto mask = face.removeFromBottom (face.getHeight() * 0.22f);
    face.removeFromBottom (1.0f);

    {
        juce::ColourGradient dial (juce::Colour (0xfff6efd8), face.getCentreX(),
                                   face.getY() - face.getHeight() * 0.25f,
                                   juce::Colour (0xffe2d6b4), face.getCentreX(),
                                   face.getBottom(), true);
        g.setGradientFill (dial);
        g.fillRoundedRectangle (face, 2.5f);

        // Lamp bloom: a warm wash from the top centre where the bulb sits.
        juce::ColourGradient bloom (juce::Colour (0xffffd98a).withAlpha (0.22f),
                                    face.getCentreX(), face.getY() + face.getHeight() * 0.15f,
                                    juce::Colours::transparentBlack, face.getCentreX(),
                                    face.getBottom(), true);
        g.setGradientFill (bloom);
        g.fillRoundedRectangle (face, 2.5f);

        // Age: a faint brown tide at the edges, worst in the corners.
        g.setColour (juce::Colour (0xff8a7040).withAlpha (0.10f));
        g.drawRoundedRectangle (face.reduced (1.0f), 2.0f, 3.0f);
    }

    g.setColour (juce::Colour (0xff0b0b0a));
    g.fillRect (mask.expanded (0.0f, 0.5f));
    g.setColour (juce::Colour (0xff3a3a35));
    g.drawLine (mask.getX(), mask.getY(), mask.getRight(), mask.getY(), 1.0f);

    // Movement geometry: the needle pivots at the bottom centre of the dial
    // and reaches almost to the glass. The previous pivot sat below the face
    // and the needle was longer than the dial, so it cut across the corners.
    const auto pivot = juce::Point<float> (face.getCentreX(),
                                           face.getBottom() + mask.getHeight() * 0.18f);
    const float needleLength = face.getHeight() * 0.95f;
    const float arcRadius = needleLength * 0.94f;
    const float needleTipRadius = needleLength * 0.90f;

    const auto angleFor = [] (float normalised)
    {
        return juce::degreesToRadians (juce::jmap (normalised, 0.0f, 1.0f,
                                                   needleMinDeg, needleMaxDeg) - 90.0f);
    };

    // Printed arc. A real VU prints a continuous thin arc with tick marks
    // crossing it, not a ring of detached dashes.
    {
        juce::Path arc;
        const int steps = 40;
        for (int i = 0; i <= steps; ++i)
        {
            const float a = angleFor (static_cast<float> (i) / static_cast<float> (steps));
            const auto pt = juce::Point<float> (pivot.x + arcRadius * std::cos (a),
                                                pivot.y + arcRadius * std::sin (a));
            if (i == 0)
                arc.startNewSubPath (pt);
            else
                arc.lineTo (pt);
        }
        g.setColour (juce::Colour (0xff413a2a));
        g.strokePath (arc, juce::PathStrokeType (0.9f));

        // Red overload sector, from 0 VU up.
        juce::Path redArc;
        const float redFrom = displayMode == DisplayMode::level ? 20.0f / 23.0f : 0.62f;
        for (int i = 0; i <= 12; ++i)
        {
            const float t = juce::jmap (static_cast<float> (i), 0.0f, 12.0f, redFrom, 1.0f);
            const float a = angleFor (t);
            const auto pt = juce::Point<float> (pivot.x + arcRadius * std::cos (a),
                                                pivot.y + arcRadius * std::sin (a));
            if (i == 0)
                redArc.startNewSubPath (pt);
            else
                redArc.lineTo (pt);
        }
        g.setColour (juce::Colour (0xffb23a2c));
        g.strokePath (redArc, juce::PathStrokeType (1.6f));
    }

    const auto drawScale = [&] (const auto& marks)
    {
        g.setFont (juce::FontOptions (juce::jmax (7.0f, face.getHeight() * 0.085f),
                                       juce::Font::bold));
        for (const auto& mark : marks)
        {
            const float a = angleFor (mark.normalised);
            const auto outer = juce::Point<float> (pivot.x + needleLength * 0.98f * std::cos (a),
                                                   pivot.y + needleLength * 0.98f * std::sin (a));
            const auto inner = juce::Point<float> (pivot.x + needleLength * 0.86f * std::cos (a),
                                                   pivot.y + needleLength * 0.86f * std::sin (a));
            const bool hot = displayMode == DisplayMode::level
                                 ? mark.normalised >= 20.0f / 23.0f
                                 : mark.normalised >= 0.62f;
            g.setColour (hot ? juce::Colour (0xff9c2f24) : juce::Colour (0xff2e2a22));
            g.drawLine ({ inner, outer }, 1.2f);

            const float labelRadius = needleLength * 0.70f;
            const auto labelCentre = juce::Point<float> (pivot.x + labelRadius * std::cos (a),
                                                          pivot.y + labelRadius * std::sin (a));
            auto labelBounds = juce::Rectangle<float> (labelCentre.x - 12.0f,
                                                        labelCentre.y - 5.0f, 24.0f, 10.0f);
            g.drawFittedText (mark.label, labelBounds.toNearestInt(),
                               juce::Justification::centred, 1);
        }
    };

    if (displayMode == DisplayMode::level)
        drawScale (vuScale);
    else
        drawScale (gainReductionScale);

    // Engraved caption under the arc, clear of the pivot.
    {
        auto labelArea = juce::Rectangle<float> (face.getX() + face.getWidth() * 0.10f,
                                                  face.getY() + face.getHeight() * 0.62f,
                                                  face.getWidth() * 0.80f,
                                                  face.getHeight() * 0.16f);
        g.setFont (juce::FontOptions (juce::jmax (6.5f, face.getHeight() * 0.075f),
                                       juce::Font::italic | juce::Font::bold));
        g.setColour (juce::Colour (0xff56503f));
        g.drawFittedText (label + (displayMode == DisplayMode::level ? "  VU" : "  GR"),
                          labelArea.toNearestInt(), juce::Justification::centred, 1);
    }

    // Needle: a tapered vane, widest at the pivot and ending inside the
    // printed arc. The short tail disappears into the black movement mask.
    {
        const float needleDeg = juce::jmap (displayedValue01, 0.0f, 1.0f,
                                            needleMinDeg, needleMaxDeg);
        const float rad = juce::degreesToRadians (needleDeg - 90.0f);
        const auto direction = juce::Point<float> (std::cos (rad), std::sin (rad));
        const auto normal = juce::Point<float> (-direction.y, direction.x);
        const auto tip = pivot + direction * needleTipRadius;
        const auto tail = pivot - direction * (needleLength * 0.075f);

        juce::Path needlePath;
        needlePath.startNewSubPath (tail + normal * 1.0f);
        needlePath.lineTo (pivot + normal * 2.3f);
        needlePath.lineTo (tip + normal * 0.42f);
        needlePath.lineTo (tip - normal * 0.42f);
        needlePath.lineTo (pivot - normal * 2.3f);
        needlePath.lineTo (tail - normal * 1.0f);
        needlePath.closeSubPath();

        auto shadowPath = needlePath;
        shadowPath.applyTransform (juce::AffineTransform::translation (1.4f, 1.2f));
        g.setColour (juce::Colour (0xff5a5140).withAlpha (0.28f));
        g.fillPath (shadowPath);

        g.setColour (juce::Colour (0xff1c1812));
        g.fillPath (needlePath);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawLine ({ pivot + normal * 1.0f, tip + normal * 0.15f }, 0.55f);
    }

    // Pivot cap sits on the black mask, as on the hardware.
    {
        g.setColour (juce::Colour (0xff050505));
        g.fillEllipse (pivot.x - 5.0f, pivot.y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (0xff4a4a44));
        g.drawEllipse (pivot.x - 4.4f, pivot.y - 4.4f, 8.8f, 8.8f, 0.9f);
        g.setColour (juce::Colours::white.withAlpha (0.20f));
        g.fillEllipse (pivot.x - 2.2f, pivot.y - 2.6f, 2.0f, 1.4f);
    }

    // Glass. A soft sheen down the upper-left plus a narrow reflection band,
    // clipped to the dial window so it never spills onto the bezel.
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (face.expanded (0.0f, 6.0f).toNearestInt());
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.14f), face.getX(),
                                    face.getY(), juce::Colours::transparentWhite,
                                    face.getX() + face.getWidth() * 0.55f,
                                    face.getY() + face.getHeight() * 0.55f, true);
        g.setGradientFill (sheen);
        g.fillRect (face);

        juce::Path band;
        band.addQuadrilateral (face.getX() + face.getWidth() * 0.10f, face.getY(),
                               face.getX() + face.getWidth() * 0.26f, face.getY(),
                               face.getX() + face.getWidth() * 0.42f, face.getBottom(),
                               face.getX() + face.getWidth() * 0.26f, face.getBottom());
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.fillPath (band);
    }
}
