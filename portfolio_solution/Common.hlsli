#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

// 쉐이더에서 include할 내용들은 .hlsli 파일에 작성
// Properties -> Item Type: Does not participate in build으로 설정

#define MAX_LIGHTS 3    // 쉐이더에서도 #define 사용 가능
#define LIGHT_OFF 0x00
#define LIGHT_DIRECTIONAL 0x01
#define LIGHT_POINT 0x02
#define LIGHT_SPOT 0x03
#define LIGHT_SHADOW 0x10

// 샘플러들을 모든 쉐이더에서 공통으로 사용
SamplerState linearWrapSampler : register(s0);
SamplerState linearClampSampler : register(s1);

// 공용 텍스춰들 t10 부터 시작
TextureCube envIBLTex : register(t10);
TextureCube specularIBLTex : register(t11);
TextureCube irradianceIBLTex : register(t12);
Texture2D brdfTex : register(t13);

struct Light
{
    float3 radiance; // Strength
    float fallOffStart;
    float3 direction;
    float fallOffEnd;
    float3 position;
    float spotPower;
    
    uint type;
    float3 dummy;
};

// 공용 Constants
cbuffer GlobalConstants : register(b1)
{
    matrix view;
    matrix proj;
    Matrix invProj;
    matrix viewProj;
    float3 eyeWorld;
    float strengthIBL;

    int textureToDraw = 0;      // 0: Env, 1: Specular, 2: Irradiance, 그외: 검은색
    float envLodBias = 0.0f;    // 환경맵 LodBias
    float lodBias = 2.0f;       // 다른 물체들 LodBias
    float dummy2;
    
    Light lights[MAX_LIGHTS];
};

struct VertexShaderInput
{
    float3 posModel : POSITION;     //모델 좌표계의 위치 position
    float3 normalModel : NORMAL0;   // 모델 좌표계의 normal    
    float2 texcoord : TEXCOORD0;
    float3 tangentModel : TANGENT0;
};

struct PixelShaderInput
{
    float4 posProj : SV_POSITION;   // Screen position
    float3 posWorld : POSITION;     // World position (조명 계산에 사용)
    float3 normalWorld : NORMAL0;
    float2 texcoord : TEXCOORD0;
    float3 tangentWorld : TANGENT0;
};

#endif // __COMMON_HLSLI__