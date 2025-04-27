#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>
#include <iterator>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    
    RawMemory(RawMemory&& other) noexcept {
        buffer_ = other.buffer_;
        capacity_ = other.capacity_;
        
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }
    
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs){
            buffer_ = rhs.buffer_;
            capacity_ = rhs.capacity_;
            
            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        
        return *this;
    }
    
    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }
    
    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept{
        return data_.GetAddress();
    }
    iterator end() noexcept{
        return data_ + size_;
    }
    const_iterator begin() const noexcept{
        return data_.GetAddress();
    }
    const_iterator end() const noexcept{
        return data_ + size_;
    }
    const_iterator cbegin() const noexcept{
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept{
        return data_ + size_;
    }
    
    Vector() = default;
    
    explicit Vector(size_t size) : size_(size), data_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other) : size_(other.size_), data_(other.Size())
    {
        
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(other.data_.GetAddress(), size_, data_.GetAddress());
        } else {
            std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
        }
    }
    
    Vector(Vector&& other) noexcept {
        data_ = std::move(other.data_);
        size_ = other.size_;
        other.size_ = 0;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector<T> rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                
                size_t min_of_copy = std::min(size_, rhs.size_);
                std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + min_of_copy, data_.GetAddress());
                
                if (size_ > rhs.size_){
                    
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                    
                } else {
                    
                    std::uninitialized_copy(rhs.data_ + size_, rhs.data_ + rhs.size_, data_ + size_);
                }
            }
        }
        
        size_ = rhs.size_;
        return *this;
    }
    
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs){
            data_ = std::move(rhs.data_);
            size_ = rhs.size_;
            rhs.size_ = 0;
        }
        
        return *this;
    }
    
    void Resize(size_t new_size){
        if (new_size != size_){
            
            if (new_size < size_){
                std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
                size_ = new_size;
                return;
            }
            
            if (new_size > size_){
                
                if (new_size > data_.Capacity()){
                    Reserve(new_size);
                }
                
                std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
                
                size_ = new_size;
                
                return;
            }
        }
    }
    
    
    void PushBack(const T& value){
        EmplaceBack(value);
    }
    
    void PushBack(T&& value){
        EmplaceBack(std::move(value));
    }
    
    void PopBack() noexcept {
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        size_--;
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args){
        
        if (size_ < data_.Capacity()){
            new (data_ + size_) T(std::forward<Args>(args)...);
        } else {
            
            size_t new_capacity = size_ == 0 ? 1 : data_.Capacity()*2;
            RawMemory<T> new_data(new_capacity);
            
            new (new_data + size_) T(std::forward<Args>(args)...);
            
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            
            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
        }
        
        ++size_;
        return data_[size_ - 1];
    }
    
    template <typename... Args>
    void EmplaceWithoutRealloc(size_t dist, Args&&... args){
        T copy_obj(std::forward<Args>(args)...);
        new (end()) T(std::move(data_[size_ - 1]));
        
        std::move_backward(begin() + dist, end() - 1, end());
        data_[dist] = std::move(copy_obj);
    }
    
    template <typename... Args>
    void EmplaceWithRealloc(size_t dist, Args&&... args){
        size_t size = size_ == 0 ? 1 : size_*2;
        RawMemory<T> new_data(size);

        new (new_data + dist) T(std::forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), dist, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), dist, new_data.GetAddress());
        }
        
        //копирование после места вставки
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress() + dist, size_ - dist, new_data.GetAddress() + dist + 1);
        } else {
            std::uninitialized_copy_n(data_.GetAddress() + dist, size_ - dist, new_data.GetAddress() + dist + 1);
        }
        
        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
        
    }
    
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){
        if (pos == end()){
            return &EmplaceBack(std::forward<Args>(args)...);
        }
        
        size_t dist = std::distance(cbegin(), pos);
        
        if (size_ < data_.Capacity()){
            EmplaceWithoutRealloc(dist, std::forward<Args>(args)...);
        } else {
            EmplaceWithRealloc(dist, std::forward<Args>(args)...);
        }
        
        ++size_;
        return begin() + dist;
    }
    
    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        
        size_t dist = std::distance(cbegin(), pos);
        std::move(begin() + dist + 1, end(), begin() + dist);
        
        data_[size_ - 1].~T();
        
        --size_;
        return begin() + dist;
    }
    
    
    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos, value);
    }
    
    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos, std::move(value));
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
             return;
         }
        
         RawMemory<T> new_data(new_capacity);
        
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        
         // Разрушаем элементы в data_
         std::destroy_n(data_.GetAddress(), size_);
         // Избавляемся от старой сырой памяти, обменивая её на новую
         data_.Swap(new_data);
         // При выходе из метода старая память будет возвращена в кучу
    }
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
private:
    size_t size_ = 0;
    RawMemory<T> data_;
};
