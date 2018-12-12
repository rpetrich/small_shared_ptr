**Some design notes:**

1. It's impossible to create cycles via normal means since only constructors can create "filled" `small_shared_ptr`'s. Straightforward to add these back if they're desired, with the usual caution about reference counts and cycles.
2. move is zero-cost, copy is single atomic increment, and `small_shared_ptr` deref is zero-cost, but upgrading from a `small_weak_ptr` via `lock()` uses a CAS loop
3. Reference counting is single atomic increment, but only 32-bits of reference counts (should be enough for anyone!) before overflow
4. `small_shared_ptr` and `small_weak_ptr` are the size of a single pointer (half the size of shared_ptr and friends), but it's not possible to migrate an existing object into a `small_shared_ptr` without copying/moving on the value itself.
5. `shared_from_this` equivalent isn't implemented, but this isn't super necessary for most uses.
6. Only 8-byte overhead on the stored value.
7. Uses some nasty pointer and unioned atomics for _speed_. A less speedy implementation would just use `std::atomic<uint64_t>`. It's unclear if this is safe on all architectures, but is on x86_64 and ARMv8.