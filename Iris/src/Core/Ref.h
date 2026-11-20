#pragma once

#include <atomic>

namespace Iris {

	/*
	 * ========================= Intrusive Reference Counting. =========================
	 * Classic Intrusive Reference Countring system where the object that wants to be reference counted, has to inherit from the RefCountedObject which holds an Atomic ref count.
	 * In Debug builds we keep a thread safe unordered_set of live references in order to provide better debugging information if their are any memory leaks on application shutdown.
	 * In Debug builds the unordered_set provides a better method to better validate weak ptr's reference validity by just looking the set for the wanted ref and if it exists then the weak ref is valid.
	 * 
	 * In Release builds the live referencing is stripped to gain performance by avoiding any mutex locking that comes with it.
	 *		- IMPORTANT: This makes it so that weak refs CAN NOT BE VALIDATED IN RELEASE BUILDS!!!!!
	 * 
	 * 
	 * 
	 * NOTE: Rewrite the Ref Counting System so that we would be able to properly validate weak refs while maintaining performance!
	 */

//#if IR_CONFIG_DEBUG

	namespace RefUtils {

		void AddToLiveReferences(void* instance);
		void RemoveFromLiveReferences(void* instance);
		bool IsLive(void* instance);
		void DumpLiveReferences();

	}

//#endif

	class RefCountedObject
	{
	public:
//#if IR_CONFIG_DEBUG
		RefCountedObject()
		{
			RefUtils::AddToLiveReferences(reinterpret_cast<void*>(this));
		}

		virtual ~RefCountedObject()
		{
			RefUtils::RemoveFromLiveReferences(reinterpret_cast<void*>(this));
		}
//#else
//		RefCountedObject() = default;
//		virtual ~RefCountedObject() = default;
//#endif

		void IncRefCount() const noexcept
		{
			++m_RefCount;
		}

		uint32_t DecRefCount() const noexcept
		{
			return m_RefCount.fetch_sub(1, std::memory_order_acq_rel);
		}

#if IR_CONFIG_DEBUG
		void SetName(const std::string& debugName)
		{
			m_DebugName = debugName;
		}
		const std::string& GetName() const { return m_DebugName; }
#endif

		uint32_t GetRefCount() const noexcept { return m_RefCount.load(); }

	private:
		mutable std::atomic<uint32_t> m_RefCount = 0;
#if IR_CONFIG_DEBUG
		std::string m_DebugName;
#endif

		template<typename T>
		friend class WeakRef;

	};

	template<typename T>
	class Ref
	{
	public:
		constexpr Ref() noexcept = default;
		constexpr Ref(std::nullptr_t) noexcept {}

		constexpr Ref(T* ptr) noexcept 
			: m_Ptr(ptr) 
		{
			static_assert(std::is_base_of<RefCountedObject, T>::value, "Object does not inherit from RefCountedObject");

			IncrementRef();
		}

		template<typename T2>
		Ref(const Ref<T2>& other) noexcept
			: m_Ptr(static_cast<T*>(other.m_Ptr))
		{
			IncrementRef();
		}

		template<typename T2>
		Ref(Ref<T2>&& other) noexcept
			: m_Ptr(static_cast<T*>(other.m_Ptr))
		{
			other.m_Ptr = nullptr;
		}

		Ref(const Ref<T>& other) noexcept
			: m_Ptr(other.m_Ptr)
		{
			IncrementRef();
		}

		Ref<T>& operator=(std::nullptr_t)
		{
			DecrementRef();
			m_Ptr = nullptr;
			return *this;
		}

		Ref<T>& operator=(const Ref<T>& other)
		{
			if (this == &other)
				return *this;

			other.IncrementRef();
			DecrementRef();

			m_Ptr = other.m_Ptr;
			return *this;
		}

		template<typename T2>
		Ref<T>& operator=(const Ref<T2>& other)
		{
			other.IncrementRef();
			DecrementRef();

			m_Ptr = other.m_Ptr;
			return *this;
		}

