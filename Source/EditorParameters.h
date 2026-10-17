#pragma once

#include "PluginProcessor.h"

// Small parameter helpers shared by the editor's controls.
namespace macro_osc_ii::editor
{
[[nodiscard]] inline juce::RangedAudioParameter* parameterFor (MacroOscIIAudioProcessor& processor, const char* parameterId)
{
    return parameterId != nullptr ? processor.getState().getParameter (parameterId) : nullptr;
}

[[nodiscard]] inline float rawValue (MacroOscIIAudioProcessor& processor, const char* parameterId)
{
    if (const auto* value = processor.getState().getRawParameterValue (parameterId))
        return value->load (std::memory_order_relaxed);
    return 0.0f;
}

inline void setNormalised (MacroOscIIAudioProcessor& processor, const char* parameterId, float normalised)
{
    if (auto* parameter = parameterFor (processor, parameterId))
        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
}

inline void setAsGesture (MacroOscIIAudioProcessor& processor, const char* parameterId, float plainValue)
{
    if (auto* parameter = parameterFor (processor, parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
        parameter->endChangeGesture();
    }
}

inline void resetToDefault (MacroOscIIAudioProcessor& processor, const char* parameterId)
{
    if (auto* parameter = parameterFor (processor, parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getDefaultValue());
        parameter->endChangeGesture();
    }
}

// Model families for the model map, in model order (kPlaitsModels).
struct ModelFamily
{
    const char* name;
    int count;
};

inline constexpr std::array<ModelFamily, 5> modelFamilies { {
    { "synth", 10 }, { "fm", 5 }, { "paraphonic", 3 }, { "physical", 3 }, { "percussive", 3 }
} };
} // namespace macro_osc_ii::editor
