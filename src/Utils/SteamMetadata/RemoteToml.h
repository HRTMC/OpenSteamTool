#pragma once
#include <string>

namespace RemoteToml {

    struct Request {
        std::string channel;    // "pattern" or "ipc"
        std::string component;  // "steamclient" or "steamui"
        std::string dllPath;
    };

    struct Result {
        bool        ok        = false;
        bool        fromCache = false;
        std::string body;
        std::string sha256;
    };

    // 优先复用以 SHA 命名的本地缓存，缺失时按序轮询全部远端镜像，全部失败才返回空
    Result Fetch(const Request& request);

    // 删掉该请求对应的缓存文件（内容串台/过期时用），返回是否删掉过文件。
    // 下一次 Fetch 会重新走远端下载。只删文件，不碰目录。
    bool Invalidate(const Request& request);

} // namespace RemoteToml
