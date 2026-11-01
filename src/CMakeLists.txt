# User drawn controls, shared by every platform
set(GUI_CONTROL_SOURCES
    Controls/Canvas.h
    Controls/Scaling.h
    Controls/Scaling.cpp
    Controls/BaseGuiObject.h
    Controls/BaseGuiObject.cpp
    Controls/ControlObject.h
    Controls/ControlObject.cpp
    Controls/ControlView.h
    Controls/ControlView.cpp
    Controls/ControlLuau.h
    Controls/ControlLuau.cpp
    Controls/ControlRegistry.h
    Controls/ControlRegistry.cpp
    Controls/View.h
    Controls/View.cpp
    Controls/ScrollView.h
    Controls/ScrollView.cpp
    Controls/TextObject.h
    Controls/TextObject.cpp
    Controls/ImageObject.h
    Controls/ImageObject.cpp
    Controls/ImageLabel.h
    Controls/ImageLabel.cpp
    Controls/ImageButton.h
    Controls/ImageButton.cpp
    Controls/TextLabel.h
    Controls/TextLabel.cpp
    Controls/TextButton.h
    Controls/TextButton.cpp
    Controls/TextInput.h
    Controls/TextInput.cpp
    Controls/ListLayout.h
    Controls/ListLayout.cpp
    Controls/Corner.h
    Controls/Corner.cpp
    Controls/Padding.h
    Controls/Padding.cpp
    Controls/Stroke.h
    Controls/Stroke.cpp
    Controls/Gradient.h
    Controls/Gradient.cpp
    Controls/Input.h
    Controls/Input.cpp
)

if(WIN32)
    luwow_add_library(Luwow.Udc
        NAME udc
        MODES SERIAL
        SOURCES
            Descriptors.cpp
            Descriptors.h
            IUdcModule.h
            Handles.cpp
            Handles.h
            PackageFiles.h
            ${GUI_CONTROL_SOURCES}
            Windows/Dpi.cpp
            Windows/Dpi.h
            Windows/Input.cpp
            Windows/Input.h
            Windows/GdiCanvas.cpp
            Windows/GdiCanvas.h
            Windows/DirectWriteText.cpp
            Windows/DirectWriteText.h
            Windows/UdcModule.cpp
            Windows/UdcModule.h
            Windows/Window.cpp
            Windows/Window.h
    )
    target_include_directories(Luwow.Udc PRIVATE
        ${UDC_ROOT}/src
        ${UDC_ROOT}/src/Windows
    )
    target_link_libraries(Luwow.Udc PRIVATE Luwow.BaseObject Luwow.ColorCore Luwow.RectCore Luwow.UDimCore Luwow.SignalCore)
    target_link_libraries(Luwow.Udc PRIVATE gdiplus dwrite d2d1 windowscodecs ole32)
    target_link_libraries(Luwow.Udc PRIVATE comctl32)
    target_link_libraries(Luwow.Udc PRIVATE shlwapi)
    target_compile_definitions(Luwow.Udc PRIVATE NOMINMAX)
elseif(APPLE)
    luwow_add_library(Luwow.Udc
        NAME udc
        MODES SERIAL
        SOURCES
            Descriptors.cpp
            Descriptors.h
            IUdcModule.h
            Handles.cpp
            Handles.h
            PackageFiles.h
            ${GUI_CONTROL_SOURCES}
            Mac/CocoaCanvas.mm
            Mac/CocoaCanvas.h
            Mac/Input.mm
            Mac/Input.h
            Mac/UdcModule.mm
            Mac/UdcModule.h
            Mac/Window.mm
            Mac/Window.h
    )
    target_include_directories(Luwow.Udc PRIVATE
        ${UDC_ROOT}/src
        ${UDC_ROOT}/src/Mac
    )

    target_link_libraries(Luwow.Udc PRIVATE Luwow.BaseObject Luwow.ColorCore Luwow.RectCore Luwow.UDimCore Luwow.SignalCore)

    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(CORETEXT_FRAMEWORK CoreText REQUIRED)
    # PUBLIC so executables linking this static library also link AppKit/Cocoa.
    target_link_libraries(Luwow.Udc PUBLIC ${COCOA_FRAMEWORK} ${CORETEXT_FRAMEWORK})
    set_target_properties(Luwow.Udc PROPERTIES OBJCXX_STANDARD 20)
    target_compile_options(Luwow.Udc PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
endif()
