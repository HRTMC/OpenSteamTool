#include "HookManager.h"
#include "Hooks_CallBack.h"
#include "Hooks_Decryption.h"
#include "Hooks_IPC.h"
#include "Hooks_KeyValues.h"
#include "Hooks_Manifest.h"
#include "Hooks_Misc.h"
#include "Hooks_NetPacket.h"
#include "Hooks_Package.h"
#include "Hooks_SteamUI.h"
#include "Utils/HookSupport/VehCommon.h"


namespace SteamUI {

    void CoreHook()   { 
        Hooks_SteamUI::Install(); 
    }
    void CoreUnhook() { 
        Hooks_SteamUI::Uninstall(); 
    }
}

namespace SteamClient {

    // CoreHook 被门禁拦掉的会话（未验证构建）里 hook 一个没装，
    // CoreUnhook 直接返回，免得 Detach 全员失败刷一屏 WARN。
    // fallback 的 Unhook→Hook 照常走（flag 按 true→false→true 翻转）。
    static bool g_clientHooksInstalled = false;

    void CoreHook() {
        // 我在 1791 上实测：client hook 一旦在没核对过偏移的新构建上活过来，
        // NetPacket 解析就会读野指针带走 Steam（四次全崩在 UnpackRaw 读 [rax]）。
        // 门禁以前只拦注入，不管 NetPacket——现在整个 client hook 共用同一份白名单，
        // 白名单外直接不装：Steam 活着、解锁等核对完偏移加白，这是 fail-closed。
        if (!Hooks_Package::IsVerifiedClientBuild()) return;
        Hooks_CallBack::Install();
        Hooks_Decryption::Install();
        Hooks_IPC::Install();
        // Hooks_KeyValues::Install();
        Hooks_Manifest::Install();
        Hooks_Misc::Install();
        Hooks_NetPacket::Install();
        Hooks_Package::Install();
        g_clientHooksInstalled = true;
    }

    void CoreUnhook() {
        if (!g_clientHooksInstalled) return;
        g_clientHooksInstalled = false;
        Hooks_CallBack::Uninstall();
        Hooks_Decryption::Uninstall();
        Hooks_IPC::Uninstall();
        // Hooks_KeyValues::Uninstall();
        Hooks_Manifest::Uninstall();
        Hooks_Misc::Uninstall();
        Hooks_NetPacket::Uninstall();
        Hooks_Package::Uninstall();
        VehCommon::DisarmAll();
        VehCommon::RemoveHandler();
    }
}
