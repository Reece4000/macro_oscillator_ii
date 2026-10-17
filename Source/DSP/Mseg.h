#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

namespace macro_osc_ii
{
inline constexpr int kMsegSlotCount = 3;
inline constexpr int kMsegMaxPoints = 32;

struct MsegSyncedDuration
{
    float beats {};
    const char* label {};
};

inline constexpr std::array<MsegSyncedDuration, 21> kMsegSyncedDurations {{
    { 1.0f / 16.0f, "1/64" },
    { 1.0f / 12.0f, "1/32T" },
    { 1.0f / 8.0f, "1/32" },
    { 1.0f / 6.0f, "1/16T" },
    { 3.0f / 16.0f, "1/32D" },
    { 1.0f / 4.0f, "1/16" },
    { 1.0f / 3.0f, "1/8T" },
    { 3.0f / 8.0f, "1/16D" },
    { 1.0f / 2.0f, "1/8" },
    { 2.0f / 3.0f, "1/4T" },
    { 3.0f / 4.0f, "1/8D" },
    { 1.0f, "1/4" },
    { 4.0f / 3.0f, "1/2T" },
    { 3.0f / 2.0f, "1/4D" },
    { 2.0f, "1/2" },
    { 3.0f, "1/2D" },
    { 4.0f, "1 bar" },
    { 6.0f, "1 bar D" },
    { 8.0f, "2 bars" },
    { 12.0f, "2 bars D" },
    { 16.0f, "4 bars" }
}};

[[nodiscard]] inline const MsegSyncedDuration& nearestMsegSyncedDuration (float beats) noexcept
{
    const auto clampedBeats = juce::jlimit (kMsegSyncedDurations.front().beats,
                                           kMsegSyncedDurations.back().beats,
                                           beats);
    const MsegSyncedDuration* nearest = &kMsegSyncedDurations.front();
    float nearestDistance = std::abs (clampedBeats - nearest->beats);

    for (const auto& duration : kMsegSyncedDurations)
    {
        const float distance = std::abs (clampedBeats - duration.beats);
        if (distance < nearestDistance)
        {
            nearest = &duration;
            nearestDistance = distance;
        }
    }

    return *nearest;
}

[[nodiscard]] inline float snapMsegSyncedBeats (float beats) noexcept
{
    return nearestMsegSyncedDuration (beats).beats;
}

[[nodiscard]] inline juce::String msegSyncedDurationLabel (float beats)
{
    return nearestMsegSyncedDuration (beats).label;
}

[[nodiscard]] inline float msegSyncedBeatsToSeconds (float beats, float bpm) noexcept
{
    const float safeBpm = juce::jlimit (20.0f, 300.0f, bpm);
    return juce::jlimit (0.01f, 64.0f, snapMsegSyncedBeats (beats) * 60.0f / safeBpm);
}

struct MsegPoint
{
    float x {};
    float y {};
};

struct MsegShape
{
    std::vector<MsegPoint> points;

    [[nodiscard]] float evaluateUnipolar (float phase) const noexcept
    {
        if (points.empty())
            return 0.5f;

        const float x = juce::jlimit (0.0f, 1.0f, phase);
        if (x <= points.front().x)
            return points.front().y;

        for (size_t i = 1; i < points.size(); ++i)
        {
            const auto& right = points[i];
            if (x <= right.x)
            {
                const auto& left = points[i - 1];
                const float width = juce::jmax (0.0001f, right.x - left.x);
                const float alpha = juce::jlimit (0.0f, 1.0f, (x - left.x) / width);
                return left.y + ((right.y - left.y) * alpha);
            }
        }

        return points.back().y;
    }

    [[nodiscard]] float evaluateBipolar (float phase) const noexcept
    {
        return (evaluateUnipolar (phase) * 2.0f) - 1.0f;
    }

