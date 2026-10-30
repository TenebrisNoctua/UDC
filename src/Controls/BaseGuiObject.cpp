#include "BaseGuiObject.h"

#include "SignalCore.h"

#include "lua.h"
#include "lualib.h"

namespace Luwow::Udc {
    // Pushes object[property], through its __index, so it's the value scripts see
    static void pushProperty(lua_State* L, BaseGuiObject& object, const char* property) {
        lua_checkstack(L, 4);
        object.push(L);
        lua_getfield(L, -1, property);
        lua_remove(L, -2);
    }

    void BaseGuiObject::setParentObject(lua_State* L, int index) {
        luaL_error(L, "the Parent of a %s can't be changed", getClassName());
    }

    // Compared with Luau equality
    void BaseGuiObject::checkPropertyChanged(lua_State* L, std::string_view property) {
        if (isDestroyed()) return;
        std::string key(property);
        auto it = propertySignals.find(key);
        if (it == propertySignals.end()) return;

        pushProperty(L, *this, key.c_str());
        lua_getref(L, it->second.lastValue);
        bool same = lua_equal(L, -1, -2);
        lua_pop(L, 1);
        if (same) {
            lua_pop(L, 1);
            return;
        }

        lua_unref(L, it->second.lastValue);
        it->second.lastValue = lua_ref(L, -1);
        lua_pop(L, 1);

        // A handler may destroy the object or bind other properties, so nothing here is used afterwards
        lua_getref(L, it->second.signal);
        Luwow::Signals::fireSignal(L, -1, 0);
        lua_pop(L, 1);
    }

    void BaseGuiObject::checkPropertiesChanged(lua_State* L) {
        std::vector<std::string> properties;
        properties.reserve(propertySignals.size());
        for (const auto& [property, signal] : propertySignals) properties.push_back(property);

        for (const std::string& property : properties) {
            if (isDestroyed()) return;
            checkPropertyChanged(L, property);
        }
    }

    void BaseGuiObject::releasePropertySignals(lua_State* L) {
        for (auto& [property, signal] : propertySignals) {
            lua_getref(L, signal.signal);
            Luwow::Signals::disconnectAll(L, -1);
            lua_pop(L, 1);
            lua_unref(L, signal.signal);
            lua_unref(L, signal.lastValue);
        }
        propertySignals.clear();
    }

    void BaseGuiObject::propertyChanged(lua_State* L, std::string_view property) {
        checkPropertyChanged(L, property);
    }

    // Luau

    // An array of the children, helpers such as a ListLayout included
    int BaseGuiObject::Luau::fetchChildren(lua_State* L, BaseGuiObject& object) {
        std::vector<BaseGuiObject*> children = object.childObjects();
        lua_createtable(L, static_cast<int>(children.size()), 0);
        for (size_t i = 0; i < children.size(); ++i) {
            children[i]->push(L);
            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }
        return 1;
    }

    // The same signal each time for a property. Reading the property checks it exists, and gives the value
    // later ones are compared with.
    int BaseGuiObject::Luau::bindToPropertyChanged(lua_State* L, BaseGuiObject& object) {
        const char* property = luaL_checkstring(L, 2);
        auto it = object.propertySignals.find(property);
        if (it != object.propertySignals.end()) {
            lua_getref(L, it->second.signal);
            return 1;
        }

        pushProperty(L, object, property);
        if (lua_isfunction(L, -1)) luaL_error(L, "%s is a method of %s, not a property", property, object.getClassName());
        int lastValue = lua_ref(L, -1);
        lua_pop(L, 1);

        Luwow::Signals::pushSignal(L);
        object.propertySignals[property] = PropertySignal{ lua_ref(L, -1), lastValue };
        object.propertySignalAdded();
        return 1;
    }

    int BaseGuiObject::Luau::getParent(lua_State* L, BaseGuiObject& object) {
        if (BaseGuiObject* parent = object.parentObject()) parent->push(L);
        else lua_pushnil(L);
        return 1;
    }

    int BaseGuiObject::Luau::setParent(lua_State* L, BaseGuiObject& object) {
        object.setParentObject(L, 3);
        return 0;
    }
} // namespace Luwow::Udc
