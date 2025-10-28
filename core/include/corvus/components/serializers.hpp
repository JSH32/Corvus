#pragma once
#include <cereal/cereal.hpp>
#include <raylib-cpp.hpp>

namespace cereal {
template <class Archive>
void serialize(Archive& ar, raylib::Vector3& v) {
    ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z));
}

template <class Archive>
void serialize(Archive& ar, raylib::Quaternion& q) {
    ar(cereal::make_nvp("x", q.x),
       cereal::make_nvp("y", q.y),
       cereal::make_nvp("z", q.z),
       cereal::make_nvp("w", q.w));
}

template <class Archive>
void serialize(Archive& ar, Color& color) {
    ar(cereal::make_nvp("r", color.r));
    ar(cereal::make_nvp("g", color.g));
    ar(cereal::make_nvp("b", color.b));
    ar(cereal::make_nvp("a", color.a));
}
}
