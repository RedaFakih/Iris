#include "IrisPCH.h"
#include "Font.h"

#include "AssetManager/AssetManager.h"
#include "MSDFData.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/UniformBufferSet.h"
#include "Utils/FileSystem.h"

namespace Iris {

#define DEFAULT_ANGLE_THRESHOLD 3.0f
#define DEFAULT_MITER_LIMIT 1.0f
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull
#define THREAD_COUNT 8

	// Taken from msdf_atlas_gen main.cpp
	struct FontInput
	{
		Buffer FontData;
		msdf_atlas::GlyphIdentifierType GlyphIdentifierType;
		const char* CharsetFilename;
		double FontScale;
		const char* FontName;
	};

	// Taken from msdf_atlas_gen main.cpp
	struct Configuration
	{
		msdf_atlas::ImageType ImageType;
		msdf_atlas::ImageFormat ImageFormat;
		msdf_atlas::YDirection YDirection;
		int Width;
		int Height;
		double EmSize;
		double PxRange;
		double AngleThreshold;
		double MiterLimit;
		void (*EdgeColoring)(msdfgen::Shape&, double, unsigned long long);
		bool ExpensiveColoring;
		unsigned long long ColoringSeed;
		msdf_atlas::GeneratorAttributes GeneratorAttributes;
	};