		template<typename T2>
		Ref<T>& operator=(Ref<T2>&& other)
		{
			DecrementRef();

			m_Ptr = other.m_Ptr;
			other.m_Ptr = nullptr;
			return *this;
		}

		~Ref()
		{
			DecrementRef();
		}

		operator bool() noexcept { return m_Ptr != nullptr; }
		operator bool() const noexcept { return m_Ptr != nullptr; }

		T* operator->() { return m_Ptr; }
		const T* operator->() const { return m_Ptr; }

		T& operator*() { return *m_Ptr; }
		const T& operator*() const { return *m_Ptr; }

		T* Raw() noexcept { return  m_Ptr; }
		const T* Raw() const noexcept { return  m_Ptr; }

		void Reset(T* ptr = nullptr)
		{
			DecrementRef();
			m_Ptr = ptr;
		}

		template<typename T2>
		Ref<T2> As() const
		{
			return Ref<T2>(*this);
		}

		bool operator==(const Ref<T>& other) const
		{
			return m_Ptr == other.m_Ptr;
		}

		bool operator!=(const Ref<T>& other) const
		{
			return !(*this == other);
		}

		bool EqualsObject(const Ref<T>& other)
		{
			if (!m_Ptr || !other.m_Ptr)
				return false;

			return *m_Ptr == *other.m_Ptr;
		}

	private:
		void IncrementRef() const noexcept
		{
			if (m_Ptr)
			{
				m_Ptr->IncRefCount();
			}
		}

		void DecrementRef() const
		{
			if (m_Ptr)
			{
				if (m_Ptr->DecRefCount() == 1)
				{
					delete m_Ptr;
					m_Ptr = nullptr;
				}
			}
		}

	private:
		mutable T* m_Ptr = nullptr;

		template<typename T2>
		friend class Ref;

		template<typename T2>
		friend class WeakRef;

	};

	template<typename T, typename... Args>
	[[nodiscard]]
	Ref<T> CreateReferencedObject(Args&&... args)
	{
		return Ref<T>(new T(std::forward<Args>(args)...));
	}

	template<typename T>
	class WeakRef
	{
	public:
		constexpr WeakRef() noexcept = default;

		constexpr WeakRef(Ref<T> ptr) noexcept
		{
			m_Ptr = ptr.Raw();
		}

		constexpr WeakRef(T* ptr) noexcept
		{
			m_Ptr = ptr;
		}

		Ref<T> Lock() const noexcept
		{
			if (!m_Ptr)
				return Ref<T>(nullptr);

			uint32_t strongRefCount = m_Ptr->m_RefCount.load(std::memory_order_relaxed);

			while (strongRefCount > 0)
			{
				if (m_Ptr->m_RefCount.compare_exchange_weak(strongRefCount, strongRefCount + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
				{
					// ponter gets set directly since the ref count has been increased in the if statement
					Ref<T> ref;
					ref.m_Ptr = m_Ptr;
					return ref;
				}
			}

			return Ref<T>(nullptr);
		}

		bool IsValid() const
		{
			// TODO(Reda): NOT REALLY A SOLUTION!! Need to rewrite the ref counting system to strip out the debugging set and have a better solution to maintain the ability to validate weak refs without losing performance
#if IR_CONFIG_DEBUG
			return m_Ptr ? RefUtils::IsLive(m_Ptr) : false;
#else
			return m_Ptr != nullptr;
#endif
		}

		template<typename T2>
		WeakRef<T2> As() const
		{
			return WeakRef<T2>(reinterpret_cast<T2*>(m_Ptr));
		}

		T* operator->() noexcept { return m_Ptr; }
		const T* operator->() const noexcept { return m_Ptr; }

		T& operator*() { return *m_Ptr; }
		const T& operator*() const { return *m_Ptr; }

		operator bool() const { return IsValid(); }

	private:
		T* m_Ptr = nullptr;

	};

}