#pragma once

namespace elysia::builtin
{
enum class StartupLogoAction
{
    None,
    PlayEngineLogo,
    PlayProjectLogo,
    IntroFinished
};

class StartupLogoSequence
{
public:
    void reset(bool has_project_logo) noexcept
    {
        _has_project_logo = has_project_logo;
        _phase = Phase::Ready;
    }

    [[nodiscard]] StartupLogoAction start() noexcept
    {
        if (_phase != Phase::Ready)
            return StartupLogoAction::None;
        _phase = Phase::EngineLogo;
        return StartupLogoAction::PlayEngineLogo;
    }

    [[nodiscard]] StartupLogoAction engine_logo_finished() noexcept
    {
        if (_phase != Phase::EngineLogo)
            return StartupLogoAction::None;
        if (_has_project_logo)
        {
            _phase = Phase::ProjectLogo;
            return StartupLogoAction::PlayProjectLogo;
        }
        _phase = Phase::Finished;
        return StartupLogoAction::IntroFinished;
    }

    [[nodiscard]] StartupLogoAction project_logo_finished() noexcept
    {
        if (_phase != Phase::ProjectLogo)
            return StartupLogoAction::None;
        _phase = Phase::Finished;
        return StartupLogoAction::IntroFinished;
    }

private:
    enum class Phase
    {
        Ready,
        EngineLogo,
        ProjectLogo,
        Finished
    };

    bool _has_project_logo = false;
    Phase _phase = Phase::Ready;
};

enum class StartupLoadingAction
{
    None,
    WaitForConfirmation,
    TransitionToSuccess,
    TransitionToFailure
};

class StartupLoadingCompletion
{
public:
    void reset(
        bool wait_for_confirmation,
        bool wait_for_logo_sequence) noexcept
    {
        _wait_for_confirmation = wait_for_confirmation;
        _wait_for_logo_sequence = wait_for_logo_sequence;
        _loading_finished = false;
        _intro_finished = false;
        _waiting = false;
        _transitioning = false;
    }

    [[nodiscard]] StartupLoadingAction mark_loading_finished() noexcept
    {
        _loading_finished = true;
        return evaluate();
    }

    [[nodiscard]] StartupLoadingAction mark_intro_finished() noexcept
    {
        _intro_finished = true;
        return evaluate();
    }

    [[nodiscard]] StartupLoadingAction confirm() noexcept
    {
        if (!_waiting || _transitioning)
            return StartupLoadingAction::None;
        _waiting = false;
        _transitioning = true;
        return StartupLoadingAction::TransitionToSuccess;
    }

    [[nodiscard]] StartupLoadingAction fail() noexcept
    {
        if (_transitioning)
            return StartupLoadingAction::None;
        _waiting = false;
        _transitioning = true;
        return StartupLoadingAction::TransitionToFailure;
    }

    [[nodiscard]] bool loading_finished() const noexcept
    {
        return _loading_finished;
    }

    [[nodiscard]] bool waiting_for_confirmation() const noexcept
    {
        return _waiting;
    }

    [[nodiscard]] bool transitioning() const noexcept
    {
        return _transitioning;
    }

private:
    [[nodiscard]] StartupLoadingAction evaluate() noexcept
    {
        if (_transitioning || _waiting || !_loading_finished
            || (_wait_for_logo_sequence && !_intro_finished))
        {
            return StartupLoadingAction::None;
        }

        if (_wait_for_confirmation)
        {
            _waiting = true;
            return StartupLoadingAction::WaitForConfirmation;
        }

        _transitioning = true;
        return StartupLoadingAction::TransitionToSuccess;
    }

    bool _wait_for_confirmation = false;
    bool _wait_for_logo_sequence = true;
    bool _loading_finished = false;
    bool _intro_finished = false;
    bool _waiting = false;
    bool _transitioning = false;
};
}
