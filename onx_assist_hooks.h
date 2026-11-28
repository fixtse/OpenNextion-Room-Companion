#pragma once
// Assist (microphone, voice assistant, speaker) is optional: the "lite" roots
// leave out features/assist.yaml. Shared panel code reaches Assist only through
// these hooks, which assist.yaml installs at boot; without it they do nothing.
#include <functional>

namespace onx_assist {
inline std::function<bool()> overlay_visible = []() { return false; };
inline std::function<bool()> running = []() { return false; };
inline std::function<void()> toggle = []() {};
inline std::function<void()> stop = []() {};
// Hides the overlay after Assist changed the shown page; a reply keeps playing.
inline std::function<void()> dismiss = []() {};
inline std::function<void(int, int)> resize = [](int, int) {};
// Stops the finished-timer alarm sound (no speaker: nothing to stop).
inline std::function<void()> stop_alarm_sound = []() {};
}  // namespace onx_assist
