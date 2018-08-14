#pragma once

#include <vsg/viewer/Window.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


namespace glfw
{

class GLFW_Instance : public vsg::Object
{
public:
protected:
    GLFW_Instance();

    virtual ~GLFW_Instance();

    friend vsg::ref_ptr<glfw::GLFW_Instance> getGLFW_Instance();
};

extern vsg::ref_ptr<glfw::GLFW_Instance> getGLFW_Instance();


class GLFW_Window : public vsg::Window
{
public:

    GLFW_Window() = delete;
    GLFW_Window(const GLFW_Window&) = delete;
    GLFW_Window& operator = (const GLFW_Window&) = delete;

    GLFW_Window(uint32_t width, uint32_t height, bool debugLayer=false, bool apiDumpLayer=false, vsg::Window* shareWindow=nullptr);

    virtual bool valid() const { return _window && !glfwWindowShouldClose(_window); }

    virtual bool pollEvents();

    virtual bool resized() const;

    virtual void resize();

    operator GLFWwindow* () { return _window; }
    operator const GLFWwindow* () const { return _window; }

protected:
    virtual ~GLFW_Window();

    vsg::ref_ptr<glfw::GLFW_Instance>   _glwInstance;
    GLFWwindow*                         _window;
};


}

