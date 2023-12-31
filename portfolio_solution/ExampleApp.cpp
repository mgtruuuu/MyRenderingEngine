#include "ExampleApp.h"

#include <DirectXCollision.h> // 구와 광선 충돌 계산에 사용
#include <directxtk/DDSTextureLoader.h>
#include <directxtk/SimpleMath.h>
#include <tuple>
#include <vector>

#include "GeometryGenerator.h"
#include "GraphicsCommon.h"

namespace Moon {

using namespace std;
using namespace DirectX;
using namespace DirectX::SimpleMath;

ExampleApp::ExampleApp() : AppBase() {}

bool ExampleApp::Initialize() {

    if (!AppBase::Initialize()) {
        return false;
    }
        

    AppBase::InitCubemaps(L"../Assets/Textures/Cubemaps/HDRI/",
                          L"EveningSkyHDRI020B_4K_EnvHDR.dds",
                          L"EveningSkyHDRI020B_4K_SpecularHDR.dds",
                          L"EveningSkyHDRI020B_4K_DiffuseHDR.dds",
                          L"EveningSkyHDRI020B_4K_Brdf.dds");


    // 후처리용 화면 사각형
    {
        auto meshData = GeometryGenerator::MakeSquare();
        m_screenSquare =
            make_shared<Model>(m_device, m_context, vector{meshData});
    }

    // 환경 박스 초기화
    {
        auto skyboxMesh = GeometryGenerator::MakeBox(40.0f);
        std::reverse(skyboxMesh.indices.begin(), skyboxMesh.indices.end());
        m_skybox = make_shared<Model>(m_device, m_context, vector{skyboxMesh});
    }

    // 바닥(거울)
    {
        auto meshData = GeometryGenerator::MakeSquare(5.0);
        meshData.albedoTextureFilename =
            "../Assets/Textures/metallic-textured-background.jpg";
        m_ground = make_shared<Model>(m_device, m_context, vector{meshData});
        m_ground->m_materialConstsCPU.albedoFactor = Vector3(0.5f);
        m_ground->m_materialConstsCPU.emissionFactor = Vector3(0.0f);
        m_ground->m_materialConstsCPU.metallicFactor = 0.2f;
        m_ground->m_materialConstsCPU.roughnessFactor = 0.8f;

        Vector3 position = Vector3(0.0f, -0.5f, 2.0f);
        m_ground->UpdateWorldRow(Matrix::CreateRotationX(3.141592f * 0.5f) *
                                 Matrix::CreateTranslation(position));

        m_mirrorPlane = SimpleMath::Plane(position, Vector3(0.0f, 1.0f, 0.0f));
        m_mirror = m_ground; // 바닥에 거울처럼 반사 구현

        // m_basicList.push_back(m_ground); // 거울은 리스트에 등록 X
    }

    // Main Object
    {
        auto meshes = GeometryGenerator::ReadFromFile(
            "../Assets/Models/DamagedHelmet/", "DamagedHelmet.gltf");

        Vector3 center(0.0f, 0.4f, 2.0f);
        m_mainObj = make_shared<Model>(m_device, m_context, meshes);
        m_mainObj->m_materialConstsCPU.invertNormalMapY = true; // GLTF는 true로
        m_mainObj->m_materialConstsCPU.albedoFactor = Vector3(1.0f);
        m_mainObj->m_materialConstsCPU.roughnessFactor = 1.0f;
        m_mainObj->m_materialConstsCPU.metallicFactor = 1.0f;
        m_mainObj->UpdateWorldRow(Matrix::CreateTranslation(center));

        m_basicList.push_back(m_mainObj); // 리스트에 등록

        // 동일한 크기와 위치에 BoundingSphere 만들기
        m_mainBoundingSphere = BoundingSphere(center, 0.4f);
    }

    // 조명 설정
    {
        // 조명 0은 고정
        m_globalConstsCPU.lights[0].position = Vector3(0.0f, 1.5f, 2.0f);
        m_globalConstsCPU.lights[0].direction = Vector3(0.0f, -1.0f, 0.0f);
        m_globalConstsCPU.lights[0].type =
            LIGHT_POINT | LIGHT_SHADOW; // Spot with shadow

        // 조명 1의 위치와 방향은 Update()에서 설정
        m_globalConstsCPU.lights[1].radiance = Vector3(5.0f);
        m_globalConstsCPU.lights[1].spotPower = 6.0f;
        m_globalConstsCPU.lights[1].fallOffEnd = 20.0f;
        m_globalConstsCPU.lights[1].type =
            LIGHT_POINT | LIGHT_SHADOW; // Spot with shadow

        // 조명 2는 꺼놓음
        m_globalConstsCPU.lights[2].type = LIGHT_OFF;
    }

    // 조명 위치 표시
    {
        for (int i = 0; i < MAX_LIGHTS; i++) {
            MeshData sphere = GeometryGenerator::MakeSphere(0.01f, 10, 10);
            m_lightSphere[i] =
                make_shared<Model>(m_device, m_context, vector{sphere});
            m_lightSphere[i]->UpdateWorldRow(Matrix::CreateTranslation(
                m_globalConstsCPU.lights[i].position));
            m_lightSphere[i]->m_materialConstsCPU.albedoFactor = Vector3(0.0f);
            m_lightSphere[i]->m_materialConstsCPU.emissionFactor =
                Vector3(1.0f, 1.0f, 0.0f);

            if (m_globalConstsCPU.lights[i].type == 0)
                m_lightSphere[i]->m_isVisible = false;

            m_basicList.push_back(m_lightSphere[i]); // 리스트에 등록
        }
    }

    // 커서 표시 (Main sphere와의 충돌이 감지되면 월드 공간에 작게 그려지는 구)
    {
        MeshData sphere = GeometryGenerator::MakeSphere(0.01f, 10, 10);
        m_cursorSphere =
            make_shared<Model>(m_device, m_context, vector{sphere});
        m_cursorSphere->m_isVisible = false; // 마우스가 눌렸을 때만 보임
        m_cursorSphere->m_materialConstsCPU.albedoFactor = Vector3(0.0f);
        m_cursorSphere->m_materialConstsCPU.emissionFactor =
            Vector3(0.0f, 1.0f, 0.0f);

        m_basicList.push_back(m_cursorSphere); // 리스트에 등록
    }

    return true;
}

void ExampleApp::Update(float dt) {

    // 카메라의 이동
    m_camera.UpdateKeyboard(dt, m_keyPressed);

    // 반사 행렬 추가
    const Vector3 eyeWorld = m_camera.GetEyePos();
    const Matrix reflectRow = Matrix::CreateReflection(m_mirrorPlane);
    const Matrix viewRow = m_camera.GetViewRow();
    const Matrix projRow = m_camera.GetProjRow();

    // 조명 업데이트 (인덱스 1이 포인트라이트)
    static Vector3 lightDev = Vector3(0.8f, 0.0f, 0.0f);
    if (m_lightRotate) {
        lightDev = Vector3::Transform(
            lightDev, Matrix::CreateRotationY(dt * 3.141592f * 0.5f));
    }
    m_globalConstsCPU.lights[1].position = Vector3(0.0f, 1.0f, 2.0f) + lightDev;
    Vector3 focusPosition = Vector3(0.0f, 0.0f, 1.7f);
    m_globalConstsCPU.lights[1].direction =
        focusPosition - m_globalConstsCPU.lights[0].position;
    m_globalConstsCPU.lights[1].direction.Normalize();

    // 공용 ConstantBuffer 업데이트
    AppBase::UpdateGlobalConstants(eyeWorld, viewRow, projRow, reflectRow);

    // 거울은 따로 처리
    m_mirror->UpdateConstantBuffers(m_device, m_context);

    // 조명의 위치 반영
    for (int i = 0; i < MAX_LIGHTS; i++)
        m_lightSphere[i]->UpdateWorldRow(
            Matrix::CreateTranslation(m_globalConstsCPU.lights[i].position));

    // 마우스 이동/회전 반영
    if (m_leftButton || m_rightButton) {
        Quaternion q;
        Vector3 dragTranslation;
        Vector3 pickPoint;
        if (UpdateMouseControl(m_mainBoundingSphere, q, dragTranslation,
                               pickPoint)) {
            Vector3 translation = m_mainObj->m_worldRow.Translation();
            m_mainObj->m_worldRow.Translation(Vector3(0.0f));
            m_mainObj->UpdateWorldRow(
                m_mainObj->m_worldRow * Matrix::CreateFromQuaternion(q) *
                Matrix::CreateTranslation(dragTranslation + translation));
            m_mainBoundingSphere.Center = m_mainObj->m_worldRow.Translation();

            // 충돌 지점에 작은 구 그리기
            m_cursorSphere->m_isVisible = true;
            m_cursorSphere->UpdateWorldRow(
                Matrix::CreateTranslation(pickPoint));
        } else {
            m_cursorSphere->m_isVisible = false;
        }
    } else {
        m_cursorSphere->m_isVisible = false;
    }

    for (auto &i : m_basicList) {
        i->UpdateConstantBuffers(m_device, m_context);
    }
}

void ExampleApp::Render() {

    AppBase::SetViewport();

    // 모든 샘플러들을 공통으로 사용 (뒤에서 더 추가됩니다.)
    m_context->VSSetSamplers(0, UINT(Graphics::sampleStates.size()),
                             Graphics::sampleStates.data());
    m_context->PSSetSamplers(0, UINT(Graphics::sampleStates.size()),
                             Graphics::sampleStates.data());

    // 공용 텍스춰들: "Common.hlsli"에서 register(t10)부터 시작
    vector<ID3D11ShaderResourceView *> commonSRVs = {
        m_envSRV.Get(), m_specularSRV.Get(), m_irradianceSRV.Get(),
        m_brdfSRV.Get()};
    m_context->PSSetShaderResources(10, UINT(commonSRVs.size()),
                                    commonSRVs.data());

    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    vector<ID3D11RenderTargetView *> rtvs = {m_floatRTV.Get()};

    // Depth Only Pass
    m_context->OMSetRenderTargets(
        1, m_resolvedRTV.GetAddressOf(),
        m_depthOnlyDSV
            .Get()); // Multi-Sampling을 사용하지 않기 위해서 m_resolvedRTV 사용
                     // (argument로 가지지만 실제로 사용되지않음)
                     // (NULL을 넣으면 그래픽스 디버거에서 경고 발생)
    m_context->ClearDepthStencilView(m_depthOnlyDSV.Get(), D3D11_CLEAR_DEPTH,
                                     1.0f, 0);
    AppBase::SetPipelineState(Graphics::defaultSolidPSO);
    AppBase::SetGlobalConsts(m_globalConstsGPU); // 비록 목적은 depthOnlyDSV만
                                                 // 변화시키는 거지만 렌더링필요
    for (auto &i : m_basicList)
        i->Render(m_context);
    m_skybox->Render(m_context);
    m_mirror->Render(m_context);

    // 거울 1. 거울은 빼고 원래 대로 그리기
    for (size_t i = 0; i < rtvs.size(); i++) {
        m_context->ClearRenderTargetView(rtvs[i], clearColor);
    }
    m_context->OMSetRenderTargets(UINT(rtvs.size()), rtvs.data(),
                                  m_depthStencilView.Get());
    m_context->ClearDepthStencilView(m_depthStencilView.Get(),
                                     D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                     1.0f, 0);
    AppBase::SetPipelineState(m_drawAsWire ? Graphics::defaultWirePSO
                                           : Graphics::defaultSolidPSO);
    AppBase::SetGlobalConsts(m_globalConstsGPU);

    for (auto &i : m_basicList) {
        i->Render(m_context);
    }

    AppBase::SetPipelineState(Graphics::normalsPSO);
    for (auto &i : m_basicList) {
        if (i->m_drawNormals)
            i->RenderNormals(m_context);
    }

    AppBase::SetPipelineState(m_drawAsWire ? Graphics::skyboxWirePSO
                                           : Graphics::skyboxSolidPSO);

    m_skybox->Render(m_context);

    // 거울 2. 거울 위치만 StencilBuffer에 1로 표기
    AppBase::SetPipelineState(Graphics::stencilMaskPSO);

    m_mirror->Render(m_context);

    // 거울 3. 거울 위치에 반사된 물체들을 렌더링
    AppBase::SetPipelineState(m_drawAsWire ? Graphics::reflectWirePSO
                                           : Graphics::reflectSolidPSO);
    AppBase::SetGlobalConsts(m_reflectGlobalConstsGPU);

    m_context->ClearDepthStencilView(m_depthStencilView.Get(),
                                     D3D11_CLEAR_DEPTH, 1.0f, 0);

    for (auto &i : m_basicList) {
        i->Render(m_context);
    }

    AppBase::SetPipelineState(m_drawAsWire ? Graphics::reflectSkyboxWirePSO
                                           : Graphics::reflectSkyboxSolidPSO);
    m_skybox->Render(m_context);

    // 거울 4. 거울 자체의 재질을 "Blend"로 그림
    AppBase::SetPipelineState(m_drawAsWire ? Graphics::mirrorBlendWirePSO
                                           : Graphics::mirrorBlendSolidPSO);
    AppBase::SetGlobalConsts(m_globalConstsGPU);

    m_mirror->Render(m_context);

    m_context->ResolveSubresource(m_resolvedBuffer.Get(), 0,
                                  m_floatBuffer.Get(), 0,
                                  DXGI_FORMAT_R16G16B16A16_FLOAT);

    // PostEffects
    AppBase::SetPipelineState(Graphics::postEffectsPSO);
    vector<ID3D11ShaderResourceView *> postEffectsSRVs = {
        m_resolvedSRV.Get(), m_depthOnlySRV.Get()}; // 20번에 넣어줌
    m_context->PSSetShaderResources(20, UINT(postEffectsSRVs.size()),
                                    postEffectsSRVs.data());
    m_context->OMSetRenderTargets(1, m_postEffectsRTV.GetAddressOf(), NULL);
    m_context->PSSetConstantBuffers(3, 1,
                                    m_postEffectsConstsGPU.GetAddressOf());
    m_screenSquare->Render(m_context);

    // 단순 이미지 처리와 블룸
    AppBase::SetPipelineState(Graphics::postProcessingPSO);
    m_postProcess.Render(m_context);
}

void ExampleApp::UpdateGUI() {

    ImGui::SetNextItemOpen(false, ImGuiCond_Once);
    if (ImGui::TreeNode("General")) {
        ImGui::Checkbox("Use FPV", &m_camera.m_useFirstPersonView);
        ImGui::Checkbox("Wireframe", &m_drawAsWire);
        if (ImGui::Checkbox("MSAA ON", &m_useMSAA)) {
            CreateBuffers();
        }
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Skybox")) {
        ImGui::SliderFloat("Strength", &m_globalConstsCPU.strengthIBL, 0.0f,
                           5.0f);
        ImGui::RadioButton("Env", &m_globalConstsCPU.textureToDraw, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Specular", &m_globalConstsCPU.textureToDraw, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Irradiance", &m_globalConstsCPU.textureToDraw, 2);
        ImGui::SliderFloat("EnvLodBias", &m_globalConstsCPU.envLodBias, 0.0f,
                           10.0f);
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Post Effects")) {
        int flag = 0;
        flag += ImGui::RadioButton("Render", &m_postEffectsConstsCPU.mode, 1);
        ImGui::SameLine();
        flag += ImGui::RadioButton("Depth", &m_postEffectsConstsCPU.mode, 2);
        flag += ImGui::SliderFloat(
            "DepthScale", &m_postEffectsConstsCPU.depthScale, 0.0, 1.0);
        flag += ImGui::SliderFloat("Fog", &m_postEffectsConstsCPU.fogStrength,
                                   0.0, 1.0);

        if (flag)
            D3D11Utils::UpdateBuffer(m_device, m_context,
                                     m_postEffectsConstsCPU,
                                     m_postEffectsConstsGPU);

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Post Processing")) {
        int flag = 0;
        flag += ImGui::SliderFloat(
            "Bloom Strength",
            &m_postProcess.m_combineFilter.m_constData.strength, 0.0f, 1.0f);
        flag += ImGui::SliderFloat(
            "Exposure", &m_postProcess.m_combineFilter.m_constData.option1,
            0.0f, 10.0f);
        flag += ImGui::SliderFloat(
            "Gamma", &m_postProcess.m_combineFilter.m_constData.option2, 0.1f,
            5.0f);
        // 편의상 사용자 입력이 인식되면 바로 GPU 버퍼를 업데이트
        if (flag) {
            m_postProcess.m_combineFilter.UpdateConstantBuffers(m_device,
                                                                m_context);
        }
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Mirror")) {

        ImGui::SliderFloat("Alpha", &m_mirrorAlpha, 0.0f, 1.0f);
        const float blendColor[4] = {m_mirrorAlpha, m_mirrorAlpha,
                                     m_mirrorAlpha, 1.0f};
        if (m_drawAsWire)
            Graphics::mirrorBlendWirePSO.SetBlendFactor(blendColor);
        else
            Graphics::mirrorBlendSolidPSO.SetBlendFactor(blendColor);

        ImGui::SliderFloat("Metallic",
                           &m_mirror->m_materialConstsCPU.metallicFactor, 0.0f,
                           1.0f);
        ImGui::SliderFloat("Roughness",
                           &m_mirror->m_materialConstsCPU.roughnessFactor, 0.0f,
                           1.0f);

        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Light")) {
        ImGui::SliderFloat3("Position", &m_globalConstsCPU.lights[0].position.x,
                            -5.0f, 5.0f);
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Material")) {
        ImGui::SliderFloat("LodBias", &m_globalConstsCPU.lodBias, 0.0f, 10.0f);

        int flag = 0;

        flag += ImGui::SliderFloat(
            "Metallic", &m_mainObj->m_materialConstsCPU.metallicFactor, 0.0f,
            1.0f);
        flag += ImGui::SliderFloat(
            "Roughness", &m_mainObj->m_materialConstsCPU.roughnessFactor, 0.0f,
            1.0f);
        flag += ImGui::CheckboxFlags(
            "AlbedoTexture", &m_mainObj->m_materialConstsCPU.useAlbedoMap, 1);
        flag += ImGui::CheckboxFlags(
            "EmissiveTexture", &m_mainObj->m_materialConstsCPU.useEmissiveMap,
            1);
        flag += ImGui::CheckboxFlags(
            "Use NormalMapping", &m_mainObj->m_materialConstsCPU.useNormalMap,
            1);
        flag += ImGui::CheckboxFlags(
            "Use AO", &m_mainObj->m_materialConstsCPU.useAOMap, 1);
        flag += ImGui::CheckboxFlags(
            "Use HeightMapping", &m_mainObj->m_meshConstsCPU.useHeightMap, 1);
        flag += ImGui::SliderFloat(
            "HeightScale", &m_mainObj->m_meshConstsCPU.heightScale, 0.0f, 0.1f);
        flag += ImGui::CheckboxFlags(
            "Use MetallicMap", &m_mainObj->m_materialConstsCPU.useMetallicMap,
            1);
        flag += ImGui::CheckboxFlags(
            "Use RoughnessMap", &m_mainObj->m_materialConstsCPU.useRoughnessMap,
            1);

        if (flag) {
            m_mainObj->UpdateConstantBuffers(m_device, m_context);
        }

        ImGui::Checkbox("Draw Normals", &m_mainObj->m_drawNormals);

        ImGui::TreePop();
    }
}

} // namespace hlab
