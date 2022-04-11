#pragma once

#include <DirectXMath.h>
#include <DirectXColors.h>

class Vertex {
public:
	Vertex() = default;
	Vertex(
		float px, float py, float pz,
		float nx, float ny, float nz) :
		mPosition(px, py, pz)
		//mNormal(nx, ny, nz)
	{ }
	void SetColor(DirectX::XMVECTORF32 c) {
		mColor = DirectX::XMFLOAT4(c);
	}
	void SetPosition(DirectX::XMFLOAT3 position) {
		mPosition = position;
	}
	void Scale(double ratio) {
		mPosition.x *= ratio;
		mPosition.y *= ratio;
		mPosition.z *= ratio;
	}
private:
	DirectX::XMFLOAT3 mPosition;
//	DirectX::XMFLOAT3 mNormal;
	DirectX::XMFLOAT4 mColor;
};