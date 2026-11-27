#include "prx/libSceRudp/include/Api.hpp"
#include "SceTypes.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {
void Require(bool condition) {
    if (!condition) std::abort();
}
std::uint32_t Read32LE(const unsigned char* data) {
    std::uint32_t v = 0;
    std::memcpy(&v, data, sizeof(v));
    return v;
}
void APS5_VABI Handler(int, int, int, void*) {}
} // namespace

int main() {
    constexpr int notInitialized = static_cast<int>(0x80770001);
    constexpr int invalidContextId = static_cast<int>(0x80770003);
    constexpr std::size_t statusSize = 0x90;

    alignas(0x400) std::array<unsigned char, 0x1000> pool{};
    pool.fill(0x5a);

    std::array<unsigned char, statusSize> status{};
    status.fill(0x5a);
    Require(sceRudpGetStatus(status.data(), statusSize) == notInitialized);

    Require(sceRudpInit_nid_postfix(pool.data() + 8, 0xC58) == 0);

    status.fill(0x5a);
    Require(sceRudpGetStatus(status.data(), statusSize) == 0);
    // debt: sceRudpInit sets total/available contexts = 1 and total/available sockets = 64
    Require(Read32LE(status.data() + 0x40) == 1);
    Require(Read32LE(status.data() + 0x44) == 1);
    Require(Read32LE(status.data() + 0x48) == 64);
    Require(Read32LE(status.data() + 0x4c) == 64);
    // used/peak/max mallocs = 0/0/0x4000 and current/peak/total memory = 0/0/0 on a clean init
    Require(Read32LE(status.data() + 0x50) == 0);
    Require(Read32LE(status.data() + 0x54) == 0);
    Require(Read32LE(status.data() + 0x58) == 0x4000);
    Require(Read32LE(status.data() + 0x68) == 0);
    Require(Read32LE(status.data() + 0x6c) == 0);
    Require(Read32LE(status.data() + 0x70) == 0);

    // sceRudpCreateContext takes a context and four sockets and sceRudpEnd frees it
    int ctx = -1;
    Require(sceRudpCreateContext(Handler, nullptr, &ctx) == 0);
    status.fill(0);
    Require(sceRudpGetStatus(status.data(), statusSize) == 0);
    Require(Read32LE(status.data() + 0x40) == 1);
    Require(Read32LE(status.data() + 0x44) == 0);
    Require(Read32LE(status.data() + 0x48) == 64);
    Require(Read32LE(status.data() + 0x4c) == 60);

    // sceRudpGetContextStatus mirrors +0x40/+0x44 and the fixed four-socket counts
    std::array<unsigned char, 0x58> ctxStatus{};
    ctxStatus.fill(0x5a);
    Require(sceRudpGetContextStatus(ctx, ctxStatus.data(), ctxStatus.size()) == 0);
    Require(Read32LE(ctxStatus.data() + 0x40) == 1);
    Require(Read32LE(ctxStatus.data() + 0x44) == 0);
    Require(Read32LE(ctxStatus.data() + 0x48) == 4);
    Require(Read32LE(ctxStatus.data() + 0x4c) == 4);

    // sceRudpNetAllocate/sceRudpNetFree move the socket totals and heap counters
    void* alloc = nullptr;
    Require(sceRudpNetAllocate(ctx, 0x58, &alloc) == 0);
    Require(alloc != nullptr);
    status.fill(0);
    Require(sceRudpGetStatus(status.data(), statusSize) == 0);
    Require(Read32LE(status.data() + 0x50) == 1);
    Require(Read32LE(status.data() + 0x54) == 1);
    Require(Read32LE(status.data() + 0x68) == 0x58);
    Require(Read32LE(status.data() + 0x6c) == 0x58);
    Require(Read32LE(status.data() + 0x70) == 0x58);

    Require(sceRudpNetFree(ctx, alloc) == 0);
    status.fill(0);
    Require(sceRudpGetStatus(status.data(), statusSize) == 0);
    Require(Read32LE(status.data() + 0x50) == 0);
    Require(Read32LE(status.data() + 0x54) == 1);
    Require(Read32LE(status.data() + 0x68) == 0);
    Require(Read32LE(status.data() + 0x6c) == 0x58);
    Require(Read32LE(status.data() + 0x70) == 0x58);

    void* alloc2 = nullptr;
    Require(sceRudpNetAllocate(ctx, 0x58, &alloc2) == 0);
    status.fill(0);
    Require(sceRudpGetStatus(status.data(), statusSize) == 0);
    Require(Read32LE(status.data() + 0x50) == 1);
    Require(Read32LE(status.data() + 0x54) == 2);
    Require(Read32LE(status.data() + 0x68) == 0x58);
    Require(Read32LE(status.data() + 0x6c) == 0x58);
    Require(Read32LE(status.data() + 0x70) == 0x58);
    Require(sceRudpNetFree(ctx, alloc2) == 0);

    // sceRudpEnd drops the live counts back to 0
    Require(sceRudpTerminate(ctx) == 0);
    Require(sceRudpEnd() == 0);
    Require(sceRudpGetStatus(status.data(), statusSize) == notInitialized);
    return 0;
}
