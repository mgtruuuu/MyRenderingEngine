#include "Common.hlsli" 

// t20에서부터 시작
Texture2D renderTex : register(t20);    // Rendering results
Texture2D depthOnlyTex : register(t21); // DepthOnly

cbuffer PostEffectsConstants : register(b3)
{
    int mode;   // 1: Rendered image, 2: DepthOnly
    float depthScale;
    float fogStrength;
};

struct SamplingPixelShaderInput
{
    float4 posProj : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

float4 TexcoordToView(float2 texcoord)
{
    float4 posProj;

    // [0, 1]x[0, 1] -> [-1, 1]x[-1, 1]
    posProj.xy = texcoord * 2.0 - 1.0;
    posProj.y *= -1; // 주의: y 방향을 뒤집어줘야 함
    posProj.z = depthOnlyTex.Sample(linearClampSampler, texcoord).r;
    posProj.w = 1.0;

    // ProjectSpace -> ViewSpace
    float4 posView = mul(posProj, invProj);
    posView.xyz /= posView.w;
    
    return posView;
}

float4 main(SamplingPixelShaderInput input) : SV_TARGET
{
    if (mode == 1)
    {
        // Beer-Lambert Law
        // fogStrength : extinction coefficient
        
        // dL_0(p, w) = -sigma_a(p,w) * L_i(p,-w) * dt, 
        // where sigma_a(p,w) : 얼마나 강하게 줄어드느냐, 
        // where L_i(p,-w) : 처음에 물체표면을 떠나는 빛의 양

        // I_0 : 물체가 반사해내는 빛의 강도
        // X : 물체와 눈의 거리 (안개를 지나는 거리)
        // I : 안개를 통과한 후에 감소하는 빛의 강도
        
        const float3 fogColor = float3(1.0, 1.0, 1.0);
        const float fogMin = 1.0;
        const float fogMax = 10.0;
        const float4 posView = TexcoordToView(input.texcoord);
        const float dist = length(posView.xyz); // 눈의 위치가 원점인 좌표계
        
        //const float dist = posView.z;
        const float distFog = saturate((dist - fogMin) / (fogMax - fogMin));
        const float fogFactor = exp(-distFog * fogStrength);
        
        float3 color = renderTex.Sample(linearClampSampler, input.texcoord).rgb;
        color = lerp(fogColor, color, fogFactor);   // (I / I_0) : 물체의 원래 색 대비 안개 통과 후 남은 비율
                                                    // 1 - (I / I_0) : 물체 색을 잃은 대신 얻은 안개의 색
                                                    // 안개 통과 후의 색 = 원래 색 * (I / I_0) + 안개색 * (1 - (I / I_0))
        return float4(color, 1.0);
    }
    else // if (mode == 2)
    {
        float z = TexcoordToView(input.texcoord).z * depthScale;
        return float4(z, z, z, 1);
    }
}
