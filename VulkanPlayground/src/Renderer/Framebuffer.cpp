#include "Framebuffer.h"

namespace vkPlayground {

    Ref<Framebuffer> Framebuffer::Create()
    {
        return CreateRef<Framebuffer>();
    }

    Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
    {
        return CreateRef<Framebuffer>(spec);
    }

    Framebuffer::Framebuffer(const FramebufferSpecification& spec)
    {

    }

    Framebuffer::~Framebuffer()
    {

    }

    void Framebuffer::Invalidate()
    {

    }

    void Framebuffer::Resize(uint32_t width, uint32_t height, bool forceRecreate)
    {

    }

    void Framebuffer::Release()
    {

    }

}