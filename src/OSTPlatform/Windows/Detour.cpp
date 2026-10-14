#include "include/Detour.h"

#include "include/Log.h"
#include "include/Thread.h"

#include <windows.h>
#include <detours.h>
#include <tlhelp32.h>

#include <vector>

namespace OSTPlatform::Detour {

namespace {

bool DetourSucceeded(LONG result, const char* operation) {
    if (result == NO_ERROR) return true;
    OSTP_LOG_WARN("{} failed (detours_error={})", operation, result);
    return false;
}

std::vector<HANDLE> s_updatedThreads;  // 登记过的线程句柄，commit 前必须一直有效

// 我把进程里所有线程都登记给 Detours：commit 改目标函数头几个字节时，
// 正好踩在 trampoline 里跑的线程只有登记了才会被它挂起并修正上下文。
// 以前只登记当前线程——启动期装 hook 没事（别的线程还没跑进来），
// 但回退是在 Steam 全速跑流量时二次装卸，网络线程正压在 hook 里，
// commit 直接改它脚下的代码，十几秒后就在野地址上崩了（已实测）。
void UpdateAllThreads() {
    const DWORD self = GetCurrentThreadId();
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    if (Thread32First(snapshot, &te)) {
        do {
            if (te.th32OwnerProcessID != GetCurrentProcessId()) continue;
            if (te.th32ThreadID == self) continue;  // 当前线程后面单独登记
            HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT |
                                  THREAD_QUERY_INFORMATION,
                                  FALSE, te.th32ThreadID);
            if (!h) continue;  // 线程刚好退出了，跳过
            DWORD exitCode = STILL_ACTIVE;
            if (!GetExitCodeThread(h, &exitCode) || exitCode != STILL_ACTIVE) {
                // 快照到打开之间退了的同进程僵尸 TID：登记它一样可能毒化事务，一并拦掉。
                CloseHandle(h);
                continue;
            }
            if (GetProcessIdOfThread(h) != GetCurrentProcessId()) {
                // 快照和打开之间线程退了、TID 被别的进程复用：登记它会让 Detours
                // 报 ACCESS_DENIED 并毒化整个事务（后面 Attach/Commit 全连带失败，
                // hook 装不上），必须在这里拦掉。我线上实测到过一次。
                CloseHandle(h);
                continue;
            }
            // 单个登记失败只跳过它，绝不往外抛——抛一次毒化整个事务。
            if (DetourUpdateThread(h) == NO_ERROR) {
                s_updatedThreads.push_back(h);
            } else {
                CloseHandle(h);
            }
        } while (Thread32Next(snapshot, &te));
    }
    CloseHandle(snapshot);
}

void CloseUpdatedThreads() {
    for (const HANDLE h : s_updatedThreads) CloseHandle(h);
    s_updatedThreads.clear();
}

} // namespace

bool BeginTransaction() {
    const LONG begin = DetourTransactionBegin();
    if (!DetourSucceeded(begin, "DetourTransactionBegin")) return false;
    UpdateAllThreads();
    return DetourSucceeded(
        DetourUpdateThread(Thread::CurrentNativeThreadHandle()),
        "DetourUpdateThread");
}

bool CommitTransaction() {
    const bool ok = DetourSucceeded(DetourTransactionCommit(), "DetourTransactionCommit");
    // 不管 commit 成败，登记时开的句柄都要关掉，不然每次装卸都漏一小把。
    CloseUpdatedThreads();
    return ok;
}

bool Attach(void** target, void* detour) {
    return DetourSucceeded(
        DetourAttach(reinterpret_cast<PVOID*>(target), reinterpret_cast<PVOID>(detour)),
        "DetourAttach");
}

bool Detach(void** target, void* detour) {
    return DetourSucceeded(
        DetourDetach(reinterpret_cast<PVOID*>(target), reinterpret_cast<PVOID>(detour)),
        "DetourDetach");
}

} // namespace OSTPlatform::Detour
