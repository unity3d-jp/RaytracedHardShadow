#pragma once

namespace rths {

// simplified boost::intrusive_ptr equivalent
template<class T>
class ref_ptr
{
public:
    ref_ptr() {}
    ref_ptr(T *data) { reset(data); }
    ref_ptr(T&& data) { swap(data); }
    ref_ptr(const ref_ptr& v) { reset(v.m_ptr); }
    ref_ptr& operator=(const ref_ptr& v) { reset(v.m_ptr); return *this; }
    ~ref_ptr() { reset(); }
    void reset(T *data = nullptr)
    {
        if (m_ptr)
            m_ptr->internalRelease();
        m_ptr = data;
        if (m_ptr)
            m_ptr->internalAddref();
    }
    void swap(ref_ptr& v)
    {
        std::swap(m_ptr, v->m_data);
    }

    T& operator*() { return *m_ptr; }
    const T& operator*() const { return *m_ptr; }
    T* operator->() { return m_ptr; }
    const T* operator->() const { return m_ptr; }
    operator T*() { return m_ptr; }
    operator const T*() const { return m_ptr; }
    operator bool() const { return m_ptr; }
    bool operator==(const ref_ptr<T>& v) const { return m_ptr == v.m_ptr; }
    bool operator!=(const ref_ptr<T>& v) const { return m_ptr != v.m_ptr; }

private:
    T *m_ptr = nullptr;
};

template<class T> void ExternalRelease(T *self);

template<class T>
class RefCount
{
friend class ref_ptr<T>;
friend void ExternalRelease<T>(T *self);
protected:
    RefCount(T *self = nullptr) : m_self(self ? self : (T*)this) {}

    int internalAddref()
    {
        return ++m_ref_count;
    }

    int internalRelease()
    {
        if (--m_ref_count == 0) {
            delete m_self;
        }
        return m_ref_count;
    }

private:
    T *m_self;
    std::atomic_int m_ref_count = 1;
};

// resource type exposed to plugin user
template<class T>
class SharedResource : public RefCount<T>
{
using super = RefCount<T>;
public:
    SharedResource(T *self = nullptr) : super(self) {};
    bool operator==(const SharedResource& v) const { return id == v.id; }
    bool operator!=(const SharedResource& v) const { return id != v.id; }
    bool operator<(const SharedResource& v) const { return id < v.id; }

protected:
    static uint64_t newID()
    {
        static uint64_t s_id;
        return ++s_id;
    }

    uint64_t id = newID();
};

} // namespace rths
