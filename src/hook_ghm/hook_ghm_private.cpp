#ifndef GLOBAL_HOTKEY_DISABLE_HOOK

#include "hook_ghm_private.hpp"

#include <chrono>   // chrono

#include <global_hotkey/keyboard_hook.hpp>
#include <global_hotkey/utility.hpp>

#define NOWTP std::chrono::steady_clock::now()

namespace gbhk
{

std::atomic<Modifiers> _HookGHMPrivate::pressedModi_(0);
std::atomic<Key> _HookGHMPrivate::pressedKey_(0);

_HookGHMPrivate::_HookGHMPrivate() :
    debouncedTime_(_DEFAULT_DEBOUNCED_TIME)
{}

_HookGHMPrivate::~_HookGHMPrivate()
{
    end();
}

int _HookGHMPrivate::start()
{
    if (isRunning_)
        return RC_SUCCESS;

    auto& keyboardHook = kbhook::KeyboardHook::getInstance();
    int ec = keyboardHook.start();
    if (ec != RC_SUCCESS)
        return ec;
    keyboardHook.setKeyPressedEvent(&pressedKeyCallback_);
    keyboardHook.setKeyReleasedEvent(&releasedKeyCallback_);

    isRunning_ = true;
    workThread_ = std::thread([this]()
    {
        workThreadId_ = CUR_TH_ID;
        work_();
        isRunning_ = false;
        cvIsRunning_.notify_all();
    });
    workThread_.detach();
    return RC_SUCCESS;
}

int _HookGHMPrivate::end()
{
    if (!isRunning_)                return RC_SUCCESS;
    if (CUR_TH_ID == workThreadId_) return RC_END_GHM_IN_WRONG_THREAD;

    shouldClose_ = true;

    std::unique_lock<std::mutex> lock(mtx_);
    cvIsRunning_.wait(lock, [this]() { return !isRunning_; });
    lock.unlock();

    shouldClose_ = false;
    pressedModi_ = 0;
    pressedKey_ = 0;
    workThreadId_ = std::thread::id();
    removeAll();

    return RC_SUCCESS;
}

int _HookGHMPrivate::add(const KeyCombination& kc, const std::function<void()>& fn, bool autoRepeat)
{
    if (!kc.isValid() || !fn)       return RC_INVALID_VALUE;
    if (has(kc))                    return RC_EXIST_SAME_VALUE;

    std::lock_guard<std::mutex> lock(mtx_);
    fns_.insert({ kc, { autoRepeat, fn } });
    return RC_SUCCESS;
}

int _HookGHMPrivate::add(const KeyCombination& kc, std::function<void()>&& fn, bool autoRepeat)
{
    if (!kc.isValid() || !fn)       return RC_INVALID_VALUE;
    if (has(kc))                    return RC_EXIST_SAME_VALUE;

    std::lock_guard<std::mutex> lock(mtx_);
    fns_.insert({ kc, { autoRepeat, std::move(fn) } });
    return RC_SUCCESS;
}

int _HookGHMPrivate::remove(const KeyCombination& kc)
{
    if (!has(kc))
        return RC_NO_SPECIFIED_VALUE;

    std::lock_guard<std::mutex> lock(mtx_);
    fns_.erase(kc);
    return RC_SUCCESS;
}

int _HookGHMPrivate::removeAll()
{
    std::lock_guard<std::mutex> lock(mtx_);
    fns_.clear();
    return RC_SUCCESS;
}

int _HookGHMPrivate::replace(const KeyCombination& oldKc, const KeyCombination& newKc)
{
    if (!newKc.isValid())   return RC_INVALID_VALUE;
    if (!has(oldKc))        return RC_NO_SPECIFIED_VALUE;
    if (has(newKc))         return RC_EXIST_SAME_VALUE;

    if (oldKc == newKc)
        return RC_SUCCESS;

    std::lock_guard<std::mutex> lock(mtx_);
    fns_[newKc] = std::move(fns_[oldKc]);
    fns_.erase(oldKc);
    return RC_SUCCESS;
}

int _HookGHMPrivate::setAutoRepeat(const KeyCombination& kc, bool autoRepeat)
{
    if (!has(kc))
        return RC_NO_SPECIFIED_VALUE;

    std::lock_guard<std::mutex> lock(mtx_);
    fns_[kc].first = autoRepeat;
    return RC_SUCCESS;
}

int _HookGHMPrivate::setDebouncedTime(size_t milliseconds)
{
    debouncedTime_ = milliseconds;
    return RC_SUCCESS;
}

void _HookGHMPrivate::work_()
{
    TimedSleeper ts;
    KeyCombination prevKc;
    auto prevExecTime = NOWTP;
    while (!shouldClose_)
    {
        ts.resetStartTime();

        KeyCombination kc(pressedModi_, pressedKey_);

        bool isDebouncing = true;
        bool isInvalidDebouncedTime = debouncedTime_ == 0;
        bool isOnlyModiHasDiff =  kc.modifiers() != prevKc.modifiers() && kc.key() == prevKc.key();
        bool isPrevModiContainsCurModi = prevKc.modifiers().has(kc.modifiers());
        bool isInDebouncedTime = NOWTP - prevExecTime < std::chrono::milliseconds(debouncedTime_);
        if (isInvalidDebouncedTime || !isOnlyModiHasDiff || !isPrevModiContainsCurModi || !isInDebouncedTime)
            isDebouncing = false;

        if (!isDebouncing)
        {
            if (has(kc))
            {
                std::lock_guard<std::mutex> lock(mtx_);
                auto& pair = fns_[kc];
                auto& autoRepeat = pair.first;
                auto& fn = pair.second;
                bool canExec = fn && (kc != prevKc || autoRepeat);
                if (canExec)
                    fn();
            }

            prevKc = kc;
            prevExecTime = NOWTP;
        }

        ts.sleepUntilElapsed(cycleTime_);
    }
}

void _HookGHMPrivate::pressedKeyCallback_(int nativeKey)
{
    auto key = getKeyFromNative(nativeKey);
    if (key == Key_Modi_Meta || key == Key_Modi_Meta_Left || key == Key_Modi_Meta_Right)
        pressedModi_ = pressedModi_.load().add(MODI_META);
    else if (key == Key_Modi_Ctrl || key == Key_Modi_Ctrl_Left || key == Key_Modi_Ctrl_Right)
        pressedModi_ = pressedModi_.load().add(MODI_CTRL);
    else if (key == Key_Modi_Alt || key == Key_Modi_Alt_Left || key == Key_Modi_Alt_Right)
        pressedModi_ = pressedModi_.load().add(MODI_ALT);
    else if (key == Key_Modi_Shift || key == Key_Modi_Shift_Left || key == Key_Modi_Shift_Right)
        pressedModi_ = pressedModi_.load().add(MODI_SHIFT);
    else
        pressedKey_ = key;
}

void _HookGHMPrivate::releasedKeyCallback_(int nativeKey)
{
    auto key = getKeyFromNative(nativeKey);
    if (key == Key_Modi_Meta || key == Key_Modi_Meta_Left || key == Key_Modi_Meta_Right)
        pressedModi_ = pressedModi_.load().remove(MODI_META);
    else if (key == Key_Modi_Ctrl || key == Key_Modi_Ctrl_Left || key == Key_Modi_Ctrl_Right)
        pressedModi_ = pressedModi_.load().remove(MODI_CTRL);
    else if (key == Key_Modi_Alt || key == Key_Modi_Alt_Left || key == Key_Modi_Alt_Right)
        pressedModi_ = pressedModi_.load().remove(MODI_ALT);
    else if (key == Key_Modi_Shift || key == Key_Modi_Shift_Left || key == Key_Modi_Shift_Right)
        pressedModi_ = pressedModi_.load().remove(MODI_SHIFT);
    else
        pressedKey_ = 0;
}

} // namespace gbhk

#endif // !GLOBAL_HOTKEY_DISABLE_HOOK
