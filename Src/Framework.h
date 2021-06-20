#pragma once
#include "Application.h"
#include "Device.h"
#include "./Common/Logger.h"
#include "Common.h"

namespace WIP3D
{
    enum class ShaderType
    {
        Vertex,         ///< Vertex shader
        Pixel,          ///< Pixel shader
        Geometry,       ///< Geometry shader
        Hull,           ///< Hull shader (AKA Tessellation control shader)
        Domain,         ///< Domain shader (AKA Tessellation evaluation shader)
        Compute,        ///< Compute shader

#ifdef WIP_D3D12
        RayGeneration,  ///< Ray generation shader
        Intersection,   ///< Intersection shader
        AnyHit,         ///< Any hit shader
        ClosestHit,     ///< Closest hit shader
        Miss,           ///< Miss shader
        Callable,       ///< Callable shader
#endif
        Count           ///< Shader Type count
    };

    /** Shading languages. Used for shader cross-compilation.
    */
    enum class ShadingLanguage
    {
        Unknown,        ///< Unknown language (e.g., for a plain .h file)
        GLSL,           ///< OpenGL Shading Language (GLSL)
        VulkanGLSL,     ///< GLSL for Vulkan
        HLSL,           ///< High-Level Shading Language
        Slang,          ///< Slang shading language
    };

    /** Framebuffer target flags. Used for clears and copy operations
    */
    enum class FboAttachmentType
    {
        None = 0,    ///< Nothing. Here just for completeness
        Color = 1,    ///< Operate on the color buffer.
        Depth = 2,    ///< Operate on the the depth buffer.
        Stencil = 4,    ///< Operate on the the stencil buffer.

        All = Color | Depth | Stencil ///< Operate on all targets
    };

    enum_class_operators(FboAttachmentType);


    enum class ComparisonFunc
    {
        Disabled,       ///< Comparison is disabled
        Never,          ///< Comparison always fails
        Always,         ///< Comparison always succeeds
        Less,           ///< Passes if source is less than the destination
        Equal,          ///< Passes if source is equal to the destination
        NotEqual,       ///< Passes if source is not equal to the destination
        LessEqual,      ///< Passes if source is less than or equal to the destination
        Greater,        ///< Passes if source is greater than to the destination
        GreaterEqual,   ///< Passes if source is greater than or equal to the destination
    };

    /** Flags indicating what hot-reloadable resources have changed
   */
    enum class HotReloadFlags
    {
        None = 0,    ///< Nothing. Here just for completeness
        Program = 1,    ///< Programs (shaders)
    };

    enum_class_operators(HotReloadFlags);

#define align_to(_alignment, _val) ((((_val) + (_alignment) - 1) / (_alignment)) * (_alignment))



    /** Helper class to check if a class has a vtable.
        Usage: has_vtable<MyClass>::value is true if vtable exists, false otherwise.
    */
    template<class T>
    struct has_vtable
    {
        class derived : public T
        {
            virtual void force_the_vtable() {}
        };
        enum { value = (sizeof(T) == sizeof(derived)) };
    };


}

namespace WIP3D
{
    /** Converts ShaderType enum elements to a string.
       \param[in] type Type to convert to string
       \return Shader type as a string
   */
    inline const std::string to_string(ShaderType Type)
    {
        switch (Type)
        {
        case ShaderType::Vertex:
            return "vertex";
        case ShaderType::Pixel:
            return "pixel";
        case ShaderType::Hull:
            return "hull";
        case ShaderType::Domain:
            return "domain";
        case ShaderType::Geometry:
            return "geometry";
        case ShaderType::Compute:
            return "compute";
#ifdef WIP_D3D12
        case ShaderType::RayGeneration:
            return "raygeneration";
        case ShaderType::Intersection:
            return "intersection";
        case ShaderType::AnyHit:
            return "anyhit";
        case ShaderType::ClosestHit:
            return "closesthit";
        case ShaderType::Miss:
            return "miss";
        case ShaderType::Callable:
            return "callable";
#endif
        default:
            assert(false);
            return "";
        }
    }
}

namespace WIP3D
{

















    class Clock;
    class FrameRate;

