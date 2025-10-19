#pragma once

#include <memory_resource>
#include <memory>
#include <cstddef>
#include <utility>

// Custom deleter for arena-allocated objects (does nothing)
struct ArenaDeleter {
    template<typename T>
    void operator()(T*) const noexcept {
        // Do nothing - arena owns the memory
    }
};

// Helper alias for arena-allocated unique_ptrs
template<typename T>
using arena_ptr = std::unique_ptr<T, ArenaDeleter>;

/**
 * AST Arena - Fast memory allocator for AST nodes
 * 
 * Uses a monotonic buffer resource to allocate AST nodes efficiently.
 * Benefits:
 * - No malloc overhead per node (batch allocation)
 * - Better cache locality (nodes allocated sequentially)
 * - Instant cleanup (reset entire buffer at once)
 * - No individual node destruction needed
 * 
 * Usage:
 *   ASTArena arena;
 *   auto* field = arena.create<Field>(name, args, directives);
 *   auto* op = arena.create<OperationDefinition>(type, name, vars);
 *   // ... use nodes ...
 *   arena.reset(); // Fast cleanup!
 */
class ASTArena {
public:
    // Start with 1MB buffer (adjustable based on typical query size)
    explicit ASTArena(size_t initial_size = 1024 * 1024)
        : buffer_(initial_size)
        , allocator_(&buffer_) {
    }
    
    // Disable copy (arena owns memory)
    ASTArena(const ASTArena&) = delete;
    ASTArena& operator=(const ASTArena&) = delete;
    
    // Allow move
    ASTArena(ASTArena&&) = default;
    ASTArena& operator=(ASTArena&&) = default;
    
    /**
     * Create an object of type T in the arena
     * 
     * @param args Constructor arguments for T
     * @return Raw pointer to newly constructed T
     * 
     * Note: Returned pointer is valid until arena.reset() is called.
     *       Do NOT delete the returned pointer - arena owns it.
     */
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        // Use allocate() with typed allocator (C++17 compatible)
        std::pmr::polymorphic_allocator<T> typed_allocator{&buffer_};
        T* mem = typed_allocator.allocate(1);
        
        // Construct object in-place
        return new(mem) T(std::forward<Args>(args)...);
    }
    
    /**
     * Allocate raw memory for an array of T
     * 
     * @param count Number of elements
     * @return Pointer to allocated array (uninitialized)
     */
    template<typename T>
    T* allocate_array(size_t count) {
        // Use allocate() with typed allocator (C++17 compatible)
        std::pmr::polymorphic_allocator<T> typed_allocator{&buffer_};
        return typed_allocator.allocate(count);
    }
    
    /**
     * Reset the arena, invalidating all previously allocated objects
     * 
     * This is O(1) and doesn't call destructors (objects must be trivially destructible
     * or not rely on destructors for cleanup).
     */
    void reset() {
        buffer_.release();
    }
    
    /**
     * Get the total bytes allocated so far
     */
    size_t bytes_allocated() const {
        // Note: monotonic_buffer_resource doesn't expose this, 
        // so we'd need to track it manually if needed
        return 0; // Placeholder
    }
    
private:
    std::pmr::monotonic_buffer_resource buffer_;
    std::pmr::polymorphic_allocator<std::byte> allocator_;
};

/**
 * RAII wrapper for arena-allocated unique_ptr equivalent
 * 
 * This allows drop-in replacement for std::unique_ptr<T> but uses arena allocation.
 * The pointer does NOT own the memory (arena does), but provides unique_ptr-like interface.
 */
template<typename T>
class ArenaPtr {
public:
    ArenaPtr() : ptr_(nullptr) {}
    explicit ArenaPtr(T* ptr) : ptr_(ptr) {}
    
    // Move only
    ArenaPtr(ArenaPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    ArenaPtr& operator=(ArenaPtr&& other) noexcept {
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
        return *this;
    }
    
    // Disable copy
    ArenaPtr(const ArenaPtr&) = delete;
    ArenaPtr& operator=(const ArenaPtr&) = delete;
    
    // Smart pointer interface
    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }
    
    T& operator*() { return *ptr_; }
    const T& operator*() const { return *ptr_; }
    
    T* get() { return ptr_; }
    const T* get() const { return ptr_; }
    
    explicit operator bool() const { return ptr_ != nullptr; }
    
    // Release ownership (for std::unique_ptr compatibility)
    T* release() {
        T* tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }
    
private:
    T* ptr_;
};

/**
 * Helper to create ArenaPtr
 */
template<typename T, typename... Args>
ArenaPtr<T> make_arena_ptr(ASTArena& arena, Args&&... args) {
    return ArenaPtr<T>(arena.create<T>(std::forward<Args>(args)...));
}

