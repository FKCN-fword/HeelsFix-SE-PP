#pragma once
#include "SkeeAPI.h"

namespace HFN::Skee
{
    bool Connect();
    bool Ready();
    float HeelHeight(RE::Actor* actor, bool female);
    bool HasPosition(RE::Actor* actor, bool firstPerson, bool female, const char* node, const char* key);
    bool HasRotation(RE::Actor* actor, bool firstPerson, bool female, const char* node, const char* key);
    SKEE::INiTransformInterface::Position Position(RE::Actor* actor, bool firstPerson, bool female, const char* node, const char* key);
    void SetPosition(RE::Actor* actor, bool firstPerson, bool female, const char* node, const char* key, float x, float y, float z);
    void SetRotation(RE::Actor* actor, bool firstPerson, bool female, const char* node, const char* key, float x, float y, float z);
    bool RemovePosition(RE::Actor* actor, bool firstPerson, bool female, const char* node, const char* key);
    bool RemoveRotation(RE::Actor* actor, bool firstPerson, bool female, const char* node, const char* key);
    bool RemoveTransform(RE::Actor* actor, bool firstPerson, bool female, const char* node, const char* key);
    void Update(RE::Actor* actor, bool firstPerson, bool female, const char* node);
}
