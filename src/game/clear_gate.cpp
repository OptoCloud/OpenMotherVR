#include "game/clear_gate.h"

#include "game/code_address.h"
#include "game/patcher.h"
#include "game/x86.h"
#include "platform/win32/patching/memory.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <span>
#include <string_view>

namespace omvr::game {

namespace {

constexpr std::string_view kUnexpectedLog = "[stereo] clear gate: unexpected code, reflections stay mono";

// The clear's prologue: `push ebp; mov ebp, esp; sub esp, 0x10`. It does
// not depend on where it runs, so the gate runs a copy of it.
constexpr std::uint8_t kGroup1Imm8To32               = 0x83; // sub/add/... r/m32, imm8 (reg field selects)
constexpr std::uint8_t kModRmSubEsp                  = 0xEC; // 83 /5, esp
constexpr std::uint8_t kModRmEbpEsp                  = 0xEC; // 8B, ebp <- esp
constexpr std::uint8_t kClearFrameSize               = 0x10;
constexpr std::array<std::uint8_t, 6> kClearPrologue = {x86::kPushEbp,   x86::kMovR32Rm32, kModRmEbpEsp,
                                                        kGroup1Imm8To32, kModRmSubEsp,     kClearFrameSize};

// The gate's machine code: room for it, and the displacement of its `jne`
// over the prologue copy and the jump back.
constexpr std::size_t kStubSize     = 64;
constexpr std::uint8_t kSkipRel     = static_cast<std::uint8_t>(kClearPrologue.size() + x86::kJmpRel32Size);
constexpr std::uint8_t kModRmCmpAbs = 0x3D; // 80 /7, [disp32]

// Read by the gate's machine code (`cmp byte ptr [skip_clears], 0`), so it
// must be one byte at a fixed address. Written and read on the render
// thread only (the reflection pass sets it around the game's call that
// clears): relaxed order is enough.
std::atomic<std::uint8_t> skip_clears{0};
static_assert(sizeof(skip_clears) == 1 && std::atomic<std::uint8_t>::is_always_lock_free);

// Appends bytes at a cursor.
class CodeWriter {
public:
    explicit CodeWriter(std::uint8_t* at) noexcept : p_{at} {}
    void put(std::span<const std::uint8_t> bytes) noexcept {
        std::memcpy(p_, bytes.data(), bytes.size());
        p_ += bytes.size();
    }
    void put(std::uint8_t b) noexcept { *p_++ = b; }
    void put_u32(std::uint32_t v) noexcept { put(x86::le32(v)); }
    std::uint8_t* cursor() const noexcept { return p_; }

private:
    std::uint8_t* p_;
};

// cmp byte ptr [skip_clears], 0
// jne skip
// <the clear's prologue>
// jmp clear+6
// skip: ret
void write_gate(std::uint8_t* stub, std::uint8_t* clear) noexcept {
    CodeWriter w{stub};
    w.put(x86::kGroup1Imm8);
    w.put(kModRmCmpAbs);
    w.put_u32(address_of(&skip_clears));
    w.put(0x00);
    w.put(x86::kJneRel8);
    w.put(kSkipRel);
    w.put(kClearPrologue);
    w.put(x86::kJmpRel32);
    // rel32 counts from the end of the jump.
    w.put_u32(address_of(clear + kClearPrologue.size()) - address_of(w.cursor() + x86::kRel32Size));
    w.put(x86::kRet);
}

} // namespace

bool install_clear_gate(std::uint8_t* clear, core::diag::ports::Log& log) noexcept {
    if (!x86::matches(clear, kClearPrologue)) {
        log.line(kUnexpectedLog);
        return false;
    }
    std::uint8_t* stub = platform::win32::allocate_code(kStubSize);
    if (!stub) return false;
    write_gate(stub, clear);
    platform::win32::seal_code(stub, kStubSize); // written once; no page stays writable and executable

    // jmp stub; nop (the sixth byte of the prologue it replaces)
    std::array<std::uint8_t, kClearPrologue.size()> entry = x86::nops<kClearPrologue.size()>();
    entry[0]                                              = x86::kJmpRel32;
    const auto rel = x86::le32(address_of(stub) - address_of(clear + x86::kJmpRel32Size));
    std::copy(rel.begin(), rel.end(), entry.begin() + x86::kCallOperandOffset);
    Patcher patcher{log};
    return patcher.write(clear, entry).has_value();
}

void set_skip_clears(bool skip) noexcept {
    skip_clears.store(static_cast<std::uint8_t>(skip), std::memory_order_relaxed);
}

} // namespace omvr::game
