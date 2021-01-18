#pragma once
#include <wrl/client.h>
#include <DXProgrammableCapture.h>
#include <pix3.h>

class PIXCapture
{
public:
	PIXCapture();
	~PIXCapture();
private:
	Microsoft::WRL::ComPtr<IDXGraphicsAnalysis> GraphicsAnalysis;
};

#ifdef _DEBUG
#define GetScopedCaptureVariableName(a, b) PIXConcatenate(a, b)
#define PIXScopedCapture() PIXCapture GetScopedCaptureVariableName(pixCapture, __LINE__)
#else
#define ScopedCapture(context, ...)
#endif