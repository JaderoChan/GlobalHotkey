#ifndef GLOBAL_HOTKEY_DISABLE_HOOK

#include "keyboard_hook_private.hpp"

namespace gbhk
{

namespace kbhook
{

std::mutex _KeyboardHookPrivate::mtx_;
std::unordered_map<_KeyboardHookPrivate::Combine, std::function<void()>, _KeyboardHookPrivate::Combine::Hash>
_KeyboardHookPrivate::fns_;
void (*_KeyboardHookPrivate::keyPressedCallback_)(int)      = nullptr;
void (*_KeyboardHookPrivate::keyReleasedCallback_)(int)     = nullptr;

_KeyboardHookPrivate::_KeyboardHookPrivate() :
    isRunning_(false)
{}

_KeyboardHookPrivate::~_KeyboardHookPrivate() = default;

int _KeyboardHookPrivate::addKeyListener(int nativeKey, KeyState state, const std::function<void()>& fn)
{
    if (!fn)                                return RC_INVALID_VALUE;
    if (hasKeyListener(nativeKey, state))   return RC_EXIST_SAME_VALUE;

    std::lock_guard<std::mutex> lock(mtx_);
    fns_.insert({ { nativeKey, state }, fn });
    return RC_SUCCESS;
}

int _KeyboardHookPrivate::addKeyListener(int nativeKey, KeyState state, std::function<void()>&& fn)
{
    if (!fn)                                return RC_INVALID_VALUE;
    if (hasKeyListener(nativeKey, state))   return RC_EXIST_SAME_VALUE;

    std::lock_guard<std::mutex> lock(mtx_);
    fns_.insert({ { nativeKey, state }, std::move(fn) });
    return RC_SUCCESS;
}

int _KeyboardHookPrivate::removeKeyListener(int nativeKey, KeyState state)
{
    if (!hasKeyListener(nativeKey, state))
        return RC_NO_SPECIFIED_VALUE;

    std::lock_guard<std::mutex> lock(mtx_);
    fns_.erase({ nativeKey, state });
    return RC_SUCCESS;
}

int _KeyboardHookPrivate::removeAllKeyListener()
{
    std::lock_guard<std::mutex> lock(mtx_);
    fns_.clear();
    return RC_SUCCESS;
}

int _KeyboardHookPrivate::setKeyPressedEvent(void (*fn)(int))
{
    if (fn == nullptr)
        return RC_INVALID_VALUE;

    std::lock_guard<std::mutex> lock(mtx_);
    keyPressedCallback_ = fn;
    return RC_SUCCESS;
}

int _KeyboardHookPrivate::setKeyReleasedEvent(void (*fn)(int))
{
    if (fn == nullptr)
        return RC_INVALID_VALUE;

    std::lock_guard<std::mutex> lock(mtx_);
    keyReleasedCallback_ = fn;
    return RC_SUCCESS;
}

int _KeyboardHookPrivate::unsetKeyPressedEvent()
{
    std::lock_guard<std::mutex> lock(mtx_);
    keyPressedCallback_ = nullptr;
    return RC_SUCCESS;
}

int _KeyboardHookPrivate::unsetKeyReleasedEvent()
{
    std::lock_guard<std::mutex> lock(mtx_);
    keyReleasedCallback_ = nullptr;
    return RC_SUCCESS;
}

bool _KeyboardHookPrivate::hasKeyListener(int nativeKey, KeyState state) const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return fns_.find({ nativeKey, state }) != fns_.end();
}

bool _KeyboardHookPrivate::isRunning() const
{
    return isRunning_;
}

void _KeyboardHookPrivate::resetStaticMember_()
{
    removeAllKeyListener();
    unsetKeyPressedEvent();
    unsetKeyReleasedEvent();
}

} // namespace kbhook

} // namespace gbhk

#endif // !GLOBAL_HOTKEY_DISABLE_HOOK
