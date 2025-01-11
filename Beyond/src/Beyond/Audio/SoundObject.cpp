#include "pch.h"
#include "SoundObject.h"

namespace Beyond
{
    eastl::string SoundObject::StringFromState(ESoundPlayState state)
    {
        eastl::string str;

        switch (state)
        {
        case ESoundPlayState::Stopped:
            str = "Stopped";
            break;
        case ESoundPlayState::Starting:
            str = "Starting";
            break;
        case ESoundPlayState::Playing:
            str = "Playing";
            break;
        case ESoundPlayState::Pausing:
            str = "Pausing";
            break;
        case ESoundPlayState::Paused:
            str = "Paused";
            break;
        case ESoundPlayState::Stopping:
            str = "Stopping";
            break;
        case ESoundPlayState::FadingOut:
            str = "FadingOut";
            break;
        case ESoundPlayState::FadingIn:
            str = "FadingIn";
            break;
        default:
            break;
        }
        return str;
    }

    SoundObject::SoundObject() = default;
    SoundObject::~SoundObject() = default;

    ISound* SoundObject::SetSound(Scope<ISound>&& sound)
    {
        //BEY_CORE_ASSERT(!m_Sound);
        m_Sound = /*CreateScope<ISound>*/(std::move(sound));
        return m_Sound.get();
    }

} // namespace Beyond
