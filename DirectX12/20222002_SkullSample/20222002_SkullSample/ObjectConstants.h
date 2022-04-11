#pragma once

#include <DirectXMath.h>

DirectX::XMFLOAT4X4 Identity4x4()
{
    static DirectX::XMFLOAT4X4 I(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    return I;
}


//struct ObjectConstants2
//{
//    ObjectConstants2() {
//        World = Identity4x4();
//        DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&World);
//        DirectX::XMStoreFloat4x4(&World, DirectX::XMMatrixTranspose(world));
//    }
//    DirectX::XMFLOAT4X4 World; 
//};