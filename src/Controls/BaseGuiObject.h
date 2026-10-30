#pragma once

#include "ObjectUserdata.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Luwow::Udc {
    // The base of every gui object.
    class BaseGuiObject : public virtual Luwow::Objects::BaseObject {
    public:
        static constexpr const char* Name = "BaseGuiObject";
        using Super = Luwow::Objects::BaseObject;

        // The object it's in: nullptr for a window, and for a control once it's destroyed
        virtual BaseGuiObject* parentObject() const = 0;
        // The objects in it, in the order they were added
        virtual std::vector<BaseGuiObject*> childObjects() const = 0;
        // Moves it into the object at index, raising when it can't be. Parent is read only by default.
        virtual void setParentObject(lua_State* L, int index);

        // Fires the signals of the bound properties whose values changed since they were last seen
        void checkPropertiesChanged(lua_State* L);
        void checkPropertyChanged(lua_State* L, std::string_view property);

        // Disconnects and releases the property signals, called when the object is destroyed
        void releasePropertySignals(lua_State* L);
        void propertyChanged(lua_State* L, std::string_view property) override;

        template <typename T>
        static void RegisterGuiObject(lua_State* L, std::vector<typename T::MethodDef> methods, std::vector<typename T::PropertyDef> properties) {
            methods.push_back({ "FetchChildren", T::template Bind<&Luau::fetchChildren> });
            methods.push_back({ "BindToPropertyChanged", T::template Bind<&Luau::bindToPropertyChanged> });
            properties.push_back({ "Parent", T::template Bind<&Luau::getParent>, T::template Bind<&Luau::setParent> });
            T::RegisterObject(L, std::move(methods), std::move(properties));
        }

    protected:
        // Called when a property's signal is first made, so internal changes can be looked for
        virtual void propertySignalAdded() {}

    private:
        // Registry references to the signal and to the value its property last had
        struct PropertySignal {
            int signal;
            int lastValue;
        };
        std::unordered_map<std::string, PropertySignal> propertySignals;

        // The Luau members
        struct Luau {
            static int fetchChildren(lua_State* L, BaseGuiObject& object);
            static int bindToPropertyChanged(lua_State* L, BaseGuiObject& object);
            static int getParent(lua_State* L, BaseGuiObject& object);
            static int setParent(lua_State* L, BaseGuiObject& object);
        };
    };
} // namespace Luwow::Udc