    // Each point moved by its own offset (-1..1 on both axes) scaled by amount.
    // Values move by up to half the full range; inner points also move in time,
    // by up to half the gap to their nearer neighbour, so they never cross.
    [[nodiscard]] float evaluateUnipolar (float phase, const std::array<MsegPoint, kMsegMaxPoints>& offsets,
                                          float amount) const noexcept
    {
        if (amount <= 0.0f || points.empty())
            return evaluateUnipolar (phase);

        const auto moved = [&] (size_t i)
        {
            auto point = points[i];
            point.y = juce::jlimit (0.0f, 1.0f, point.y + offsets[i].y * amount * 0.5f);
            if (i > 0 && i + 1 < points.size())
                point.x += offsets[i].x * amount * 0.5f * std::min (point.x - points[i - 1].x, points[i + 1].x - point.x);
            return point;
        };

        const float x = juce::jlimit (0.0f, 1.0f, phase);
        auto left = moved (0);
        if (x <= left.x)
            return left.y;
        for (size_t i = 1; i < points.size(); ++i)
        {
            const auto right = moved (i);
            if (x <= right.x)
            {
                const float width = juce::jmax (0.0001f, right.x - left.x);
                return left.y + (right.y - left.y) * juce::jlimit (0.0f, 1.0f, (x - left.x) / width);
            }
            left = right;
        }
        return left.y;
    }

    [[nodiscard]] static std::vector<MsegPoint> defaultPoints()
    {
        return {
            { 0.0f, 0.5f },
            { 0.25f, 1.0f },
            { 0.65f, 0.1f },
            { 1.0f, 0.5f }
        };
    }

    [[nodiscard]] static std::vector<MsegPoint> sanitize (std::vector<MsegPoint> input)
    {
        if (input.empty())
            input = defaultPoints();

        for (auto& point : input)
        {
            point.x = juce::jlimit (0.0f, 1.0f, point.x);
            point.y = juce::jlimit (0.0f, 1.0f, point.y);
        }

        // A single point is a constant: no movement, just a level (or, with
        // random, a fresh level for every note).
        if (input.size() == 1)
            return { { 0.5f, input.front().y } };

        std::stable_sort (input.begin(), input.end(), [] (const auto& a, const auto& b)
        {
            return a.x < b.x;
        });

        std::vector<MsegPoint> result;
        result.reserve (std::min<size_t> (32, input.size() + 2));

        result.push_back ({ 0.0f, input.front().y });
        for (const auto& point : input)
        {
            if (point.x <= 0.0001f || point.x >= 0.9999f)
                continue;

            if (! result.empty()
                && std::abs (point.x - result.back().x) < 0.000001f
                && std::abs (point.y - result.back().y) < 0.000001f)
                result.back() = point;
            else if (result.size() < static_cast<size_t> (kMsegMaxPoints - 1))
                result.push_back (point);
        }
        result.push_back ({ 1.0f, input.back().y });

        return result;
    }

    [[nodiscard]] static juce::String serialise (const std::vector<MsegPoint>& points)
    {
        juce::StringArray encoded;
        for (const auto& point : sanitize (points))
        {
            encoded.add (juce::String (point.x, 6) + "," + juce::String (point.y, 6));
        }
        return encoded.joinIntoString (";");
    }

    [[nodiscard]] static std::vector<MsegPoint> parse (const juce::String& text)
    {
        if (text.isEmpty())
            return defaultPoints();

        std::vector<MsegPoint> parsed;
        juce::StringArray pairs;
        pairs.addTokens (text, ";", {});

        for (const auto& pair : pairs)
        {
            juce::StringArray values;
            values.addTokens (pair, ",", {});
            if (values.size() != 2)
                continue;

            parsed.push_back ({ values[0].getFloatValue(), values[1].getFloatValue() });
        }

        return sanitize (std::move (parsed));
    }
};

// Preset files store these indices: append new destinations at the end.
enum class MsegDestination : int
{
    off,
    pitch,
    model,
    harmonics,
    timbre,
    morph,
    auxMix,
    level,
    filterFrequency,
    filterQ,
    distortionAmount,
    distortionMix,
    distortionOutput,
    filterType,
    distortionType
};

[[nodiscard]] inline juce::StringArray msegDestinationNames()
{
    return { "Off", "Pitch", "Model", "Harmonics", "Timbre", "Morph", "Aux Mix", "Level",
             "Filter Freq", "Filter Q", "Dist Amount", "Dist Mix", "Dist Output",
             "Filter Type", "Dist Type" };
}
} // namespace macro_osc_ii
