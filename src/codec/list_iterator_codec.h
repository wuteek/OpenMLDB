/*-------------------------------------------------------------------------
 * Copyright (C) 2019, 4paradigm
 * window.h
 *
 * Author: chenjing
 * Date: 2019/11/25
 *--------------------------------------------------------------------------
 **/

#ifndef SRC_CODEC_LIST_ITERATOR_CODEC_H_
#define SRC_CODEC_LIST_ITERATOR_CODEC_H_
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "codec/type_codec.h"
#include "glog/logging.h"
namespace fesql {
namespace codec {
class Row {
 public:
    Row() : buf_(nullptr), size_(0), need_free_(false) {}
    Row(int8_t *d, size_t n) : buf_(d), size_(n), need_free_(false) {}
    Row(int8_t *d, size_t n, bool need_free)
        : buf_(d), size_(n), need_free_(need_free) {}
    Row(const char *d, size_t n)
        : buf_(reinterpret_cast<int8_t *>(const_cast<char *>(d))),
          size_(n),
          need_free_(false) {}
    Row(const char *d, size_t n, bool need_free)
        : buf_(reinterpret_cast<int8_t *>(const_cast<char *>(d))),
          size_(n),
          need_free_(need_free) {}
    Row(Row &s) : buf_(s.buf()), size_(s.size()), need_free_(false) {}
    Row(const Row &s) : buf_(s.buf()), size_(s.size()), need_free_(false) {}
    Row(const Row &s, bool need_free)
        : buf_(s.buf()), size_(s.size()), need_free_(need_free) {}
    explicit Row(const std::string &s)
        : buf_(reinterpret_cast<int8_t *>(const_cast<char *>(s.data()))),
          size_(s.size()),
          need_free_(false) {}

    explicit Row(const char *s)
        : buf_(reinterpret_cast<int8_t *>(const_cast<char *>(s))),
          size_(strlen(s)),
          need_free_(false) {}
    virtual ~Row() {
        if (need_free_) {
            free(buf_);
        }
    }
    inline int8_t *buf() const { return buf_; }
    inline const char *data() const {
        return reinterpret_cast<const char *>(buf_);
    }
    inline int32_t size() const { return size_; }
    // Return true if the length of the referenced data is zero
    inline bool empty() const { return 0 == size_; }
    // Three-way comparison.  Returns value:
    //   <  0 iff "*this" <  "b",
    //   == 0 iff "*this" == "b",
    //   >  0 iff "*this" >  "b"
    int compare(const Row &b) const;
    int8_t *buf_;
    int32_t size_;
    bool need_free_;
};
inline int Row::compare(const Row &b) const {
    const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
    int r = memcmp(buf_, b.buf_, min_len);
    if (r == 0) {
        if (size_ < b.size_)
            r = -1;
        else if (size_ > b.size_)
            r = +1;
    }
    return r;
}

inline bool operator==(const Row &x, const Row &y) {
    return ((x.size() == y.size()) &&
            (memcmp(x.buf(), y.buf(), x.size()) == 0));
}

inline bool operator!=(const Row &x, const Row &y) { return !(x == y); }

template <class V>
class ArrayListIterator;

template <class V>
class ColumnImpl;

template <class V>
class ColumnIterator;

template <class K, class V>
class IteratorV {
 public:
    IteratorV() {}
    virtual ~IteratorV() {}
    virtual void Seek(K key) = 0;
    virtual void SeekToFirst() = 0;
    virtual bool Valid() = 0;
    virtual void Next() = 0;
    virtual const V &GetValue() = 0;
    virtual const K GetKey() = 0;
};

typedef IteratorV<uint64_t, Row> RowIterator;

class WindowIterator {
 public:
    WindowIterator() {}
    virtual ~WindowIterator() {}
    virtual void Seek(const std::string &key) = 0;
    virtual void SeekToFirst() = 0;
    virtual void Next() = 0;
    virtual bool Valid() = 0;
    virtual std::unique_ptr<RowIterator> GetValue() = 0;
    virtual const Row GetKey() = 0;
};

template <class V>
class ListV {
 public:
    ListV() {}
    virtual ~ListV() {}
    // TODO(chenjing): at 数组越界处理
    virtual std::unique_ptr<IteratorV<uint64_t, V>> GetIterator() const = 0;
    virtual IteratorV<uint64_t, V> *GetIterator(int8_t *addr) const = 0;
    virtual const uint64_t GetCount() = 0;
    virtual V At(uint64_t pos) = 0;
};

template <class V, class R>
class WrapListImpl : public ListV<V> {
 public:
    WrapListImpl() : ListV<V>() {}
    ~WrapListImpl() {}
    virtual const V GetField(R row) const = 0;
};

template <class V>
class ColumnImpl : public WrapListImpl<V, Row> {
 public:
    ColumnImpl(ListV<Row> *impl, uint32_t offset)
        : WrapListImpl<V, Row>(), root_(impl), offset_(offset) {}

    ~ColumnImpl() {}
    const V GetField(Row row) const override {
        V value;
        const int8_t *ptr = row.buf() + offset_;
        value = *((const V *)ptr);
        return value;
    }
    std::unique_ptr<IteratorV<uint64_t, V>> GetIterator() const override {
        auto iter = std::unique_ptr<IteratorV<uint64_t, V>>(
            new ColumnIterator<V>(root_, this));
        return std::move(iter);
    }
    IteratorV<uint64_t, V> *GetIterator(int8_t *addr) const override {
        if (nullptr == addr) {
            return new ColumnIterator<V>(root_, this);
        } else {
            return new (addr) ColumnIterator<V>(root_, this);
        }
    }
    const uint64_t GetCount() override { return root_->GetCount(); }
    V At(uint64_t pos) override { return GetField(root_->At(pos)); }

