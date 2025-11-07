# ApiAgnosticRendering
OOP base Renderer (abstract) that defines high-level methods: surface init, create shaders, create pipeline, start/stop, cleanup.

## Design notes
1. Renderer (abstract): public methods initialize(), renderFrame(), shutdown(). Also exposes protected low-level virtual hooks for concrete implementations (surfaceCreate(), createShader(), createPipeline(), etc.) so high-level flow stays in base class if desired.

2. InitHandler (Chain of Responsibility): each handler does one job and calls next->handle(ctx). The chain is assembled by the client or the factory. Handlers can be generic (logging, validation) or API-specific.

3. RendererFactory: simple factory returning unique_ptr<Renderer> given an enum/string.

4. InitContext: shared state passed along chain (window/surface info, shader sources, device handles — mocked as void* here).

5. Concrete OpenGLRenderer / VulkanRenderer implement the low-level operations and can expose API-specific resources inside InitContext.

## Todo

Actual implementation yet to be verified. This is still just a demo for OOPs concepts.