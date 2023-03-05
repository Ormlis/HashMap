#include <memory>
#include <algorithm>
#include <vector>

template<class KeyType, class ValueType, class Hash = std::hash<KeyType>>
class HashMap {
    using TableType = std::vector<std::unique_ptr<std::pair<const KeyType, ValueType>>>;
    using TableIterator = typename TableType::iterator;
    using ConstTableIterator = typename TableType::const_iterator;

public:
    using SizeType = size_t;
    using PositionType = size_t;

    template<typename TDataType, typename TIterator>
    class HashMapIterator {
    public:
        using value_type = TDataType;  // NOLINT

        explicit HashMapIterator(TIterator it, TIterator end) : iterator_(it), end_(end) {
        }

        HashMapIterator() : iterator_(), end_() {
        }

        HashMapIterator &operator=(HashMapIterator other) {
            iterator_ = other.iterator_;
            end_ = other.end_;
            return *this;
        }

        HashMapIterator &operator++() {
            ++iterator_;
            while (iterator_ != end_ && !(*iterator_)) ++iterator_;
            return *this;
        }

        HashMapIterator operator++(int) {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        HashMapIterator &operator--() {
            --iterator_;
            while (iterator_ != end_ && !(*iterator_)) --iterator_;
            return *this;
        }

        HashMapIterator operator--(int) {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        TDataType &operator*() const {
            return *(*iterator_);
        }

        TDataType *operator->() const {
            return (*iterator_).get();
        }

        bool operator==(const HashMapIterator &other) const {
            return iterator_ == other.iterator_ && end_ == other.end_;
        }

        bool operator!=(const HashMapIterator &other) const {
            return iterator_ != other.iterator_ || end_ != other.end_;
        }

    protected:
        TIterator iterator_;
        TIterator end_;
    };

    using iterator = HashMapIterator<std::pair<const KeyType, ValueType>, TableIterator>;
    using const_iterator = HashMapIterator<const std::pair<const KeyType, ValueType>, ConstTableIterator>;


    HashMap(Hash hasher = Hash()) : hasher_(hasher), size_(0) {
        capacity_ = 1;
        table_.resize(1);
        from_.resize(1);
    };

    template<typename Iterator>
    HashMap(Iterator begin, Iterator end, Hash hasher = Hash()) : HashMap(hasher) {
        for (auto it = begin; it != end; ++it) {
            insert(*it);
        }
    }


    HashMap(std::initializer_list<std::pair<KeyType, ValueType>> list, Hash hasher = Hash()) : HashMap(list.begin(),
                                                                                                       list.end(),
                                                                                                       hasher) {
    }

    HashMap &operator=(const HashMap &other) {
        hasher_ = other.hash_function();
        std::vector<std::pair<KeyType, ValueType>> values;
        for (auto &p: other) values.push_back(p);
        clear();
        for (auto &p: values) insert(p);
        return *this;
    }

    HashMap(const HashMap &other) : HashMap(other.begin(), other.end(), other.hash_function()) {
    }

    SizeType size() const {
        return size_;
    }

    bool empty() const {
        return size() == 0;
    }

    Hash hash_function() const {
        return hasher_;
    }

    const_iterator begin() const {
        auto it = const_iterator(table_.begin(), table_.end());
        if (!table_[0]) it++;
        return it;
    }

    const_iterator end() const {
        return const_iterator(table_.end(), table_.end());
    }

    iterator begin() {
        auto it = iterator(table_.begin(), table_.end());
        if (!table_[0]) it++;
        return it;
    }

    iterator end() {
        return iterator(table_.end(), table_.end());
    }

    iterator find(KeyType key) {
        PositionType h = FindPosition(key);
        return iterator(h != capacity_ ? (table_.begin() + h) : table_.end(), table_.end());
    }

    const_iterator find(KeyType key) const {
        PositionType h = FindPosition(key);
        return const_iterator(h != capacity_ ? (table_.begin() + h) : table_.end(), table_.end());
    }

    ValueType &operator[](KeyType key) {
        auto it = find(key);
        if (it == end()) {
            std::pair<KeyType, ValueType> elem;
            elem.first = key;
            insert(elem);
        }
        return find(key)->second;
    }

    const ValueType &at(KeyType key) const {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range("Key not found");
        }
        return it->second;
    }

    void clear() {
        size_ = 0;
        capacity_ = 1;
        table_.resize(capacity_);
        table_[0] = nullptr;
        from_.resize(capacity_);
        BLOCK_SIZE = DEFAULT_BLOCK_SIZE;
    }

    void insert(std::pair<KeyType, ValueType> elem) {
        if (find(elem.first) != end()) return;
        auto ptr = std::make_unique<std::pair<const KeyType, ValueType>>(elem);
        while (true) {
            PositionType h = GetBucket(ptr->first);
            PositionType h2 = h;
            PositionType diff = 0;
            while (diff < BLOCK_SIZE && ptr) {
                if (!table_[h2] || from_[h2] < diff) {
                    std::swap(table_[h2], ptr);
                    std::swap(from_[h2], diff);
                }
                diff++;
                h2 = GetNext(h2);
            }
            if (!ptr) {
                size_++;
                return;
            }
            Rebuild();
        }
    }

    void erase(KeyType key) {
        PositionType h = FindPosition(key);
        if (h == capacity_) {
            return; // Key is not exist
        }
        size_--;
        table_[h] = nullptr;
    }


private:
    static const size_t STANDARD_DIFF = 5;
    const size_t DEFAULT_BLOCK_SIZE = 64;
    size_t BLOCK_SIZE = DEFAULT_BLOCK_SIZE;

    Hash hasher_;
    SizeType size_;
    SizeType capacity_;

    TableType table_;
    std::vector<PositionType> from_;

    PositionType GetBucket(const KeyType &key) const {
        return hasher_(key) & (capacity_ - 1);
    }

    inline PositionType GetNext(PositionType h) const {
        return (h + 1) & (capacity_ - 1);
    }

    PositionType FindPosition(const KeyType &key) const {
        PositionType h = GetBucket(key);
        for (size_t it = 0; it < BLOCK_SIZE; ++it, h = GetNext(h)) {
            if (table_[h] && table_[h]->first == key) {
                return h;
            }
        }
        return capacity_;
    }

    void Rebuild() {
        TableType values;
        for (auto &it_elem: table_) {
            if (it_elem) {
                values.push_back(std::move(it_elem));
            }
        }
        capacity_ = 1;
        while (size_ * STANDARD_DIFF > capacity_) capacity_ <<= 1;
        table_.resize(capacity_);
        from_.resize(capacity_);
        std::vector<size_t> cnt(capacity_);
        for (auto &elem: values) {
            ++cnt[GetBucket(elem->first)];
        }
        BLOCK_SIZE = std::max(DEFAULT_BLOCK_SIZE, 2 * (*std::max_element(cnt.begin(), cnt.end())));
        size_ = 0;
        for (auto &p: values) {
            insert(*p);
        }
    }
};
