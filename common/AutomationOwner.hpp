#pragma once
#include <Geode/Geode.hpp>

// Shared, scene-scoped ownership; both mods must use this key before injecting.
namespace automation {
inline constexpr char key[] = "leokang.automation-owner";
inline bool owns(cocos2d::CCNode* scene, char const* owner) {
    auto current = static_cast<cocos2d::CCString*>(scene->getUserObject(key));
    return current && std::string_view(current->getCString()) == owner;
}
inline bool acquire(cocos2d::CCNode* scene, char const* owner) {
    auto current = static_cast<cocos2d::CCString*>(scene->getUserObject(key));
    if (current && std::string_view(current->getCString()) != owner) return false;
    scene->setUserObject(key, cocos2d::CCString::create(owner));
    return true;
}
inline void release(cocos2d::CCNode* scene, char const* owner) {
    auto current = static_cast<cocos2d::CCString*>(scene->getUserObject(key));
    if (current && std::string_view(current->getCString()) == owner)
        scene->setUserObject(key, nullptr);
}
}
