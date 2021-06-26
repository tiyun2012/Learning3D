/*     Copyright 2015-2018 Egor Yusov
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
*
*  In no event and under no legal theory, whether in tort (including negligence),
*  contract, or otherwise, unless required by applicable law (such as deliberate
*  and grossly negligent acts) or agreed to in writing, shall any Contributor be
*  liable for any damages, including any direct, indirect, special, incidental,
*  or consequential damages of any character arising as a result of this License or
*  out of the use or inability to use the software (including but not limited to damages
*  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
*  all other commercial damages or losses), even if such Contributor has been advised
*  of the possibility of such damages.
*/

#include "AndroidFileSystem.hpp"
#include "EngineFactoryOpenGL.h"
#if VULKAN_SUPPORTED
#    include "EngineFactoryVk.h"
#endif

#include "SampleApp.hpp"
#include "RenderDeviceGLES.h"
#include "ImGuiImplAndroid.hpp"


namespace Diligent
{

class SampleAppAndroid final : public SampleApp
{
public:
    std::atomic_bool paused;
    SampleAppAndroid()
    {
        paused = true;
        m_DeviceType = RENDER_DEVICE_TYPE_GLES;
    }

    virtual void DrawFrame() override final
    {
        if (paused || !m_pSwapChain)
            return;

        // It is amazingly frustrating, but there is no robust way to detect orientation change:
        // - APP_CMD_WINDOW_RESIZED and APP_CMD_CONTENT_RECT_CHANGED events are never generated by native_app_glue as of NDK r21b (21.1.6352462)
        // - onNativeWindowResized callback is only called once after the window has been created
        // - APP_CMD_CONFIG_CHANGED may be generated either before or after physical surface has actually been rotated
        //
        // So, as annoying as it is, we have to check if the surface has been resized every frame
        m_pSwapChain->Resize(0, 0, SURFACE_TRANSFORM_OPTIMAL);

        const auto& SCDesc = m_pSwapChain->GetDesc();
        if (m_WindowWidth != SCDesc.Width || m_WindowHeight != SCDesc.Height || m_PreTransform != SCDesc.PreTransform)
        {
            m_WindowWidth  = SCDesc.Width;
            m_WindowHeight = SCDesc.Height;
            m_PreTransform = SCDesc.PreTransform;

            m_TheSample->WindowResize(static_cast<int>(m_WindowWidth), static_cast<int>(m_WindowHeight));
        }

        SampleApp::DrawFrame();
    }

    virtual void Initialize() override final
    {
        switch (m_DeviceType)
        {
#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                GetEngineFactoryVk()->InitAndroidFileSystem(app_->activity, native_activity_class_name_.c_str(), nullptr);
                break;
#endif

            case RENDER_DEVICE_TYPE_GLES:
                GetEngineFactoryOpenGL()->InitAndroidFileSystem(app_->activity, native_activity_class_name_.c_str(), nullptr);
                break;

            default:
                UNEXPECTED("Unexpected device type");
        }

        AndroidFileSystem::Init(app_->activity, native_activity_class_name_.c_str(), nullptr);

        SampleApp::Initialize();

        AndroidNativeWindow Window;
        Window.pAWindow = app_->window;
        InitializeDiligentEngine(&Window);
        const auto& SCDesc = m_pSwapChain->GetDesc();
        m_pImGui.reset(new ImGuiImplAndroid(m_pDevice, SCDesc.ColorBufferFormat, SCDesc.DepthBufferFormat));

        m_WindowWidth  = SCDesc.Width;
        m_WindowHeight = SCDesc.Height;
        m_PreTransform = SCDesc.PreTransform;

        switch (m_DeviceType)
        {
#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                break;
#endif

            case RENDER_DEVICE_TYPE_GLES:
                m_RenderDeviceGLES = RefCntAutoPtr<IRenderDeviceGLES>(m_pDevice, IID_RenderDeviceGLES);
                break;

            default:
                UNEXPECTED("Unexpected device type");
        }

        InitializeSample();
        paused = false;
    }

    virtual int Resume(ANativeWindow* window) override final
    {
        switch (m_DeviceType)
        {
#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
            {
                // Create a new swap chain for the new window
                m_pSwapChain.Release();
                m_TheSample->ResetSwapChain(nullptr);
                m_pDevice->IdleGPU();

                AndroidNativeWindow AndroidWindow;
                AndroidWindow.pAWindow = window;
                GetEngineFactoryVk()->CreateSwapChainVk(m_pDevice, GetImmediateContext(),
                                                        m_SwapChainInitDesc, AndroidWindow,
                                                        &m_pSwapChain);
                m_TheSample->ResetSwapChain(m_pSwapChain);
                paused = m_pSwapChain;
                return paused ? EGL_SUCCESS : EGL_NOT_INITIALIZED;
            }
#endif

            case RENDER_DEVICE_TYPE_GLES: {
                auto ret = m_RenderDeviceGLES->Resume(window);
                paused = ret == EGL_SUCCESS ? true : false;
                return ret;
            }

            default:
                UNEXPECTED("Unexpected device type");
        }

        return EGL_NOT_INITIALIZED;
    }

