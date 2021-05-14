#pragma once
#include <unordered_map>
#include <memory>
#include "Formats.h"

#if 1
#include "./D3D12/WIPD3D12.h"
#else

#endif

namespace WIP3D
{
	class Texture;
	class Buffer;
	class ParameterBlock;
	class ResourceViewInfo;

	//
	//
	class Resource : public std::enable_shared_from_this<Resource>
	{
	public:
		using ApiHandle = ResourceHandle;
		using BindFlags = ResourceBindFlags;

	};
}