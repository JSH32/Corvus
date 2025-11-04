#pragma once
#include "corvus/input/event_producer.hpp"
#include <algorithm>
#include <functional>
#include <vector>

namespace Corvus::Core::Events {

template <typename Variant>
class EventConsumer {
public:
    virtual ~EventConsumer() {
        for (auto& wp : producers) {
            if (auto sp = wp.lock())
                sp->unsubscribe(this);
        }
    }

protected:
    virtual void onEvent(const Variant&) { }

    template <typename... Visitors>
    static void matchEvent(const Variant& variant, Visitors&&... visitors) {
        using namespace Corvus::Core::Events;
        std::visit(overloaded { std::forward<Visitors>(visitors)... }, variant);
    }

private:
    friend class EventProducer<Variant>;

    std::vector<std::weak_ptr<EventProducer<Variant>>> producers;

    void handle(const Variant& v) { onEvent(v); }

    void onProducerDestroyed(EventProducer<Variant>* ptr) {
        producers.erase(std::remove_if(producers.begin(),
                                       producers.end(),
                                       [ptr](const auto& wp) {
                                           auto sp = wp.lock();
                                           return !sp || sp.get() == ptr;
                                       }),
                        producers.end());
    }

    template <typename... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };
    template <typename... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;
};

template <typename Variant>
void EventProducer<Variant>::notifyDestroyed() {
    for (auto& [consumer, _] : subscribers)
        if (consumer)
            consumer->onProducerDestroyed(this);
}

template <typename Variant>
void EventProducer<Variant>::attachConsumer(EventConsumer<Variant>& consumer) {
    consumer.producers.push_back(this->weak_from_this());
    subscribe(&consumer, [&consumer](const Variant& v) { consumer.handle(v); });
}
}
