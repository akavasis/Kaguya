#ifndef SHARED_TYPES_HLSLI
#define SHARED_TYPES_HLSLI

// ==================== Material ====================
enum MaterialModel
{
	LambertianModel,
	GlossyModel,
	MetalModel,
	DielectricModel,
	DiffuseLightModel
};

enum TextureTypes
{
	AlbedoIdx,
	NormalIdx,
	RoughnessIdx,
	MetallicIdx,
	EmissiveIdx,
	NumTextureTypes
};

#define TEXTURE_CHANNEL_RED		0
#define TEXTURE_CHANNEL_GREEN	1
#define TEXTURE_CHANNEL_BLUE	2
#define TEXTURE_CHANNEL_ALPHA	3

struct Material
{
	float3	Albedo;
	float3	Emissive;
	float3	Specular;
	float3	Refraction;
	float	SpecularChance;
	float	Roughness;
	float	Metallic;
	float	Fuzziness;
	float	IndexOfRefraction;
	uint	Model;
	uint	UseAttributeAsValues; // If this is true, then the attributes above will be used rather than actual textures

	int		TextureIndices[NumTextureTypes];
	int		TextureChannel[NumTextureTypes];
};

// ==================== Mesh ====================
struct Mesh
{
	uint		VertexOffset;
	uint		IndexOffset;
	uint		MaterialIndex;
	uint		InstanceIDAndMask;
	uint		InstanceContributionToHitGroupIndexAndFlags;
	uint64_t	AccelerationStructure;
	matrix		World;
	matrix		PreviousWorld;
	float3x4	Transform;
};

// ==================== Camera ====================
struct Camera
{
	float	NearZ;
	float	FarZ;
	float	ExposureValue100;
	float	_padding1;

	float	FocalLength;
	float	RelativeAperture;
	float	ShutterTime;
	float	SensorSensitivity;

	float4	Position;
	float4	U;
	float4	V;
	float4	W;

	matrix	View;
	matrix	Projection;
	matrix	ViewProjection;
	matrix	InvView;
	matrix	InvProjection;
	matrix	InvViewProjection;

	RayDesc GenerateCameraRay(in float2 ndc, inout uint seed)
	{
		float3 direction = ndc.x * U.xyz + ndc.y * V.xyz + W.xyz;

		// Find the focal point for this pixel
		direction /= length(W); // Make ray have length 1 along the camera's w-axis.
		float3 focalPoint = Position.xyz + FocalLength * direction; // Select point on ray a distance FocalLength along the w-axis

		// Get random numbers (in polar coordinates), convert to random cartesian uv on the lens
		float2 rnd = float2(s_2PI * RandomFloat01(seed), RelativeAperture * RandomFloat01(seed));
		float2 uv = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);

		// Use uv coordinate to compute a random origin on the camera lens
		float3 origin = Position.xyz + uv.x * normalize(U.xyz) + uv.y * normalize(V.xyz);
		direction = normalize(focalPoint - origin);

		RayDesc ray = { origin, NearZ, direction, FarZ };
		return ray;
	}
};

// Physically-based camera from Moving Frostbite to PBR
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf

float ComputeEV100(float aperture, float shutterTime, float ISO)
{
    // EV number is defined as:
    // 2^ EV_s = N^2 / t and EV_s = EV_100 + log2 (S /100)
    // This gives
    // EV_s = log2 (N^2 / t)
    // EV_100 + log2 (S /100) = log2 (N^2 / t)
    // EV_100 = log2 (N^2 / t) - log2 (S /100)
    // EV_100 = log2 (N^2 / t . 100 / S)
	return log2((aperture * aperture) / shutterTime * 100 / ISO);
}

float ComputeEV100FromAvgLuminance(float avgLuminance)
{
    // We later use the middle gray at 12.7% in order to have
    // a middle gray at 18% with a sqrt (2) room for specular highlights
    // But here we deal with the spot meter measuring the middle gray
    // which is fixed at 12.5 for matching standard camera
    // constructor settings (i.e. calibration constant K = 12.5)
    // Reference: http://en.wikipedia.org/wiki/Film_speed
	return log2(avgLuminance * 100.0f / 12.5f);
}

float ComputeLuminousExposure(float aperture, float shutterTime)
{
	const float q = 0.65;
	float H = q * shutterTime / aperture * aperture;
	return H;
}

float ConvertEV100ToMaxHsbsLuminance(float EV100)
{
    // Compute the maximum luminance possible with H_sbs sensitivity.
    // Saturation Based Sensitivity: defined as the maximum possible exposure that does
    // not lead to a clipped or bloomed camera output

    // maxLum = 78 / ( S * q ) * N^2 / t
    // = 78 / ( S * q ) * 2^ EV_100
    // = 78 / (100 * 0.65) * 2^ EV_100
    // = 1.2 * 2^ EV
    // Reference: http://en.wikipedia.org/wiki/Film_speed
	float maxLuminance = 1.2f * pow(2.0f, EV100);

	return maxLuminance;
}

float3 ExposeLuminance(float3 luminance, Camera camera)
{
	float maxLuminance = ConvertEV100ToMaxHsbsLuminance(camera.ExposureValue100);
	return luminance / maxLuminance;
}

#endif // SHARED_TYPES_HLSLI