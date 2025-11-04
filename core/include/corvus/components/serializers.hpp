#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <cereal/cereal.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace cereal {
template <class Archive>
void serialize(Archive& ar, glm::vec2& v) {
    ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y));
}

template <class Archive>
void serialize(Archive& ar, glm::vec3& v) {
    ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z));
}

template <class Archive>
void serialize(Archive& ar, glm::vec4& v) {
    ar(cereal::make_nvp("x", v.x),
       cereal::make_nvp("y", v.y),
       cereal::make_nvp("z", v.z),
       cereal::make_nvp("w", v.w));
}

template <class Archive>
void serialize(Archive& ar, glm::quat& q) {
    ar(cereal::make_nvp("x", q.x),
       cereal::make_nvp("y", q.y),
       cereal::make_nvp("z", q.z),
       cereal::make_nvp("w", q.w));
}

}
