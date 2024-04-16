#pragma once

#include <atomic>

namespace Iris {

	class RefCountedObject
	{
	public:
		virtual ~RefCountedObject() = default;

		void IncRefCount() const noexcept
		{
			++m_RefCount;
		}
		void DecRefCount() const noexcept
		{
			--m_RefCount;
		}

		uint32_t GetRefCount() const noexcept { return m_RefCount.load(); }

	private:
		mutable std::atomic<uint32_t> m_RefCount = 0;
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



		bool operator==(const Ref<T>& other) const
		{
			return m_Ptr == other.m_Ptr;
		}

		bool operator!=(const Ref<T>& other) const
		{
			return !(*this == other);
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
				m_Ptr->DecRefCount();

				if (m_Ptr->GetRefCount() == 0)
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
	};

	template<typename T, typename... Args>
	[[nodiscard]]
	Ref<T> CreateReferencedObject(Args&&... args)
	{
		return Ref<T>(new T(std::forward<Args>(args)...));
	}

}