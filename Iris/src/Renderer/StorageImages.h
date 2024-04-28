#pragma once

namespace Iris {

	/*
	 * Storage images are separated from the Texture class for a couple of reasons, mainly not wanting to crows the Texure class with alot of different
	 * types of images and having to handle that produces spaghetti code
	 * The usage of storage images is generally and most often going to be as follows:
	 *    - Compute shader filles storage image with data
	 *	  - Other shaders sample from that storage image so it just becomes a sampled image after
	 */

	class StorageImage
	{
	public:
	private:
	};

}