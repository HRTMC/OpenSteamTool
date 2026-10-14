#pragma once

#include "dllmain.h"

// Hooks targeting steamui.dll:

namespace Hooks_SteamUI {
    void Install();
    void Uninstall();

    // Diversion 劫持是否就绪（LoadModuleWithPath pattern 是否命中）。
    // 没命中 = 无法把 Steam 重定向到影子模块，此时必须回退原版，
    // 否则 client hook 全打在没人用的副本上，解锁会静默全灭。
    bool IsDiversionRedirectArmed();

    // 首载抢劫（1791）：Steam 不再走 LoadModuleWithPath 拿 steamclient，
    // 而是启动期直接 LoadLibraryExW。diversion 就绪后把 kernel32 的首载也挂上，
    // 抢到就是隔离生效；来晚了（首载已发生）它自然打空，不影响现有流程。
    void InstallFirstLoadRedirect();

    // 我们自己要 Load 原版时（回退）必须短暂 bypass，不然 Load 会被劫回副本。
    void SetFirstLoadRedirectBypass(bool bypass);

    // Queues an appId for removal from the library UI
    void QueueRemoval(AppId_t appId);
    // Cancels a queued removal when the app is added again before the UI drains it.
    void CancelRemoval(AppId_t appId);
}
