#define IR_VMA_LOGS 0

#if IR_CONFIG_DEBUG

	#if IR_VMA_LOGS

		#define VMA_DEBUG_LOG_FORMAT(format, ...) do { printf((format), __VA_ARGS__); printf("\n"); } while (false)
		#define VMA_LEAK_LOG_FORMAT(format, ...) do { printf("VMA LEAK: " format "\n", __VA_ARGS__); } while (false)

	#endif

#endif

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"