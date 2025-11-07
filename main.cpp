// renderer_example.cpp
#include <iostream>
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <optional>

// ---------- Shared domain objects ----------
struct InitContext {
    std::string appName;
    std::string surfaceType; // "window", "offscreen"
    std::string shaderVertexSrc;
    std::string shaderFragmentSrc;

    // API-specific handles (mocked). In real code use typed handles.
    void* apiDevice = nullptr;
    void* apiSurface = nullptr;
    void* apiPipeline = nullptr;

    // Error / status
    bool success = true;
    std::string lastError;
};

// ---------- Chain of Responsibility ----------
class InitHandler {
public:
    virtual ~InitHandler() = default;
    void setNext(std::shared_ptr<InitHandler> n) { next_ = n; }
    // returns true on success
    bool handle(InitContext &ctx) {
        if (!process(ctx)) return false;
        if (next_) return next_->handle(ctx);
        return true;
    }
protected:
    // implement in derived. Should set ctx.lastError & ctx.success on failure.
    virtual bool process(InitContext &ctx) = 0;
private:
    std::shared_ptr<InitHandler> next_;
};

// Concrete handlers (mock implementations)
class SurfaceHandler : public InitHandler {
protected:
    bool process(InitContext &ctx) override {
        std::cout << "[SurfaceHandler] creating surface of type: " << ctx.surfaceType << "\n";
        // Mock: set a fake surface handle
        ctx.apiSurface = reinterpret_cast<void*>(0x1);
        return true;
    }
};

class DeviceHandler : public InitHandler {
protected:
    bool process(InitContext &ctx) override {
        std::cout << "[DeviceHandler] creating device for app: " << ctx.appName << "\n";
        ctx.apiDevice = reinterpret_cast<void*>(0x2);
        return true;
    }
};

class ShaderHandler : public InitHandler {
public:
    ShaderHandler(std::function<bool(InitContext&)> compileFn) : compile_(compileFn) {}
protected:
    bool process(InitContext &ctx) override {
        std::cout << "[ShaderHandler] compiling shaders...\n";
        bool ok = compile_(ctx);
        if (!ok) {
            ctx.lastError = "Shader compilation failed";
            ctx.success = false;
            std::cerr << "[ShaderHandler] shader compilation failed\n";
            return false;
        }
        std::cout << "[ShaderHandler] shaders compiled OK\n";
        return true;
    }
private:
    std::function<bool(InitContext&)> compile_;
};

class PipelineHandler : public InitHandler {
protected:
    bool process(InitContext &ctx) override {
        std::cout << "[PipelineHandler] creating pipeline using device=" << ctx.apiDevice << " surface=" << ctx.apiSurface << "\n";
        ctx.apiPipeline = reinterpret_cast<void*>(0x3);
        return true;
    }
};

class FinalizeHandler : public InitHandler {
protected:
    bool process(InitContext &ctx) override {
        std::cout << "[FinalizeHandler] final validation & ready\n";
        return true;
    }
};

// ---------- Renderer base class ----------
class Renderer {
public:
    virtual ~Renderer() = default;

    // high-level API used by client
    bool initialize() {
        InitContext ctx;
        ctx.appName = appName_;
        ctx.surfaceType = surfaceType_;
        ctx.shaderVertexSrc = vertexSrc_;
        ctx.shaderFragmentSrc = fragmentSrc_;
        // Build chain
        auto chain = buildInitChain();
        if (!chain) {
            std::cerr << "[Renderer] No init chain provided\n";
            return false;
        }
        bool ok = chain->handle(ctx);
        if (!ok) {
            std::cerr << "[Renderer] Initialization failed: " << ctx.lastError << "\n";
            return false;
        }
        // allow concrete impl to store handles / post-init
        onPostInit(ctx);
        initialized_ = true;
        return true;
    }

    virtual void renderFrame() = 0;
    virtual void shutdown() {
        std::cout << "[Renderer] shutdown\n";
        initialized_ = false;
    }

    // configuration helpers
    void setAppName(std::string n) { appName_ = std::move(n); }
    void setSurfaceType(std::string s) { surfaceType_ = std::move(s); }
    void setVertexShaderSrc(std::string s) { vertexSrc_ = std::move(s); }
    void setFragmentShaderSrc(std::string s) { fragmentSrc_ = std::move(s); }

protected:
    // Concrete classes should override to provide an initialization chain (handlers)
    virtual std::shared_ptr<InitHandler> buildInitChain() = 0;
    // After chain success, concrete implementation can capture handles from context
    virtual void onPostInit(InitContext const& ctx) {
        // default: nothing
        (void)ctx;
    }

    bool initialized_ = false;
private:
    std::string appName_ = "App";
    std::string surfaceType_ = "window";
    std::string vertexSrc_, fragmentSrc_;
};

