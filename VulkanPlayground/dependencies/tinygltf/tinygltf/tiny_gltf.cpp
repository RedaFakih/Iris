#ifndef TINYGLTF_IMPLEMENTATION
	#define TINYGLTF_IMPLEMENTATION
	#define TINYGLTF_USE_CPP14 // May result in better performance apparently...
	#define TINYGLTF_USE_RAPIDJSON
	#ifdef PLAYGROUND_RELEASE
		#define TINYGLTF_NOEXCEPTION
	#endif
#endif

#include "tiny_gltf.h"