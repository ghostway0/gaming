#include <map>
#include <unordered_map>
#include <queue>
#include <functional>
#include <any>
#include <vector>
#include <typeindex>

#include <absl/time/time.h>
#include <absl/time/clock.h>

class EventQueue {
  using Handler = std::function<void(const std::any&)>;

  struct QueuedEvent {
    std::type_index type;
    std::any data;
  };

  struct DelayedEvent {
    absl::Time trigger;
    std::type_index type;
    std::any data;
  };

 public:
  template <typename T>
  void send(const T& event) {
    queue_.push({std::type_index(typeid(T)), event});
  }

  template <typename T>
  void sendDelayed(const T& event, absl::Duration delay) {
    absl::Time trigger = absl::Now() + delay;
    delayed_.emplace(trigger, QueuedEvent{std::type_index(typeid(T)), event});
  }

  template <typename T>
  void subscribe(std::function<void(const T&)> handler) {
    auto wrapper = [handler](const std::any& data) {
      handler(std::any_cast<const T&>(data));
    };
    handlers[std::type_index(typeid(T))].push_back(wrapper);
  }

  void process();

 private:
  std::queue<QueuedEvent> queue_;
  std::multimap<absl::Time, QueuedEvent> delayed_;
  std::unordered_map<std::type_index, std::vector<Handler>> handlers;
};
