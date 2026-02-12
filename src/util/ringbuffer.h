#pragma once

#include <memory>
#include <cassert>
#include <stdexcept>
#include <iterator>
#include <utility>


template<typename BufferType, typename ValueType>
    class ring_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = ValueType;
        using difference_type = std::ptrdiff_t;
        using pointer = ValueType *;
        using reference = ValueType &;

        ring_iterator(BufferType *buf, size_t index) : buf_(buf), index_(index) {
        }

        reference operator*() const {
            return (*buf_)[index_];
        }

        pointer operator->() const {
            return &(**this);
        }

        ring_iterator &operator++() {
            ++index_;
            return *this;
        }

        ring_iterator operator++(int) {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        ring_iterator &operator--() {
            --index_;
            return *this;
        }

        ring_iterator operator--(int) {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        ring_iterator &operator+=(difference_type n) {
            index_ += n;
            return *this;
        }

        ring_iterator &operator-=(difference_type n) {
            index_ -= n;
            return *this;
        }

        ring_iterator operator+(difference_type n) const {
            return ring_iterator(buf_, index_ + n);
        }

        ring_iterator operator-(difference_type n) const {
            return ring_iterator(buf_, index_ - n);
        }

        difference_type operator-(const ring_iterator &other) const {
            return index_ - other.index_;
        }

        reference operator[](difference_type n) const {
            return (*buf_)[index_ + n];
        }

        bool operator==(const ring_iterator &other) const {
            return index_ == other.index_;
        }

        bool operator!=(const ring_iterator &other) const {
            return index_ != other.index_;
        }

        bool operator<(const ring_iterator &other) const {
            return index_ < other.index_;
        }

        bool operator>(const ring_iterator &other) const {
            return index_ > other.index_;
        }

        bool operator<=(const ring_iterator &other) const {
            return index_ <= other.index_;
        }

        bool operator>=(const ring_iterator &other) const {
            return index_ >= other.index_;
        }

    private:
        BufferType *buf_;
        size_t index_;
    };


template<typename T, typename Alloc = std::allocator<T>>
    class ringbuffer {
    public:
        using value_type = T;
        using allocator_type = Alloc;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;

        using reference = value_type &;
        using const_reference = const value_type &;
        using pointer = std::allocator_traits<Alloc>::pointer;
        using const_pointer = std::allocator_traits<Alloc>::const_pointer;

        using iterator = ring_iterator<ringbuffer, T>;
        using const_iterator = ring_iterator<const ringbuffer, const T>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        explicit ringbuffer(size_type capacity = 8)
                : capacity_(round_up(capacity)),
                  head_(0),
                  tail_(0),
                  size_(0),
                  alloc_(),
                  data_(alloc_.allocate(capacity_)) {
        }

        ~ringbuffer() {
            clear();
            alloc_.deallocate(data_, capacity_);
        }

        ringbuffer(const ringbuffer &other)
                : capacity_(other.capacity_),
                  head_(0),
                  tail_(other.size_),
                  size_(other.size_),
                  alloc_(std::allocator_traits<Alloc>::select_on_container_copy_construction(other.alloc_)),
                  data_(alloc_.allocate(capacity_)) {

            for (size_type i = 0; i < size_; ++i)
                construct_at(i, other[i]);
        }

        ringbuffer(ringbuffer &&other) noexcept
                : capacity_(other.capacity_),
                  head_(other.head_),
                  tail_(other.tail_),
                  size_(other.size_),
                  alloc_(std::move(other.alloc_)),
                  data_(other.data_) {
            other.data_ = nullptr;
            other.capacity_ = 0;
            other.head_ = other.tail_ = other.size_ = 0;
        }

        ringbuffer &operator=(const ringbuffer &other) {
            if (this == &other)
                return *this;

            if constexpr (std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value) {
                if (alloc_ != other.alloc_) {
                    clear();
                    alloc_.deallocate(data_, capacity_);
                    alloc_ = other.alloc_;
                    data_ = alloc_.allocate(other.capacity_);
                    capacity_ = other.capacity_;
                }
            }
            else if (capacity_ < other.size_) {
                clear();
                alloc_.deallocate(data_, capacity_);
                data_ = alloc_.allocate(other.capacity_);
                capacity_ = other.capacity_;
            }

            clear();
            size_ = other.size_;
            head_ = 0;
            tail_ = size_;
            for (size_type i = 0; i < size_; ++i)
                construct_at(i, other[i]);

            return *this;
        }

        ringbuffer &operator=(ringbuffer &&other) noexcept {
            if (this == &other)
                return *this;

            clear();
            alloc_.deallocate(data_, capacity_);

            data_ = other.data_;
            capacity_ = other.capacity_;
            head_ = other.head_;
            tail_ = other.tail_;
            size_ = other.size_;
            alloc_ = std::move(other.alloc_);

            other.data_ = nullptr;
            other.capacity_ = 0;
            other.head_ = other.tail_ = other.size_ = 0;

            return *this;
        }

        reference front() {
            return data_[head_];
        }

        reference back() {
            return data_[(tail_ + capacity_ - 1) % capacity_];
        }

        void push_back(const_reference val) {
            ensure_capacity();
            construct_at(tail_, val);
            tail_ = (tail_ + 1) % capacity_;
            ++size_;
        }

        void push_back(T &&val) {
            ensure_capacity();
            construct_at(tail_, std::move(val));
            tail_ = (tail_ + 1) % capacity_;
            ++size_;
        }

        void push_front(const_reference val) {
            ensure_capacity();
            head_ = (head_ + capacity_ - 1) % capacity_;
            construct_at(head_, val);
            ++size_;
        }

        void push_front(T &&val) {
            ensure_capacity();
            head_ = (head_ + capacity_ - 1) % capacity_;
            construct_at(head_, std::move(val));
            ++size_;
        }

        void pop_front() {
            destroy_at(head_);
            head_ = (head_ + 1) % capacity_;
            --size_;
        }

        void pop_back() {
            tail_ = (tail_ + capacity_ - 1) % capacity_;
            destroy_at(tail_);
            --size_;
        }

        iterator insert(iterator pos, const_reference value) {
            return insert_impl(pos.index_, value);
        }

        iterator insert(iterator pos, T &&value) {
            return insert_impl(pos.index_, std::move(value));
        }

        template<typename U>
            iterator insert_impl(size_type logical_index, U &&value) {
                ensure_capacity();
                size_type phys_index = (head_ + logical_index) % capacity_;

                if (logical_index < size_ / 2) {
                    // shift left
                    head_ = (head_ + capacity_ - 1) % capacity_;
                    for (size_type i = 0; i < logical_index; ++i) {
                        size_type from = (head_ + i + 1) % capacity_;
                        size_type to = (head_ + i) % capacity_;
                        construct_at(to, std::move(data_[from]));
                        destroy_at(from);
                    }
                    construct_at((head_ + logical_index) % capacity_, std::forward<U>(value));
                }
                else {
                    // shift right
                    for (size_type i = size_; i > logical_index; --i) {
                        size_type from = (head_ + i - 1) % capacity_;
                        size_type to = (head_ + i) % capacity_;
                        construct_at(to, std::move(data_[from]));
                        destroy_at(from);
                    }
                    construct_at(phys_index, std::forward<U>(value));
                    tail_ = (tail_ + 1) % capacity_;
                }

                ++size_;
                return iterator(this, logical_index);
            }

        iterator erase(iterator pos) {
            return erase(pos, pos + 1);
        }

        iterator erase(iterator first, iterator last) {
            if (first == last)
                return first;

            size_type first_idx = first.index_;
            size_type last_idx = last.index_;
            size_type count = last_idx - first_idx;

            if (first_idx < size_ / 2) {
                // shift left
                for (size_type i = first_idx; i > 0; --i) {
                    size_type from = (head_ + i - 1) % capacity_;
                    size_type to = (head_ + i - 1 + count) % capacity_;
                    destroy_at(to);
                    construct_at(to, std::move(data_[from]));
                    destroy_at(from);
                }
                head_ = (head_ + count) % capacity_;
            }
            else {
                // shift right
                for (size_type i = last_idx; i < size_; ++i) {
                    size_type from = (head_ + i) % capacity_;
                    size_type to = (head_ + i - count) % capacity_;
                    destroy_at(to);
                    construct_at(to, std::move(data_[from]));
                    destroy_at(from);
                }
                tail_ = (tail_ + capacity_ - count) % capacity_;
            }

            size_ -= count;
            return iterator(this, first_idx);
        }

        reference operator[](size_type idx) {
            assert(idx < size_);
            return data_[(head_ + idx) % capacity_];
        }

        const_reference operator[](size_type idx) const {
            assert(idx < size_);
            return data_[(head_ + idx) % capacity_];
        }

        [[nodiscard]] size_type size() const noexcept {
            return size_;
        }

        [[nodiscard]] bool empty() const noexcept {
            return size_ == 0;
        }

        void clear() {
            for (size_type i = 0; i < size_; ++i)
                destroy_at((head_ + i) % capacity_);
            head_ = tail_ = size_ = 0;
        }

        iterator begin() {
            return iterator(this, 0);
        }

        iterator end() {
            return iterator(this, size_);
        }

        const_iterator begin() const {
            return const_iterator(this, 0);
        }

        const_iterator end() const {
            return const_iterator(this, size_);
        }

        const_iterator cbegin() const {
            return begin();
        }

        const_iterator cend() const {
            return end();
        }

    private:
        size_type capacity_, head_, tail_, size_;
        Alloc alloc_;
        T *data_;

        [[gnu::always_inline]]
        void ensure_capacity() {
            if (size_ < capacity_)
                return;
            grow();
        }

        void grow() {
            size_type new_cap = capacity_ * 2;
            T *new_data = alloc_.allocate(new_cap);

            for (size_type i = 0; i < size_; ++i)
                construct_at(new_data + i, std::move(operator[](i)));

            for (size_type i = 0; i < size_; ++i)
                destroy_at((head_ + i) % capacity_);

            alloc_.deallocate(data_, capacity_);
            data_ = new_data;
            capacity_ = new_cap;
            head_ = 0;
            tail_ = size_;
        }

        template<typename... Args>
            void construct_at(size_type idx, Args &&... args) {
                std::allocator_traits<Alloc>::construct(alloc_, data_ + idx, std::forward<Args>(args)...);
            }

        template<typename... Args>
            void construct_at(T *ptr, Args &&... args) {
                std::allocator_traits<Alloc>::construct(alloc_, ptr, std::forward<Args>(args)...);
            }

        void destroy_at(size_type idx) {
            std::allocator_traits<Alloc>::destroy(alloc_, data_ + idx);
        }

        static constexpr size_type round_up(size_type n) {
            if (n <= 8)
                return 8;
            --n;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            if constexpr (sizeof(size_type) > 4)
                n |= n >> 32;

            return n + 1;
        }
    };
