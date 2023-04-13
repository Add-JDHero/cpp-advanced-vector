#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <type_traits>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }
    
    RawMemory(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }
    
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory& operator=(RawMemory&& other) noexcept {
        Swap(other);
        return *this;
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
    
    Vector() = default;
    
    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
        : data_(other.Size())
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }
    
    Vector(Vector&& other) noexcept
		: data_(std::move(other.data_))
		, size_(std::move(other.size_))
	{
		other.size_ = 0;
	}
    
    Vector& operator=(const Vector& other) {
        if (this != &other) {
            if (other.size_ > this->Capacity()) {
                Vector new_vector(other);
                this->Swap(new_vector);
            } else {
                size_t size = std::min(other.size_, size_);
                for(size_t i = 0; i < size; ++i) {
                    data_[i] = other.data_[i];
                }
                (other.size_ > size_) ? std::uninitialized_copy_n(other.data_.GetAddress() + size_, other.size_ - size_, data_.GetAddress() + size_) : std::destroy_n(data_.GetAddress() + other.size_, size_ - other.size_);
                size_ = other.size_;
            }
        }
        return *this;
    }
    
    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity > Capacity()) {
            RawMemory<T> new_data(new_capacity);
            ReplaceOrCopyData(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
        }
    }
    
    void Resize(size_t new_size) {
        if (new_size == size_) {
            return;
        } else if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        
        size_ = new_size;
    }
    
    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }
    
    /*void PushBack(T&& value) {
        if (data_.Capacity() > size_) {
            new (data_.GetAddress() + size_) T(std::forward<Type>(value));
        } else {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + size_) T(std::forward<Type>(value));
            
            ReplaceOrCopyData(data_.GetAddress(), size_, new_data.GetAddress());
            
            data_.Swap(new_data);
        }
        size_++;
    }*/
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        T* t = nullptr;
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            t = new (new_data + size_) T(std::forward<Args>(args)...);
            ReplaceOrCopyData(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
        } else {
            t = new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *t;
    }
    
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {

        if (pos == end()) {
            return &EmplaceBack(std::forward<Args>(args)...);
        }

        size_t index = pos - begin();
        if (size_ != Capacity()) {
            T temp_obj (std::forward<Args>(args)...);
            size_t last_el = size_ - 1;
            new (end()) T(std::move(data_[last_el]));
            std::move_backward(data_ + index, data_ + last_el, end());
            data_[index] = std::move(temp_obj);
        } else {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + index) T(std::forward<Args>(args)...);
            try {
                ReplaceOrCopyData(data_.GetAddress(), index, new_data.GetAddress());
            } catch (...) {
                /*
                * Если исключение выбросится при их копировании, 
                * нужно разрушить ранее вставленный элемент в обработчике
                */
                new_data[index].~T();
                throw;
            } try {
                //Затем копируются либо перемещаются элементы, которые следуют за вставляемым
                ReplaceOrCopyData(data_ + index, size_ - index, new_data + (index + 1));
            } catch (...) {
                std::destroy_n(new_data.GetAddress(), index + 1);
            }
            data_.Swap(new_data);
        }
        ++size_;
        return data_ + index;
    }
  
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos) {
        if (pos == end()) {
            PopBack();
            return end();
        }
        
        size_t index = pos - begin();
        std::move(data_ + index + 1, end(), data_ + index);
        PopBack();
        return data_ + index;
    }

    void PopBack() noexcept {
        data_[--size_].~T();
    }
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return (data_ + size_);
    }
    const_iterator begin() const noexcept  {
        return const_cast<Vector*>(this)->begin();
    }
    const_iterator end() const noexcept {
        return const_cast<Vector*>(this)->end();
    }
    const_iterator cbegin() const noexcept {
        return begin();
    }
    const_iterator cend() const noexcept {
        return end();
    }
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

private:
    void ReplaceOrCopyData(iterator old, size_t size, iterator new_it) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(old, size, new_it);
        } else {
            std::uninitialized_copy_n(old, size, new_it);
        }
        std::destroy_n(old, size);
    }
    
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }
    
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
    
    RawMemory<T> data_;
    size_t size_ = 0;
};
