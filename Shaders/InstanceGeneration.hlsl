typedef uint64_t D3D12_GPU_VIRTUAL_ADDRESS;

struct D3D12_RAYTRACING_INSTANCE_DESC
{
    float Transform[ 3 ][ 4 ];
    uint InstanceID	: 24;
    uint InstanceMask	: 8;
    uint InstanceContributionToHitGroupIndex	: 24;
    uint Flags	: 8;
    D3D12_GPU_VIRTUAL_ADDRESS AccelerationStructure;
};