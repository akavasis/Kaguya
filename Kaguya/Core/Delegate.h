#pragma once

template <typename T> class Delegate;

template<class R, class ...Args>
class Delegate<R(Args...)>
{
	using StubType = R(*)(void*, Args&&...);

	Delegate(void* const o, StubType const m) noexcept :
		m_pObject(o),
		m_pStub(m)
	{
	}
public:
	Delegate() :
		m_pObject(),
		m_pDeleter(),
		m_StoreSize()
	{
	}
	Delegate(const Delegate&) = default;
	Delegate& operator=(const Delegate&) = default;
	Delegate(Delegate&&) = default;
	Delegate& operator=(Delegate&&) = default;
	Delegate(std::nullptr_t) noexcept : Delegate() { }

	template <typename Callable, typename = typename std::enable_if<!std::is_same<Delegate, typename std::decay<Callable>::type>::value> ::type>
	Delegate(Callable&& callable)
		: m_pStore(operator new(sizeof(typename std::decay<Callable>::type)), FunctorDeleter<typename std::decay<Callable>::type>),
		m_StoreSize(sizeof(typename std::decay<Callable>::type))
	{
		using CallableType = typename std::decay<Callable>::type;
		new (m_pStore.get()) CallableType(std::forward<Callable>(callable));
		m_pObject = m_pStore.get();
		m_pStub = FunctorStub<CallableType>;
		m_pDeleter = DeleterStub<CallableType>;
	}

	template <typename Callable, typename = typename std::enable_if<!std::is_same<Delegate, typename std::decay<Callable>::type>::value>::type>
	Delegate& operator=(Callable&& callable)
	{
		using CallableType = typename std::decay<Callable>::type;

		// Note that use_count is an approximation in multithreaded environments.
		if ((sizeof(CallableType) > m_StoreSize) || m_pStore.use_count() != 1)
		{
			m_pStore.reset(operator new(sizeof(CallableType)), FunctorDeleter<CallableType>);
			m_StoreSize = sizeof(CallableType);
		}
		else
		{
			m_pDeleter(m_pStore.get());
		}
		new (m_pStore.get()) CallableType(std::forward<Callable>(callable));
		m_pObject = m_pStore.get();
		m_pStub = FunctorStub<CallableType>;
		m_pDeleter = DeleterStub<CallableType>;
		return *this;
	}

	// No assignment operator overload for this constructor because assignment operator only takes 1 arg
	template <class T>
	Delegate(T* pObject, R(T::* pMethod)(Args...))
	{
		*this = Bind(pObject, pMethod);
	}

	// No assignment operator overload for this constructor because assignment operator only takes 1 arg
	template <class T>
	Delegate(T* pObject, R(T::* pMethod)(Args...) const)
	{
		*this = Bind(pObject, pMethod);
	}

	template <R(*pFunction)(Args...)>
	static Delegate Bind() noexcept
	{
		return { nullptr, FunctionStub<pFunction> };
	}

	template <class T, R(T::* pMethod)(Args...)>
	static Delegate Bind(T* pObject) noexcept
	{
		return { pObject, MethodStub<T, pMethod> };
	}

	template <class C, R(C::* pMethod)(Args...) const>
	static Delegate Bind(const C* pObject) noexcept
	{
		return { const_cast<C*>(pObject), ConstMethodStub<C, pMethod> };
	}

	template <typename T>
	static Delegate Bind(T&& f)
	{
		return std::forward<T>(f);
	}

	static Delegate Bind(R(*pFunction)(Args...))
	{
		return pFunction;
	}

	bool operator==(const Delegate& rhs) const noexcept
	{
		return (m_pObject == rhs.m_pObject) && (m_pStub == rhs.m_pStub);
	}

	bool operator!=(const Delegate& rhs) const noexcept
	{
		return !(*this == rhs);
	}

	bool operator==(std::nullptr_t) const noexcept
	{
		return !m_pStub;
	}

	bool operator!=(std::nullptr_t) const noexcept
	{
		return m_pStub;
	}

	operator bool() const noexcept
	{
		return m_pStub;
	}

	R operator()(Args&&... args) const
	{
		return (*m_pStub)(m_pObject, std::forward<Args>(args)...);
	}
private:
	using DeleterType = void(*)(void*);

	void* m_pObject = nullptr;
	StubType m_pStub = nullptr;
	std::shared_ptr<void> m_pStore = nullptr;
	DeleterType m_pDeleter = nullptr;
	std::size_t m_StoreSize = 0;

	template <class T>
	static void FunctorDeleter(void* pObject)
	{
		static_cast<T*>(pObject)->~T();
		operator delete(pObject);
	}

	template <class T>
	static void DeleterStub(void* pObject)
	{
		static_cast<T*>(pObject)->~T();
	}

	template <R(*pFunction)(Args...)>
	static R FunctionStub(void*, Args&&... args)
	{
		return pFunction(std::forward<Args>(args)...);
	}

	template <class T, R(T::* pMethod)(Args...)>
	static R MethodStub(void* pObject, Args&&... args)
	{
		return (static_cast<T*>(pObject)->*pMethod)(std::forward<Args>(args)...);
	}

	template <class T, R(T::* pMethod)(Args...) const>
	static R ConstMethodStub(void* pObject, Args&&... args)
	{
		return (static_cast<const T*>(pObject)->*pMethod)(std::forward<Args>(args)...);
	}

	template <typename T>
	struct is_member_pair : std::false_type { };

	template <class T>
	struct is_member_pair<std::pair<T* const, R(T::*)(Args...)>> : std::true_type { };

	template <typename>
	struct is_const_member_pair : std::false_type { };

	template <class T>
	struct is_const_member_pair<std::pair<const T*, R(T::*)(Args...) const>> : std::true_type { };

	template <typename T>
	static typename std::enable_if<!(is_member_pair<T>::value || is_const_member_pair<T>::value), R>::type
		FunctorStub(void* pObject, Args&&... args)
	{
		return (*static_cast<T*>(pObject))(std::forward<Args>(args)...);
	}

	template <typename T>
	static typename std::enable_if<is_member_pair<T>::value || is_const_member_pair<T>::value, R>::type
		FunctorStub(void* pObject, Args&&... args)
	{
		return (static_cast<T*>(pObject)->first->*static_cast<T*>(pObject)->second)(std::forward<Args>(args)...);
	}
};