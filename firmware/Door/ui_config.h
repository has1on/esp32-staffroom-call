#pragma once
#include <stdint.h>
// false = 正式连接 Audio；true = 只演示界面，不读 TF、不联网、不发送。
// 第一次只检查触控时可改 true。安装给学生前必须改回 false 并重新烧录。
#ifndef STAFFROOM_UI_DEMO
#define STAFFROOM_UI_DEMO 0
#endif
constexpr bool UI_DEMO_MODE = (STAFFROOM_UI_DEMO != 0);
constexpr uint32_t INPUT_IDLE_MS = 60000;  // 普通输入闲置 60 秒后清空。
constexpr uint32_t SUCCESS_HOLD_MS = 5000; // 完成页显示 5 秒。
// 结果未知时不自动清空，避免同一学生生成第二条召唤。

// 特殊功能：欢迎语。密码页闲置 20 秒自动关闭，免得学生把管理页留在屏幕上。
constexpr uint32_t PASSCODE_IDLE_MS = 20000;
constexpr uint32_t WELCOME_SLIDE_MS = 2600;  // 欢迎语从右侧滑入所需时间。
constexpr uint32_t WELCOME_TOTAL_MS = 14000; // 欢迎页总显示时间，之后自动返回。
constexpr uint32_t WELCOME_FRAME_MS = 70;    // 滑入期间的重绘间隔（约 14 fps）。