	struct AtlasHeader
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
	};

	namespace Utils {

		static std::filesystem::path GetCacheDirectory()
		{
			return "Resources/Fonts/Cache/FontAtlases";
		}

		static void CreateCacheDirectoryIfNeeded()
		{
			std::filesystem::path cacheDirectory = GetCacheDirectory();
			if (!std::filesystem::exists(cacheDirectory))
				std::filesystem::create_directories(cacheDirectory);
		}

		static bool TryGetCachedAtlas(std::string_view fontName, float fontSize, AtlasHeader& header, void*& pixels, Buffer& buffer)
		{
			std::string filename = fmt::format("{}_{}.Ifa", fontName, fontSize);
			std::filesystem::path filePath = GetCacheDirectory() / filename;

			if (!FileSystem::Exists(filePath))
				return false;

			buffer = FileSystem::ReadBytes(filePath);
			header = *reinterpret_cast<AtlasHeader*>(buffer.Data);
			pixels = reinterpret_cast<uint8_t*>(buffer.Data + sizeof(AtlasHeader));

			return true;
		}

		static void CacheFontAtlas(std::string_view fontName, float fontSize, AtlasHeader header, const void* pixels)
		{
			CreateCacheDirectoryIfNeeded();

			std::string filename = fmt::format("{}_{}.Ifa", fontName, fontSize);
			std::filesystem::path filePath = GetCacheDirectory() / filename;

			FileStreamWriter stream(filePath, true);
			if (!stream)
			{
				IR_CORE_ERROR_TAG("Renderer", "Failed to cache font atlas to {0}", filePath.string());
				return;
			}

			stream.WriteRaw<AtlasHeader>(header);
			stream.WriteData(reinterpret_cast<const uint8_t*>(pixels), header.Width * header.Height * sizeof(float) * 4);
		}

		template<typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFn>
		static Ref<Texture2D> CreateAndCacheAtlas(
			std::string_view fontName,
			float fontSize,
			const std::vector<msdf_atlas::GlyphGeometry>& glyphs,
			const msdf_atlas::FontGeometry& fontGeometry,
			const Configuration& config
		)
		{
			msdf_atlas::ImmediateAtlasGenerator<S, N, GenFn, msdf_atlas::BitmapAtlasStorage<T, N>> generator(config.Width, config.Height);
			generator.setAttributes(config.GeneratorAttributes);
			generator.setThreadCount(THREAD_COUNT);
			generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));

			msdfgen::BitmapConstRef<T, N> bitmap = static_cast<msdfgen::BitmapConstRef<T, N>>(generator.atlasStorage());

			AtlasHeader header = { static_cast<uint32_t>(bitmap.width), static_cast<uint32_t>(bitmap.height) };
			CacheFontAtlas(fontName, fontSize, header, bitmap.pixels);

			TextureSpecification spec = {
				.DebugName = "FontAtlas",
				.Width = header.Width,
				.Height = header.Height,
				.Format = ImageFormat::RGBA32F,
				.WrapMode = TextureWrap::Clamp,
				.GenerateMips = false
			};
			return Texture2D::Create(spec, Buffer(reinterpret_cast<const uint8_t*>(bitmap.pixels), header.Width * header.Height * sizeof(float) * 4));
		}

		static Ref<Texture2D> CreateCacheAtlas(AtlasHeader header, const void* pixels)
		{
			TextureSpecification spec = {
				.DebugName = "FontAtlas",
				.Width = header.Width,
				.Height = header.Height,
				.Format = ImageFormat::RGBA32F,
				.WrapMode = TextureWrap::Clamp,
				.GenerateMips = false
			};
			return Texture2D::Create(spec, Buffer(reinterpret_cast<const uint8_t*>(pixels), header.Width * header.Height * sizeof(float) * 4));
		}

	}

	Font::Font(const std::filesystem::path& filepath)
		: m_MSDFData(new MSDFData())
	{
		m_Name = filepath.stem().string();

		Buffer buffer = FileSystem::ReadBytes(filepath);
		CreateAtlas(buffer);
		buffer.Release();
	}

	Font::Font(const std::string_view name, Buffer buffer)
		: m_Name(name), m_MSDFData(new MSDFData())
	{
		CreateAtlas(buffer);
	}

	Font::~Font()
	{
		delete m_MSDFData;
	}

	void Font::Init()
	{
		s_DefaultFont = Font::Create("Resources/Fonts/opensans/OpenSans-Regular.ttf");
		s_DefaultMonoSpacedFont = Font::Create("Resources/Fonts/SourceCodePro/SourceCodePro-Medium.ttf");
	}

	void Font::Shutdown()
	{
		s_DefaultFont.Reset();
		s_DefaultMonoSpacedFont.Reset();
	}

	Ref<Font> Font::GetDefaultFont()
	{
		return s_DefaultFont;
	}

	Ref<Font> Font::GetDefaultMonoSpacedFont()
	{
		return s_DefaultMonoSpacedFont;
	}

	Ref<Font> Font::GetFontAssetForTextComponent(AssetHandle font)
	{
		if (font == s_DefaultFont->Handle || !AssetManager::IsAssetHandleValid(font))
		{
			return s_DefaultFont;
		}

		return AssetManager::GetAssetAsync<Font>(font);
	}

	void Font::CreateAtlas(Buffer buffer)
	{
		class FontHolder
		{
		public:
			FontHolder() : m_FT(msdfgen::initializeFreetype()), m_Font(nullptr) {}
			~FontHolder()
			{
				if (m_FT)
				{
					if (m_Font)
						msdfgen::destroyFont(m_Font);
					msdfgen::deinitializeFreetype(m_FT);
				}
			}

			bool load(Buffer buffer)
			{
				if (m_FT && buffer)
				{
					if (m_Font)
						msdfgen::destroyFont(m_Font);
					if ((m_Font = msdfgen::loadFontData(m_FT, reinterpret_cast<const msdfgen::byte*>(buffer.Data), static_cast<int>(buffer.Size))))
						return true;
				}
				return false;
			}

			operator msdfgen::FontHandle* () const
			{
				return m_Font;
			}

		private:
			msdfgen::FreetypeHandle* m_FT;
			msdfgen::FontHandle* m_Font;

		} font;

		FontInput fontInput = {
			.FontData = buffer,
			.GlyphIdentifierType = msdf_atlas::GlyphIdentifierType::UNICODE_CODEPOINT,
			.FontScale = -1
		};

		Configuration config = {
			.ImageType = msdf_atlas::ImageType::MTSDF,
			.ImageFormat = msdf_atlas::ImageFormat::BINARY_FLOAT,
			.YDirection = msdf_atlas::YDirection::BOTTOM_UP,
			.EmSize = 40,
			.AngleThreshold = DEFAULT_ANGLE_THRESHOLD,
			.MiterLimit = DEFAULT_MITER_LIMIT,
			.EdgeColoring = msdfgen::edgeColoringInkTrap,
			.GeneratorAttributes = {
				.config = msdfgen::MSDFGeneratorConfig(true),
				.scanlinePass = true
			}
		};

		bool anyCodePointsAvailable = false;
		bool success = font.load(fontInput.FontData);
		IR_ASSERT(success);

		if (fontInput.FontScale <= 0)
			fontInput.FontScale = 1;

		// From ImGui
		static const uint32_t charsetRanges[] =
		{
			0x0020, 0x00FF, // Basic Latin + Latin Supplement
			0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
			0x2DE0, 0x2DFF, // Cyrillic Extended-A
			0xA640, 0xA69F, // Cyrillic Extended-B
			0,
		};

		// Load character set
		msdf_atlas::Charset charset;
		for (int range = 0; range < 8; range += 2)
		{
			for (uint32_t c = charsetRanges[range]; c <= charsetRanges[range + 1]; c++)
				charset.add(c);
		}

		// Load glyphs
		m_MSDFData->FontGeometry = msdf_atlas::FontGeometry(&m_MSDFData->Glyphs);
		int glyphsLoaded = -1;
		switch (fontInput.GlyphIdentifierType)
		{
			case msdf_atlas::GlyphIdentifierType::GLYPH_INDEX:
				glyphsLoaded = m_MSDFData->FontGeometry.loadGlyphset(font, fontInput.FontScale, charset);
				break;
			case msdf_atlas::GlyphIdentifierType::UNICODE_CODEPOINT:
				glyphsLoaded = m_MSDFData->FontGeometry.loadCharset(font, fontInput.FontScale, charset);
				anyCodePointsAvailable |= glyphsLoaded > 0;
				break;
		}

		IR_ASSERT(glyphsLoaded > 0);
		IR_CORE_TRACE_TAG("Renderer", "Loaded geometry of {0} out of {1} glyphs", glyphsLoaded, static_cast<int>(charset.size()));
		// List missing glyphs
		if (glyphsLoaded < static_cast<int>(charset.size()))
			IR_CORE_WARN_TAG("Renderer", "Missing {0} {1}", static_cast<int>(charset.size()) - glyphsLoaded, fontInput.GlyphIdentifierType == msdf_atlas::GlyphIdentifierType::UNICODE_CODEPOINT ? "Codepoints" : "Glyphs");

		if (fontInput.FontName)
			m_MSDFData->FontGeometry.setName(fontInput.FontName);

		int fixedWidth = -1;
		int fixedHeight = -1;

		bool fixedDimensions = fixedWidth > 0 && fixedHeight > 0;
		bool fixedScale = config.EmSize > 0;

		msdf_atlas::TightAtlasPacker atlasPacker;
		if (fixedDimensions)
			atlasPacker.setDimensions(fixedWidth, fixedHeight);
		else
			atlasPacker.setDimensionsConstraint(msdf_atlas::TightAtlasPacker::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE);

		atlasPacker.setPadding(config.ImageType == msdf_atlas::ImageType::MSDF || config.ImageType == msdf_atlas::ImageType::MTSDF ? 0 : -1);

		if (fixedScale)
			atlasPacker.setScale(config.EmSize);
		else
			atlasPacker.setMinimumScale(0.0f);
		
		atlasPacker.setPixelRange(2.0);
		atlasPacker.setMiterLimit(config.MiterLimit);

		int remainingGlyphs = atlasPacker.pack(m_MSDFData->Glyphs.data(), static_cast<int>(m_MSDFData->Glyphs.size()));
		if (remainingGlyphs)
		{
			if (remainingGlyphs < 0)
			{
				IR_ASSERT(false);
			}
			else
			{
				IR_CORE_ERROR_TAG("Renderer", "Error: Could not fit {0} out of {1} glyphs into the atlas.", remainingGlyphs, static_cast<int>(m_MSDFData->Glyphs.size()));
				IR_ASSERT(false);
			}
		}

		atlasPacker.getDimensions(config.Width, config.Height);
		IR_ASSERT(config.Width > 0 && config.Height > 0);
		config.EmSize = atlasPacker.getScale();
		config.PxRange = atlasPacker.getPixelRange();
		if (!fixedScale)
			IR_CORE_TRACE_TAG("Renderer", "Glyph size: {0} pixels/EM", config.EmSize);
		if(!fixedDimensions)
			IR_CORE_TRACE_TAG("Renderer", "Atlas dimensions: {0} x {1}", config.Width, config.Height);

		// Edge coloring
		if (config.ImageType == msdf_atlas::ImageType::MSDF || config.ImageType == msdf_atlas::ImageType::MTSDF)
		{
			if (config.ExpensiveColoring)
			{
				msdf_atlas::Workload([&glyphs = m_MSDFData->Glyphs, &config](int i, int threadNo) -> bool
				{
					unsigned long long glyphSeed = (LCG_MULTIPLIER * (config.ColoringSeed ^ i) + LCG_INCREMENT) * config.ColoringSeed;
					glyphs[i].edgeColoring(config.EdgeColoring, config.AngleThreshold, glyphSeed);
					return true;
				}, static_cast<int>(m_MSDFData->Glyphs.size())).finish(THREAD_COUNT);
			}
			else
			{
				unsigned long long glyphSeed = config.ColoringSeed;
				for (msdf_atlas::GlyphGeometry& glyph : m_MSDFData->Glyphs)
				{
					glyphSeed *= LCG_MULTIPLIER;
					glyph.edgeColoring(config.EdgeColoring, config.AngleThreshold, glyphSeed);
				}
			}
		}

		// Check cache
		Buffer storageBuffer;
		AtlasHeader header;
		void* pixels;
		if (Utils::TryGetCachedAtlas(m_Name, static_cast<float>(config.EmSize), header, pixels, storageBuffer))
		{
			m_TextureAtlas = Utils::CreateCacheAtlas(header, pixels);
			storageBuffer.Release();
		}
		else
		{
			bool floatingPointFomrat = true;
			Ref<Texture2D> texture;

			switch (config.ImageType)
			{
				case msdf_atlas::ImageType::MSDF:
				{
					if (floatingPointFomrat)
						texture = Utils::CreateAndCacheAtlas<float, float, 3, msdf_atlas::msdfGenerator>(m_Name, static_cast<float>(config.EmSize), m_MSDFData->Glyphs, m_MSDFData->FontGeometry, config);
					else
						texture = Utils::CreateAndCacheAtlas<msdf_atlas::byte, float, 3, msdf_atlas::msdfGenerator>(m_Name, static_cast<float>(config.EmSize), m_MSDFData->Glyphs, m_MSDFData->FontGeometry, config);
					break;
				}
				case msdf_atlas::ImageType::MTSDF:
				{
					if (floatingPointFomrat)
						texture = Utils::CreateAndCacheAtlas<float, float, 4, msdf_atlas::mtsdfGenerator>(m_Name, static_cast<float>(config.EmSize), m_MSDFData->Glyphs, m_MSDFData->FontGeometry, config);
					else
						texture = Utils::CreateAndCacheAtlas<msdf_atlas::byte, float, 4, msdf_atlas::mtsdfGenerator>(m_Name, static_cast<float>(config.EmSize), m_MSDFData->Glyphs, m_MSDFData->FontGeometry, config);
					break;
				}
			}

			m_TextureAtlas = texture;
		}
	}

}
