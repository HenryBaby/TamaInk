#pragma once
#include <cstdint>

namespace tamaink::wake {
struct Coordinator {
  bool pending = false;
  bool requested = false;
  bool eligible(bool emulatorActive, bool persistenceReady, bool idle, bool automatic, bool awaitReset) const {
    return !pending && emulatorActive && persistenceReady && idle && !automatic && !awaitReset;
  }
  bool request(bool emulatorActive, bool persistenceReady, bool idle, bool automatic, bool awaitReset) {
    if (!eligible(emulatorActive, persistenceReady, idle, automatic, awaitReset)) return false;
    pending = true; requested = true; return true;
  }
  bool mutationAllowed(bool isInternalAdvance) const { return isInternalAdvance || !pending; }
  bool saveSucceeded() { if (!pending) return false; pending = false; return true; }
  void saveFailed() { pending = false; }
  bool armFailed() { pending = false; return true; }
};
}
