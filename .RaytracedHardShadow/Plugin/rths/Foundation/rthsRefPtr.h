#pragma once

namespace rths {

template<class T> void addref(T *v) { v->addref(); }
template<class T> void release(T *v) { v->release(); }

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
            release<T>(m_ptr);
        m_ptr = data;
        if (m_ptr)
            addref<T>(m_ptr);
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

template<class T>
class RefCount
{
public:
    void addref()
    {
        ++ref_count;
    }

    void release()
    {
        if (--ref_count == 0)
            delete (T*)this;
    }

private:
    std::atomic_int ref_count = 1;
};

} // namespace rths
