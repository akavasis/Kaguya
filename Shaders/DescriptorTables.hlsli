#ifndef __DESCRIPTOR_TABLES_HLSLI__
#define __DESCRIPTOR_TABLES_HLSLI__
Texture2D Tex2DTable[] : register(t0, space0);
Texture2DArray Tex2DArrayTable[] : register(t0, space1);
TextureCube TexCubeTable[] : register(t0, space2);
#endif