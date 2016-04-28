#pragma once


template<typename T, int InitialCapacity = 1024, int CapcityIncrement = 256>
class fhRenderList {
public:
	fhRenderList()
		: array((T*)R_FrameAlloc(sizeof(T)*InitialCapacity))
		, capacity(InitialCapacity)
		, size(0) {
	}

	void Append(const T& t) {
		assert(size < capacity);
		array[size] = t;
		++size;
	}

	void Clear() {
		size = 0;
	}

	int Num() const {
		return size;
	}

	bool IsEmpty() const {
		return Num() == 0;
	}

	const T& operator[](int i) const {
		assert(i < size);
		return array[i];
	}

private:
	T*  array;
	int capacity;
	int size;
};

struct drawDepth_t {
	const drawSurf_t*  surf;
	idImage*           texture;
	idVec4             textureMatrix[2];
	float              polygonOffset;
	bool               isSubView;	
	float              alphaTestThreshold;
	idVec4             color;
};

struct drawStage_t {
	const drawSurf_t* surf;
	const fhRenderProgram*  program;
	idImage*          textures[4];
	idCinematic*      cinematic;
	float             textureMatrix[16];
	bool              hasBumpMatrix;
	idVec4            bumpMatrix[2];
	depthBlendMode_t  depthBlendMode;
	float             depthBlendRange;
	stageVertexColor_t vertexColor;
	float             polygonOffset;
	idVec4            localViewOrigin;
	idVec4            diffuseColor;
	cullType_t        cullType;
	int               drawStateBits;
	idVec4            shaderparms[4];
	int               numShaderparms;
	fhVertexLayout    vertexLayout;
};

using DepthRenderList = fhRenderList<drawDepth_t>;
using StageRenderList = fhRenderList<drawStage_t>;