// ---------- Concrete Renderers (mocked) ----------
class OpenGLRenderer : public Renderer {
protected:
    std::shared_ptr<InitHandler> buildInitChain() override {
        auto s = std::make_shared<SurfaceHandler>();
        auto d = std::make_shared<DeviceHandler>();
        // shader handler with OpenGL-specific compile mock
        auto shader = std::make_shared<ShaderHandler>(
            [](InitContext &ctx)->bool {
                std::cout << "  [OpenGL] compiling vertex shader (len=" << ctx.shaderVertexSrc.size() << ")\n";
                std::cout << "  [OpenGL] compiling fragment shader (len=" << ctx.shaderFragmentSrc.size() << ")\n";
                // pretend success
                return true;
            }
        );
        auto p = std::make_shared<PipelineHandler>();
        auto f = std::make_shared<FinalizeHandler>();
        // chain: s -> d -> shader -> pipeline -> finalize
        s->setNext(d);
        d->setNext(shader);
        shader->setNext(p);
        p->setNext(f);
        return s;
    }

    void onPostInit(InitContext const& ctx) override {
        std::cout << "[OpenGLRenderer] post-init: capturing handles\n";
        oglDevice_ = ctx.apiDevice;
        oglSurface_ = ctx.apiSurface;
        oglPipeline_ = ctx.apiPipeline;
    }

public:
    void renderFrame() override {
        if (!initialized_) { std::cerr << "[OpenGLRenderer] not initialized\n"; return; }
        std::cout << "[OpenGLRenderer] rendering frame (mock)\n";
        // real code: bind pipeline, draw, swapbuffers
    }

private:
    void* oglDevice_ = nullptr;
    void* oglSurface_ = nullptr;
    void* oglPipeline_ = nullptr;
};

class VulkanRenderer : public Renderer {
protected:
    std::shared_ptr<InitHandler> buildInitChain() override {
        // Vulkan may require a different chain (e.g., instance -> surface -> physical device -> logical device -> swapchain -> shaders -> pipeline)
        auto s = std::make_shared<SurfaceHandler>();
        auto d = std::make_shared<DeviceHandler>();
        auto shader = std::make_shared<ShaderHandler>(
            [](InitContext &ctx)->bool {
                std::cout << "  [Vulkan] compiling SPIR-V from GLSL (len=" << ctx.shaderVertexSrc.size() << ")\n";
                // pretend success
                return true;
            }
        );
        auto p = std::make_shared<PipelineHandler>();
        auto f = std::make_shared<FinalizeHandler>();
        // chain assembly (could insert more vk-specific handlers)
        s->setNext(d);
        d->setNext(shader);
        shader->setNext(p);
        p->setNext(f);
        return s;
    }

    void onPostInit(InitContext const& ctx) override {
        std::cout << "[VulkanRenderer] post-init: storing vk handles (mock)\n";
        vkDevice_ = ctx.apiDevice;
        vkSurface_ = ctx.apiSurface;
        vkPipeline_ = ctx.apiPipeline;
    }

public:
    void renderFrame() override {
        if (!initialized_) { std::cerr << "[VulkanRenderer] not initialized\n"; return; }
        std::cout << "[VulkanRenderer] rendering frame (mock)\n";
        // real code: command buffers, submit, present
    }

private:
    void* vkDevice_ = nullptr;
    void* vkSurface_ = nullptr;
    void* vkPipeline_ = nullptr;
};

// ---------- Factory ----------
enum class GraphicsAPI { OpenGL, Vulkan };

std::unique_ptr<Renderer> RendererFactory(GraphicsAPI api) {
    switch(api) {
        case GraphicsAPI::OpenGL: return std::make_unique<OpenGLRenderer>();
        case GraphicsAPI::Vulkan: return std::make_unique<VulkanRenderer>();
    }
    return nullptr;
}

// ---------- Example usage ----------
int main() {
    std::cout << "=== Factory + Chain of Responsibility Renderer Demo ===\n";

    {
        auto renderer = RendererFactory(GraphicsAPI::OpenGL);
        renderer->setAppName("DemoOpenGL");
        renderer->setSurfaceType("window");
        renderer->setVertexShaderSrc("// vertex shader source");
        renderer->setFragmentShaderSrc("// fragment shader source");
        if (renderer->initialize()) {
            renderer->renderFrame();
            renderer->shutdown();
        }
    }

    std::cout << "----\n";

    {
        auto renderer = RendererFactory(GraphicsAPI::Vulkan);
        renderer->setAppName("DemoVulkan");
        renderer->setSurfaceType("window");
        renderer->setVertexShaderSrc("// vertex shader source");
        renderer->setFragmentShaderSrc("// fragment shader source");
        if (renderer->initialize()) {
            renderer->renderFrame();
            renderer->shutdown();
        }
    }

    return 0;
}
