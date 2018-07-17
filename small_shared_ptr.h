#include <utility>
#include <atomic>
#include <stdlib.h>
#include <new>
#if defined(__APPLE__)
	#include <machine/endian.h>
#else
	#include <endian.h>
#endif

template <class T>
struct small_weak_ptr;

template <class T>
struct small_shared_ptr {
private:
	static const uint64_t strong_incr = (1ull << 32) + 1ull;

	struct buffer {
		union {
			std::atomic<uint64_t> state;
			struct {
#if __BYTE_ORDER == __BIG_ENDIAN
				std::atomic<uint32_t> strong;
				std::atomic<uint32_t> weak;
#else
				std::atomic<uint32_t> weak;
				std::atomic<uint32_t> strong;
#endif
			};
		};
		T value;
		template<typename... Args>
		buffer(Args&&... args) : state(strong_incr), value(std::forward<Args>(args)...) {
		}
		virtual void cleanup() {
			(&value)->~T();
		}
	};
	T* ptr;
	template<typename... Args>
	small_shared_ptr(buffer* buf) noexcept : ptr(&buf->value) {
	}

	buffer* get_buffer() const {
	    return (buffer*)((char*)ptr - (size_t)&(reinterpret_cast<buffer*>(0)->value));
	}

public:
	small_shared_ptr() noexcept : ptr(nullptr) {
	}
	small_shared_ptr(std::nullptr_t null) noexcept : ptr(nullptr) {
	}
	small_shared_ptr(small_shared_ptr<T>&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}
	small_shared_ptr(const small_shared_ptr<T>& other) noexcept : ptr(other.ptr) {
		if (ptr) {
			std::atomic_fetch_add_explicit(&get_buffer()->state, strong_incr, std::memory_order_relaxed);
		}
	}
	template <class U>
	small_shared_ptr(small_shared_ptr<U>&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}
	template <class U>
	small_shared_ptr(const small_shared_ptr<U>& other) noexcept : ptr(other.ptr) {
		if (ptr) {
			std::atomic_fetch_add_explicit(&get_buffer()->state, strong_incr, std::memory_order_relaxed);
		}
	}
	~small_shared_ptr() {
		*this = nullptr;
	}
	small_shared_ptr<T>& operator=(const small_shared_ptr<T>&) = delete;
	small_shared_ptr<T>& operator=(small_shared_ptr<T>&&) = delete;
	small_shared_ptr<T>& operator=(std::nullptr_t null) noexcept {
		if (ptr != nullptr) {
			auto ref = get_buffer();
			ptr = nullptr;
			auto new_state = std::atomic_fetch_sub_explicit(&ref->state, strong_incr, std::memory_order_acq_rel);
			if ((new_state >> 32) == 0) {
				try {
					ref->cleanup();
				} catch (...) {
					if (new_state == 0) {
						free(ref);
					}
					throw;
				}
			}
			if (new_state == 0) {
				free(ref);
			}
		}
		return *this;
	}
	T& operator*() const noexcept {
		return *get();
	}
	T* operator->() const noexcept {
		return get();
	}
	T* get() const noexcept {
		return static_cast<T*>(ptr);
	}
	long use_count() const noexcept {
		return ptr ? get_buffer()->strong : 0;
	}
	explicit operator bool() const noexcept {
		return ptr != nullptr;
	}
	template<typename U, typename... Args>
	friend small_shared_ptr<U> make_small_shared(Args&&... args);
	friend small_weak_ptr<T>;
};