    /** Sample configuration
    */
    struct SampleConfig
    {
        Window::Desc windowDesc;                 ///< Controls window creation
        Device::Desc deviceDesc;                 ///< Controls device creation
        bool suppressInput = false;              ///< Suppress all keyboard and mouse input (other than escape to terminate)
        bool showMessageBoxOnError = true;       ///< Show message box on framework/API errors.
        float timeScale = 1.0f;                  ///< A scaling factor for the time elapsed between frames
        bool pauseTime = false;                  ///< Control whether or not to start the clock when the sample start running
        bool showUI = true;                      ///< Show the UI
    };

    class IFramework
    {
    public:
        /** Get the render-context for the current frame. This might change each frame.
        */
        virtual RenderContext* getRenderContext() = 0;

        /** Get the current FBO.
        */
        virtual std::shared_ptr<Fbo> getTargetFbo() = 0;

        /** Get the window.
        */
        virtual Window* getWindow() = 0;

        /** Get the global Clock object.
        */
        virtual Clock& getGlobalClock() = 0;

        /** Get the global FrameRate object.
        */
        virtual FrameRate& getFrameRate() = 0;

        /** Resize the swap-chain buffers.
        */
        virtual void resizeSwapChain(uint32_t width, uint32_t height) = 0;

        /** Render a frame.
        */
        virtual void renderFrame() = 0;

        /** Check if a key is pressed.
        */
        virtual bool isKeyPressed(const KeyboardEvent::Key& key) = 0;

        /** Show/hide the UI.
        */
        virtual void toggleUI(bool showUI) = 0;

        /** Show/hide the UI.
        */
        virtual bool isUiEnabled() = 0;

        /** Takes and outputs a screenshot.
        */
        virtual std::string captureScreen(const std::string explicitFilename = "", const std::string explicitOutputDirectory = "") = 0;

        /** Shutdown the app.
        */
        virtual void shutdown() = 0;

        /** Pause/resume the renderer. The GUI will still be rendered.
        */
        virtual void pauseRenderer(bool pause) = 0;

        /** Check if the renderer running.
        */
        virtual bool isRendererPaused() = 0;

        /** Get the current configuration.
        */
        virtual SampleConfig getConfig() = 0;

        /** Render the global UI. You'll can open a GUI window yourself before calling it.
        */
        virtual void renderGlobalUI(Gui* pGui) = 0;

        /** Get the global shortcuts message.
        */
        virtual std::string getKeyboardShortcutsStr() = 0;

        /** Set VSYNC.
        */
        virtual void toggleVsync(bool on) = 0;

        /** Get the VSYNC state.
        */
        virtual bool isVsyncEnabled() = 0;
    };

    dlldecl extern IFramework* gpFramework;

    class IRenderer
    {
    public:
        using UniquePtr = std::unique_ptr<IRenderer>;
        IRenderer() = default;
        virtual ~IRenderer() {};

        /** Called once right after context creation.
        */
        virtual void onLoad(RenderContext* pRenderContext) {}

        /** Called on each frame render.
        */
        virtual void onFrameRender(RenderContext* pRenderContext, const std::shared_ptr<Fbo>& pTargetFbo) {}

        /** Called right before the context is destroyed.
        */
        virtual void onShutdown() {}

        /** Called every time the swap-chain is resized. You can query the default FBO for the new size and sample count of the window.
        */
        virtual void onResizeSwapChain(uint32_t width, uint32_t height) {}

        /** Called upon hot reload (by pressing F5).
            \param[in] reloaded Resources that have been reloaded.
        */
        virtual void onHotReload(HotReloadFlags reloaded) {}

        /** Called every time a key event occurred.
        \param[in] keyEvent The keyboard event
        \return true if the event was consumed by the callback, otherwise false
        */
        virtual bool onKeyEvent(const KeyboardEvent& keyEvent) { return false; }

        /** Called every time a mouse event occurred.
        \param[in] mouseEvent The mouse event
        \return true if the event was consumed by the callback, otherwise false
        */
        virtual bool onMouseEvent(const MouseEvent& mouseEvent) { return false; }

        /** Called after onFrameRender().
        It is highly recommended to use onGuiRender() exclusively for GUI handling. onGuiRender() will not be called when the GUI is hidden, which should help reduce CPU overhead.
        You could also ignore this and render the GUI directly in your onFrameRender() function, but that is discouraged.
        */
        virtual void onGuiRender(Gui* pGui) {};

        /** Called when a file is dropped into the window
        */
        virtual void onDroppedFile(const std::string& filename) {}

        // Deleted copy operators (copy a pointer type!)
        IRenderer(const IRenderer&) = delete;
        IRenderer& operator=(const IRenderer&) = delete;
    };
}
