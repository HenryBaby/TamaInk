#pragma once
#include <cstdint>
namespace tamaink::autosave {
enum class Action { None, Begin, Advance };
struct Scheduler { std::uint32_t next=0; bool armed=false; bool immediate=false; };
inline bool due(const Scheduler& s, std::uint32_t now) { return s.immediate || (s.armed && static_cast<std::int32_t>(now - s.next) >= 0); }
inline void success(Scheduler& s, std::uint32_t now, std::uint32_t interval) { s.next = now + interval; s.armed = true; s.immediate = false; }
inline void failure(Scheduler& s, std::uint32_t now, std::uint32_t backoff) { s.next = now + backoff; s.armed = true; s.immediate = false; }
inline void request(Scheduler& s) { s.immediate = true; }
struct Controller {
  Scheduler scheduler{}; bool automatic=false;
  Action tick(std::uint32_t now, bool idle, bool awaitReset) {
    if (awaitReset || automatic || !idle || !due(scheduler, now)) return Action::None;
    automatic = true; return Action::Begin;
  }
  Action advance(bool idle) { return automatic && !idle ? Action::Advance : Action::None; }
  bool manualAllowed(bool awaitReset) const { return !awaitReset && !automatic; }
  void beginFailure(std::uint32_t now, std::uint32_t backoff) { automatic=false; failure(scheduler, now, backoff); }
  void writeFailure(std::uint32_t now, std::uint32_t backoff) { beginFailure(now, backoff); }
  void commitRereadFailure(std::uint32_t now, std::uint32_t backoff) { beginFailure(now, backoff); }
  void successCommit(std::uint32_t now, std::uint32_t interval) { automatic=false; success(scheduler, now, interval); }
  void cleanup(std::uint32_t now, std::uint32_t interval) { automatic=false; success(scheduler, now, interval); }
  void arm(std::uint32_t now, std::uint32_t interval) { automatic=false; success(scheduler, now, interval); }
};
}
