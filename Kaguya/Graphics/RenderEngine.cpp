#include "pch.h"
#include "RenderEngine.h"
#include "../Core/Time.h"
#include "../Core/Window.h"

RenderEngine::RenderEngine(Window& Window)
	: m_Window(Window),
	m_RenderDevice(m_DXGIManager.QueryAdapter(), FALSE),
	m_AspectRatio(static_cast<float>(m_Window.GetWindowWidth()) / static_cast<float>(m_Window.GetWindowHeight())),
	m_pSwapChain(m_DXGIManager.CreateSwapChain(m_RenderDevice.GetGraphicsQueue(), m_Window, SwapChainBufferFormat, NumSwapChainBuffers))
{
}

RenderEngine::~RenderEngine()
{
}

void RenderEngine::Update(const Time& Time)
{

}

void RenderEngine::Render()
{

}

void RenderEngine::Present()
{

}