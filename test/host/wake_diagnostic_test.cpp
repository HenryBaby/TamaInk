#include <cassert>
#include "tamaink_wake_diagnostic.h"
int main() {
  tamaink::wake::Coordinator c;
  assert(c.request(true,true,true,false,false));
  assert(!c.request(true,true,true,false,false));
  assert(!c.mutationAllowed(false));
  assert(c.mutationAllowed(true));
  assert(c.saveSucceeded());
  assert(!c.saveSucceeded());
  assert(c.request(true,true,true,false,false)); c.saveFailed();
  assert(c.request(true,true,true,false,false)); c.armFailed();
  return 0;
}