template<class T, class U> inline bool operator==(const small_shared_ptr<T>& l, const small_shared_ptr<U>& r) noexcept {
    return l.get() == r.get();
}
template<class T, class U> inline bool operator!=(const small_shared_ptr<T>& l, const small_shared_ptr<U>& r) noexcept {
    return l.get() != r.get();
}
template<class T, class U> inline bool operator<=(const small_shared_ptr<T>& l, const small_shared_ptr<U>& r) noexcept {
    return l.get() <= r.get();
}
template<class T, class U> inline bool operator<(const small_shared_ptr<T>& l, const small_shared_ptr<U>& r) noexcept {
    return l.get() < r.get();
}
template<class T, class U> inline bool operator>=(const small_shared_ptr<T>& l, const small_shared_ptr<U>& r) noexcept {
    return l.get() >= r.get();
}
template<class T, class U> inline bool operator>(const small_shared_ptr<T>& l, const small_shared_ptr<U>& r) noexcept {
    return l.get() > r.get();
}

template <class T>
struct small_weak_ptr {
private:
	typename small_shared_ptr<T>::buffer* ptr;
public:
	small_weak_ptr() noexcept : ptr(nullptr) {
	}
	small_weak_ptr(std::nullptr_t null) noexcept : ptr(nullptr) {
	}
	small_weak_ptr(small_weak_ptr<T>&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}
	small_weak_ptr(const small_weak_ptr<T>& other) noexcept : ptr(other.ptr) {
		if (ptr) {
			std::atomic_fetch_add_explicit(&ptr->weak, static_cast<uint32_t>(1), std::memory_order_relaxed);
		}
	}
	template <class U>
	small_weak_ptr(const small_shared_ptr<U>& other) noexcept : ptr(other.get_buffer()) {
		if (ptr) {
			std::atomic_fetch_add_explicit(&ptr->weak, static_cast<uint32_t>(1), std::memory_order_relaxed);
		}
	}
	~small_weak_ptr() {
		*this = nullptr;
	}
	small_weak_ptr<T>& operator=(const small_weak_ptr<T>& other) noexcept {
		*this = nullptr;
		if (other.ptr && std::atomic_load_explicit(&other.ptr->strong, std::memory_order_relaxed)) {
			ptr = other.ptr;
			std::atomic_fetch_add_explicit(&other.ptr->weak, static_cast<uint32_t>(1), std::memory_order_relaxed);
		}
	}
	small_weak_ptr<T>& operator=(small_weak_ptr<T>&& other) noexcept {
		std::swap(other.ptr, ptr);
	}
	small_weak_ptr<T>& operator=(std::nullptr_t null) noexcept {
		if (ptr != nullptr) {
			if (std::atomic_fetch_add_explicit(&ptr->state, static_cast<uint64_t>(-1ull), std::memory_order_acq_rel) == 0) {
				free(ptr);
			}
		}
		return *this;
	}
	small_shared_ptr<T> lock() const noexcept {
		if (ptr) {
			for (;;) {
				auto strong = std::atomic_load_explicit(&ptr->strong, std::memory_order_relaxed);
				if (strong == 0) {
					break;
				}
				if (std::atomic_compare_exchange_weak_explicit(&ptr->strong, strong, strong + 1, std::memory_order_acq_rel)) {
					std::atomic_fetch_add(&ptr->weak, 1);
					return ptr;
				}
			}
		}
		return nullptr;
	}
	long use_count() const noexcept {
		return ptr ? std::atomic_load_explicit(&ptr->strong, std::memory_order_relaxed) : 0;
	}
	bool expired() const noexcept {
		return !ptr || std::atomic_load_explicit(&ptr->strong, std::memory_order_relaxed) == 0;
	}
};


template<typename T, typename... Args>
small_shared_ptr<T> make_small_shared(Args&&... args) {
	typename small_shared_ptr<T>::buffer* result = static_cast<typename small_shared_ptr<T>::buffer*>(malloc(sizeof(typename small_shared_ptr<T>::buffer)));
	if (!result) {
		throw std::bad_alloc();
	}
	try {
		new (result) typename small_shared_ptr<T>::buffer(std::forward<Args>(args)...);
	} catch(...) {
		free(result);
		throw;
	}
	return small_shared_ptr<T>(result);
}