    virtual void TermDisplay() override final
    {
        paused = true;
        switch (m_DeviceType)
        {
#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                // Destroy the swap chain as we will need to recreate it for the new window
                m_pSwapChain.Release();
                m_TheSample->ResetSwapChain(nullptr);
                break;
#endif

            case RENDER_DEVICE_TYPE_GLES:
                // Tear down the EGL context currently associated with the display.
                if (m_RenderDeviceGLES)
                {
                    m_RenderDeviceGLES->Suspend();
                }
                break;

            default:
                UNEXPECTED("Unexpected device type");
        }
    }

    virtual void TrimMemory() override final
    {
        LOGI("Trimming memory");
        switch (m_DeviceType)
        {
#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:

                break;
#endif

            case RENDER_DEVICE_TYPE_GLES:
                if (m_RenderDeviceGLES)
                {
                    m_RenderDeviceGLES->Invalidate();
                }
                break;

            default:
                UNEXPECTED("Unexpected device type");
        }
    }

    virtual int32_t HandleInput(AInputEvent* event) override final
    {
        if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
        {
            ndk_helper::GESTURE_STATE doubleTapState = doubletap_detector_.Detect(event);
            ndk_helper::GESTURE_STATE dragState      = drag_detector_.Detect(event);
            ndk_helper::GESTURE_STATE pinchState     = pinch_detector_.Detect(event);

            //Double tap detector has a priority over other detectors
            if (doubleTapState == ndk_helper::GESTURE_STATE_ACTION)
            {
                //Detect double tap
                //tap_camera_.Reset( true );
            }
            else
            {
                //Handle drag state
                if (dragState & ndk_helper::GESTURE_STATE_START)
                {
                    //Otherwise, start dragging
                    ndk_helper::Vec2 v;
                    drag_detector_.GetPointer(v);
                    float fX = 0, fY = 0;
                    v.Value(fX, fY);
                    auto Handled = static_cast<ImGuiImplAndroid*>(m_pImGui.get())->BeginDrag(fX, fY);
                    if (!Handled)
                    {
                        m_TheSample->GetInputController().BeginDrag(fX, fY);
                    }
                }
                else if (dragState & ndk_helper::GESTURE_STATE_MOVE)
                {
                    ndk_helper::Vec2 v;
                    drag_detector_.GetPointer(v);
                    float fX = 0, fY = 0;
                    v.Value(fX, fY);
                    auto Handled = static_cast<ImGuiImplAndroid*>(m_pImGui.get())->DragMove(fX, fY);
                    if (!Handled)
                    {
                        m_TheSample->GetInputController().DragMove(fX, fY);
                    }
                }
                else if (dragState & ndk_helper::GESTURE_STATE_END)
                {
                    static_cast<ImGuiImplAndroid*>(m_pImGui.get())->EndDrag();
                    m_TheSample->GetInputController().EndDrag();
                }

                //Handle pinch state
                if (pinchState & ndk_helper::GESTURE_STATE_START)
                {
                    //Start new pinch
                    ndk_helper::Vec2 v1;
                    ndk_helper::Vec2 v2;
                    pinch_detector_.GetPointers(v1, v2);
                    float fX1 = 0, fY1 = 0, fX2 = 0, fY2 = 0;
                    v1.Value(fX1, fY1);
                    v2.Value(fX2, fY2);
                    m_TheSample->GetInputController().StartPinch(fX1, fY1, fX2, fY2);
                    //tap_camera_.BeginPinch( v1, v2 );
                }
                else if (pinchState & ndk_helper::GESTURE_STATE_MOVE)
                {
                    //Multi touch
                    //Start new pinch
                    ndk_helper::Vec2 v1;
                    ndk_helper::Vec2 v2;
                    pinch_detector_.GetPointers(v1, v2);
                    float fX1 = 0, fY1 = 0, fX2 = 0, fY2 = 0;
                    v1.Value(fX1, fY1);
                    v2.Value(fX2, fY2);
                    m_TheSample->GetInputController().PinchMove(fX1, fY1, fX2, fY2);
                    //tap_camera_.Pinch( v1, v2 );
                }
                else if (pinchState & ndk_helper::GESTURE_STATE_END)
                {
                    m_TheSample->GetInputController().EndPinch();
                }
            }
            return 1;
        }
        return 0;
    }

private:
    RefCntAutoPtr<IRenderDeviceGLES> m_RenderDeviceGLES;

    Uint32            m_WindowWidth  = 0;
    Uint32            m_WindowHeight = 0;
    SURFACE_TRANSFORM m_PreTransform = SURFACE_TRANSFORM_OPTIMAL;
};

NativeAppBase* CreateApplication()
{
    return new SampleAppAndroid;
}

} // namespace Diligent