 private:
    ListV<Row> *root_;
    const uint32_t offset_;
};

class StringColumnImpl : public ColumnImpl<StringRef> {
 public:
    StringColumnImpl(ListV<Row> *impl, int32_t str_field_offset,
                     int32_t next_str_field_offset, int32_t str_start_offset)
        : ColumnImpl<StringRef>(impl, 0u),
          str_field_offset_(str_field_offset),
          next_str_field_offset_(next_str_field_offset),
          str_start_offset_(str_start_offset) {}

    ~StringColumnImpl() {}
    const StringRef GetField(Row row) const override {
        int32_t addr_space = v1::GetAddrSpace(row.size());
        StringRef value;
        v1::GetStrField(row.buf(), str_field_offset_, next_str_field_offset_,
                        str_start_offset_, addr_space,
                        reinterpret_cast<int8_t **>(&(value.data)),
                        &(value.size));
        return value;
    }

 private:
    uint32_t str_field_offset_;
    uint32_t next_str_field_offset_;
    uint32_t str_start_offset_;
};

template <class V>
class ArrayListV : public ListV<V> {
 public:
    ArrayListV() : start_(0), end_(0), buffer_(nullptr) {}
    explicit ArrayListV(std::vector<V> *buffer)
        : start_(0), end_(buffer->size()), buffer_(buffer) {}

    ArrayListV(std::vector<V> *buffer, uint32_t start, uint32_t end)
        : start_(start), end_(end), buffer_(buffer) {}

    ~ArrayListV() {}
    // TODO(chenjing): at 数组越界处理

    std::unique_ptr<IteratorV<uint64_t, V>> GetIterator() const override {
        return std::unique_ptr<ArrayListIterator<V>>(
            new ArrayListIterator<V>(buffer_, start_, end_));
    }
    IteratorV<uint64_t, V> *GetIterator(int8_t *addr) const override {
        if (nullptr == addr) {
            return new ArrayListIterator<V>(buffer_, start_, end_);
        } else {
            return new (addr) ArrayListIterator<V>(buffer_, start_, end_);
        }
    }
    virtual const uint64_t GetCount() { return end_ - start_; }
    virtual V At(uint64_t pos) { return buffer_->at(start_ + pos); }

 protected:
    uint64_t start_;
    uint64_t end_;
    std::vector<V> *buffer_;
};

template <class V>
class ArrayListIterator : public IteratorV<uint64_t, V> {
 public:
    explicit ArrayListIterator(const std::vector<V> *buffer,
                               const uint64_t start, const uint64_t end)
        : buffer_(buffer),
          iter_start_(buffer->cbegin() + start),
          iter_end_(buffer->cbegin() + end),
          iter_(iter_start_) {}

    explicit ArrayListIterator(const ArrayListIterator<V> &impl)
        : buffer_(impl.buffer_),
          iter_start_(impl.iter_start_),
          iter_end_(impl.iter_end_),
          iter_(impl.iter_start_) {}

    explicit ArrayListIterator(const ArrayListIterator<V> &impl, uint64_t start,
                               uint64_t end)
        : buffer_(impl.buffer_),
          iter_start_(impl.iter_start_ + start),
          iter_end_(impl.iter_start_ + end),
          iter_(iter_start_) {}

    ~ArrayListIterator() {}
    void Seek(uint64_t key) override {
        iter_ =
            (iter_start_ + key) >= iter_end_ ? iter_end_ : iter_start_ + key;
    }

    bool Valid() override { return iter_end_ != iter_; }

    void Next() override { iter_++; }

    const V &GetValue() override { return *iter_; }

    const uint64_t GetKey() override { return iter_ - iter_start_; }

    void SeekToFirst() { iter_ = iter_start_; }

    ArrayListIterator<V> *range(int start, int end) {
        if (start > end || end < iter_start_ || start > iter_end_) {
            return new ArrayListIterator(buffer_, iter_start_, iter_start_);
        }
        start = start < iter_start_ ? iter_start_ : start;
        end = end > iter_end_ ? iter_end_ : end;
        return new ArrayListIterator(buffer_, start, end);
    }

 protected:
    const std::vector<V> *buffer_;
    const typename std::vector<V>::const_iterator iter_start_;
    const typename std::vector<V>::const_iterator iter_end_;
    typename std::vector<V>::const_iterator iter_;
};

template <class V>
class ColumnIterator : public IteratorV<uint64_t, V> {
 public:
    ColumnIterator(ListV<Row> *list, const ColumnImpl<V> *column_impl)
        : IteratorV<uint64_t, V>(), column_impl_(column_impl) {
        row_iter_ = list->GetIterator();
    }
    ~ColumnIterator() {}
    void Seek(uint64_t key) override { row_iter_->Seek(key); }
    void SeekToFirst() override { row_iter_->SeekToFirst(); }
    bool Valid() override { return row_iter_->Valid(); }
    void Next() override { row_iter_->Next(); }
    const V &GetValue() override {
        value_ = column_impl_->GetField(row_iter_->GetValue());
        return value_;
    }
    const uint64_t GetKey() override { return row_iter_->GetKey(); }

 private:
    const ColumnImpl<V> *column_impl_;
    std::unique_ptr<IteratorV<uint64_t, Row>> row_iter_;
    V value_;
};

}  // namespace codec
}  // namespace fesql

#endif  // SRC_CODEC_LIST_ITERATOR_CODEC_H_
