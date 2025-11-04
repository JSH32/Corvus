#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <variant>
#include <vector>

namespace Corvus::Core::Events {
template <typename Variant>
class EventConsumer;

template <typename Variant>
class EventProducer : public std::enable_shared_from_this<EventProducer<Variant>> {
public:
    using Callback = std::function<void(const Variant&)>;

    ~EventProducer();

    void subscribe(EventConsumer<Variant>* consumer, Callback cb) {
        subscribers.emplace_back(consumer, std::move(cb));
    }

    void unsubscribe(EventConsumer<Variant>* consumer) {
        subscribers.erase(std::remove_if(subscribers.begin(),
                                         subscribers.end(),
                                         [consumer](auto& p) { return p.first == consumer; }),
                          subscribers.end());
    }

    template <typename T>
    void emit(const T& ev) {
        Variant v = ev;
        for (auto& [consumer, cb] : subscribers)
            cb(v);
    }

    void attachConsumer(EventConsumer<Variant>& consumer);

private:
    std::vector<std::pair<EventConsumer<Variant>*, Callback>> subscribers;

    void notifyDestroyed();
};

template <typename Variant>
EventProducer<Variant>::~EventProducer() {
    notifyDestroyed();
}

}
