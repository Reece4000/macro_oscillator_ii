#include "NativeFilterProcessor.h"

void NativeFilterProcessor::prepare (double sampleRate,
                                     double parameterSmoothingSeconds) noexcept
{
    preparedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (auto& path : paths)
        path.prepare (preparedSampleRate, parameterSmoothingSeconds);

    transition.reset (preparedSampleRate, kIdentityTransitionSeconds);
    transition.setCurrentAndTargetValue (1.0f);
    transitionActive = false;
    queuedRequest.reset();
}

void NativeFilterProcessor::reset() noexcept
{
    for (auto& path : paths)
        path.reset();
    sourcePath = destinationPath;
    transition.setCurrentAndTargetValue (1.0f);
    transitionActive = false;
    queuedRequest.reset();
}

void NativeFilterProcessor::setParameters (bool enabled,
                                           int type,
                                           float frequencyHz,
                                           float q,
                                           float shelfControl,
                                           bool snapImmediately) noexcept
{
    const Identity requested { enabled, juce::jlimit (0, 2, type) };
    if (snapImmediately)
    {
        sourcePath = destinationPath;
        identities[sourcePath] = requested;
        paths[sourcePath].setParameters (requested.enabled,
                                         requested.type,
                                         frequencyHz,
                                         q,
                                         shelfControl,
                                         true);
        transition.setCurrentAndTargetValue (1.0f);
        transitionActive = false;
        queuedRequest.reset();
        return;
    }

    if (identities[destinationPath] != requested)
    {
        if (transitionActive)
        {
            queuedRequest = Request { requested, frequencyHz, q, shelfControl };
            return;
        }

        destinationPath = 1 - sourcePath;
        identities[destinationPath] = requested;
        paths[destinationPath].reset();
        paths[destinationPath].setParameters (requested.enabled,
                                              requested.type,
                                              frequencyHz,
                                              q,
                                              shelfControl,
                                              true);
        transition.reset (preparedSampleRate, kIdentityTransitionSeconds);
        transition.setCurrentAndTargetValue (0.0f);
        transition.setTargetValue (1.0f);
        transitionActive = true;
    }
    else
    {
        paths[destinationPath].setParameters (requested.enabled,
                                              requested.type,
                                              frequencyHz,
                                              q,
                                              shelfControl,
                                              false);
    }
}

void NativeFilterProcessor::processStereo (float& left, float& right) noexcept
{
    if (! transitionActive)
    {
        paths[destinationPath].processStereo (left, right);
        return;
    }

    float sourceLeft = left;
    float sourceRight = right;
    float destinationLeft = left;
    float destinationRight = right;
    paths[sourcePath].processStereo (sourceLeft, sourceRight);
    paths[destinationPath].processStereo (destinationLeft, destinationRight);

    const float mix = transition.getNextValue();
    left = sourceLeft + ((destinationLeft - sourceLeft) * mix);
    right = sourceRight + ((destinationRight - sourceRight) * mix);
    if (! transition.isSmoothing())
    {
        sourcePath = destinationPath;
        transition.setCurrentAndTargetValue (1.0f);
        transitionActive = false;
        if (queuedRequest.has_value())
        {
            const auto next = *queuedRequest;
            queuedRequest.reset();
            setParameters (next.identity.enabled,
                           next.identity.type,
                           next.frequencyHz,
                           next.q,
                           next.shelfControl,
                           false);
        }
    }
}

bool NativeFilterProcessor::isActive() const noexcept
{
    return transitionActive || paths[sourcePath].isActive() || paths[destinationPath].isActive();
}
