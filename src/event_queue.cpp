#include "sunset/event_queue.h"

void EventQueue::process() {
  absl::Time now = absl::Now();

  for (auto it = delayed_.begin(); it != delayed_.end();) {
    if (it->first <= now) {
      queue_.push(it->second);
      it = delayed_.erase(it);
    } else {
      break;
    }
  }

  while (!queue_.empty()) {
    auto [type, data] = queue_.front();
    queue_.pop();

    auto it = handlers.find(type);
    if (it != handlers.end()) {
      for (auto& handler : it->second) {
        handler(data);
      }
    }
  }
}
