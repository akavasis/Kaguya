#ifndef MATH_HLSLI
#define MATH_HLSLI

static const float g_EPSILON = 1e-4f;

static const float g_PI = 3.141592654f;
static const float g_2PI = 6.283185307f;
static const float g_1DIVPI = 0.318309886f;
static const float g_1DIV2PI = 0.159154943f;
static const float g_1DIV4PI = 0.079577471f;
static const float g_PIDIV2 = 1.570796327f;
static const float g_PIDIV4 = 0.785398163f;

static const float FLT_MAX = asfloat(0x7F7FFFFF);

void CoordinateSystem(float3 v1, out float3 v2, out float3 v3)
{
	if (abs(v1.x) > abs(v1.y))
	{
		v2 = float3(-v1.z, 0.0f, v1.x) / sqrt(v1.x * v1.x + v1.z * v1.z);
	}
	else
	{
		v2 = float3(0.0f, v1.z, -v1.y) / sqrt(v1.y * v1.y + v1.z * v1.z);
	}
	v3 = cross(v1, v2);
}

struct Frame
{
	float3 ToWorld(float3 v)
	{
		return s * v.x + t * v.y + n * v.z;
	}

	float3 ToLocal(float3 v)
	{
		return float3(dot(v, s), dot(v, t), dot(v, n));
	}
	
	// tangent, bitangent, normal
	float3 s;
	float3 t;
	float3 n;
};

Frame InitFrame(float3 n)
{
	Frame frame;
	frame.n = n;
	
	CoordinateSystem(frame.n, frame.s, frame.t);
	
	return frame;
}

#endif // MATH_HLSLI