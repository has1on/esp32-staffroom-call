#pragma once
#include <Arduino.h>

enum class UiPage : uint8_t { Input, Teachers, Confirm, Complete, Passcode, Welcome, NetUnlock, Network, NetworkEdit };
enum class UiAction : uint8_t { Key, Clear, Backspace, Next, SelectTeacher,
                                Back, Submit, Query, NextStudent,
                                OpenPasscode, PasscodeKey, PasscodeClear,
                                PasscodeBack, PasscodeSubmit, PasscodeCancel,
                                Dismiss, OpenNetwork, NetKey, NetClear, NetBackspace, NetUnlockSubmit, NetCancel,
                                NetField, NetEditKey, NetEditShift, NetEditSave, NetMenu,
                                NetConnect, NetTest, NetDiscover, NetPairJoin, NetPairSend, NetSpeak,
                                NetJoinAudioAp };
struct Hit {
    int x, y, w, h;
    UiAction action;
    String value;
};
// UI snapshot: no HTTP, SD or hardware dependency, so the same renderer can be
// exercised on a computer without pretending a real ESP32 has been tested.
struct ViewState {
    UiPage page = UiPage::Input;
    bool demo = false, ready = false, wifi = false, frozen = false;
    bool canRetry = true, displayWarning = false;
    String blockingTitle, blockingHelp, notice;
    String studentId, studentClass, maskedName, selectedTeacher, completeState;
    String teacherNames[15];
    size_t teacherCount = 0;
    // Special "welcome banner" feature. passcode only ever reaches the screen as
    // asterisks of the same length, so the typed digits are never rendered.
    String passcode;
    String bannerText;
    int bannerX = 1024;
    String ownIp, ownMac, networkSsid, audioHost, matrixHost, netStatus;
    // Local delta: the Audio hotspot name, so drawNetwork can label its join
    // button without pulling NetworkProfile.h into the renderer.
    String apSsid;
    String netPin, netEditCaption, netEditValue, netModeName;
    String netFields[4];
    bool netSecret=false, netCredentials=false, netPairing=false;
    uint8_t netKeyboard=0;
};
