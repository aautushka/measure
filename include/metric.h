/*
MIT License

Copyright (c) 2018 Anton Autushka

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <sys/time.h>

namespace metric
{

class timer
{
public:
    using usec_t = unsigned long long;

    void start()
    {
        _elapsed = now() - _elapsed;
    }

    usec_t stop()
    {
        _elapsed = now() - _elapsed;
        return elapsed();
    }

    usec_t elapsed()
    {
        return _elapsed;
    }

    static usec_t now()
    {
        // TODO: std::chrono? performance!
        //      benchmark gettimeofday -- it's supposed to be fast and work in user-space
        //      consider TSC
        timeval time;
        gettimeofday(&time, nullptr);
        return usec(time);
    }

    static usec_t start(usec_t elapsed)
    {
        return now() - elapsed;
    }

    static usec_t stop(usec_t elapsed)
    {
        return now() - elapsed;
    }

private:
    static usec_t usec(timeval time)
    {
        return (usec_t)time.tv_sec * 1000 * 1000 + time.tv_usec;
    }

    usec_t _elapsed = 0;
};


class aggregate_timer
{
public:
    using usec_t = unsigned long long;

    void start()
    {
        _elapsed = now() - _elapsed;
    }

    usec_t stop()
    {
        ++_calls;
        _elapsed = now() - _elapsed;
        return elapsed();
    }

    usec_t elapsed()
    {
        return _elapsed;
    }

    static usec_t now()
    {
        // TODO: std::chrono? performance!
        //      benchmark gettimeofday -- it's supposed to be fast and work in user-space
        //      consider TSC
        timeval time;
        gettimeofday(&time, nullptr);
        return usec(time);
    }

private:

    static usec_t usec(timeval time)
    {
        return (usec_t)time.tv_sec * 1000 * 1000 + time.tv_usec;
    }

    usec_t _elapsed = 0;
    unsigned long _calls = 0;
};

template <typename T>
union pooled_object
{
    T obj;
    pooled_object* next;

    pooled_object() {}
    ~pooled_object() {}
};


template <typename T>
class heap_pool final
{
public:
    using size_type = std::size_t;

    heap_pool() = default; 
    heap_pool(heap_pool&&) = default;
    heap_pool& operator =(heap_pool&&) = default;

    ~heap_pool() noexcept
    {
        free_memory();
    }

    T* alloc() noexcept
    {
        if (flist_ == nullptr)
        {
            create_new_slab();

            if (!flist_)
            {
                return nullptr;
            }
        }

        auto ret = remove_free_list_head();
        ++size_;
        return ret;
    }

    void dealloc(T* t) noexcept
    {
        auto o = reinterpret_cast<object*>(t);
        o->next = flist_;
        flist_ = o;
        --size_;
    }

    template <typename ... Args>
    T* construct(Args&& ... args) 
    {
        T* o = alloc();
        if (o)
        {
            new (o) T(std::forward<Args>(args)...);
        }
        return o;
    }


    void destroy(T* t)
    {
        t->~T();
        dealloc(t);
    }

    heap_pool(const heap_pool&) = delete;
    heap_pool& operator =(const heap_pool&) = delete;

    size_type capacity() const noexcept
    {
        return capacity_;
    }

    size_type size() const noexcept
    {
        return size_;
    }

    void dealloc_all() noexcept
    {
        auto s = head_;
        object* flist = nullptr;
        while (s)
        {
            auto f = create_free_list(s);
            f.second->next = flist;
            flist = f.first;
            s = s->next;
        }
        flist_ = flist;
        size_ = 0;
    }

    static T* at(T* t)
    {
        return t;
    }

private:
    using object = pooled_object<T>;
    
    struct slab
    {
        slab* next;
        int32_t count;
        object data[1];
    };


    T* remove_free_list_head() noexcept
    {
        assert(flist_ != nullptr);
        auto ret = flist_;
        flist_ = flist_->next;
        return &ret->obj;
    }

    void free_memory() noexcept
    {
        auto cur = head_;
        while (cur)
        {
            const auto next = cur->next;
            free(cur);
            cur = next;
        }
    }

    void create_new_slab() noexcept
    {
        assert(slab_size_ >= 0);

        auto size = (slab_size_ - 1) * sizeof(object) + sizeof(slab);
        auto p = static_cast<slab*>(malloc(size)); // TODO: alignment
        if (p)
        {
            p->count = slab_size_;
            p->next = nullptr;

            enlist_new_slab(p);
            flist_ = create_free_list(p).first;

            capacity_ += slab_size_;
            increase_next_slab_size(size);
        }
    }

    void enlist_new_slab(slab* s) noexcept
    {
        if (tail_)
        {
            tail_->next  = s;
        }
        else
        {
            head_ = tail_ = s;
        }
    }

    void increase_next_slab_size(size_t current_size) noexcept
    {
        slab_size_ = current_size > 4 * 1024 ? slab_size_ : slab_size_ * 2;
    }

    static std::pair<object*, object*> create_free_list(slab* s)
    {
        auto flist = s->data;
        auto prev = flist;

        for (auto i = 1; i < s->count; ++i)
        {
            prev->next = &s->data[i];
            prev = prev->next;
        }

        prev->next = nullptr;
        return {flist, prev};
    }

    slab* head_ = nullptr;
    slab* tail_ = nullptr;
    object* flist_ = nullptr;
    uint16_t slab_size_ = 1;
    size_type capacity_{};
    size_type size_{};
};

// TODO:
// 1. limit stack depth
// 2. limit number of metrics stored
// 3. contiguos object pool
template <typename K, typename V, int N = 254>
class trie
{
public:
    using key_type = K;
    using value_type = V;

    ~trie()
    {
        /* foreach_node(root, [](auto n){ delete n; }); */
    }

    value_type& up()
    {
        assert(cursor != nullidx);
        auto& res = at(cursor).value;
        cursor = at(cursor).parent;
        return res;
    }

    template <typename F>
    void up(F&& value_func)
    {
        assert(cursor != nullidx);
        at(cursor).value = value_func(at(cursor).value);
        cursor = at(cursor).parent;
    }

    value_type& down(key_type key)
    {
        if (cursor != nullidx)
        {
            cursor = add_child(cursor, key);
        }
        else
        {
            if (root != nullidx)
            {
                assert(key == at(root).key);
                cursor = root;
            }
            else 
            {
                root = cursor = new_node(key);
            }
        }

        return at(cursor).value;
    }

    template <typename F>
    void down(key_type key, F&& value_func)
    {
        down(key);
        at(cursor).value = value_func(at(cursor).value);
    }

    value_type& get()
    {
        assert(cursor != nullidx);
        return at(cursor).value;
    }

    value_type& at(std::initializer_list<key_type>&& path)
    {
        auto res = nullidx;
        for (auto p : path)
        {
            res = get_child(res, p);
            assert(res != nullidx);
        }
        return at(res).value;
    }

    bool has(std::initializer_list<key_type>&& path)
    {
        auto res = nullidx;
        for (auto p : path)
        {
            res = get_child(res, p);
            if (res == nullidx)
            {
                return false;
            }
        }
        return true;
    }

    value_type& create(std::initializer_list<key_type>&& path)
    {
        auto res = nullidx;
        for (auto p: path)
        {
            res = create_child(res, p);
        }

        return at(res).value;
    }

    template <typename F>
    void foreach(F&& func)
    {
        foreach_node(root, [&func, this](auto node) { func(at(node).key, at(node).value); });
    }

private:
    struct node
    {
        node* parent;
        node* child;
        node* sibling;
        key_type key;
        value_type value;
    };

    using index_type = node*;
    constexpr static index_type nullidx = nullptr;

    node& at(index_type idx)
    {
        assert(idx != nullidx);
        return *pool.at(idx);
    }

    index_type new_node()
    {
        return new_node({});
    }

    index_type new_node(key_type key, index_type parent = nullidx)
    {
        const auto index = pool.construct();
        auto& node = at(index);
        node.child = node.sibling = nullidx;
        node.parent = parent;
        node.key = key;
        return index;
    }

    index_type get_child(index_type parent, key_type key)
    {
        if (parent != nullidx)
        {
            return find_child(parent, key);
        }
        else
        {
            if (root != nullidx && key == at(root).key)
            {
                return root;
            }
        }

        return nullidx;
    }

    index_type add_child(index_type p, key_type key)
    {
        if (at(p).child != nullidx)
        {
            p = guess_child(p, key);

            if (at(p).key != key)
            {
                auto sibling = new_node(key, at(p).parent);
                at(p).sibling = sibling; // p may relocate
                p = sibling;
            }
        }
        else
        {
            auto child = new_node(key, p);
            at(p).child = child; // p may relocate
            p = child;
        }
        return p;
    }

    index_type create_child(index_type parent, key_type key)
    {
        if (parent == nullidx)
        {
            assert(root == nullidx);
            root = new_node();
            at(root).key = key;
            return root;
        }
        else
        {
            auto existing_child =  get_child(parent, key);
            return existing_child != nullidx ? existing_child : add_child(parent, key);
        }
    }

    template <typename F>
    void foreach_node(index_type p, F&& func)
    {
        if (p != nullidx)
        {
            foreach_node(at(p).child, std::forward<decltype(func)>(func)); 
            foreach_node(at(p).sibling, std::forward<decltype(func)>(func));

            std::forward<decltype(func)>(func)(p);
        }
    }

    index_type find_child(index_type n, key_type key)
    {
        auto child = at(n).child;
        while (child != nullidx && at(child).key != key)
        {
            child = at(child).sibling;
        }

        return child;
    }

    // finds a child or, if not present, its insertion place
    // assumes there is at least one child
    index_type guess_child(index_type n, key_type key)
    {
        auto child = at(n).child;
        assert(child != nullidx);

        while (at(child).key != key && at(child).sibling != nullidx)
        {
            child = at(child).sibling;
        }

        return child;
    }

    index_type cursor = nullidx;
    index_type root = nullidx;
    heap_pool<node> pool;
};

template <typename T>
class monitor
{
public:
    class metric
    {
    public:
        metric()
        {
        }

        ~metric()
        {
            stop();
        }

        void stop()
        {
            if (_mon)
            {
                _mon->stop();
            }
            _mon = nullptr;
        }

        metric(const metric&) = delete;
        metric& operator =(const metric&) = delete;


        metric(metric&& other)
        {
            *this = std::move(other);
        }

        metric& operator =(metric&& other)
        {
            stop();
            _mon = other._mon;

            other._mon = nullptr;
            return *this;
        }

    private:
        metric(T id, monitor& mon)
            : _mon(&mon)
        {
            _mon->start(id);
        }

        monitor* _mon = nullptr;

        friend class monitor;
    };
    
    /* using report_t = tree<T, unsigned long long>; */

    void start(T id)
    {
        trie_.down(id).start();
    }

    void stop()
    {
        trie_.up().start();
    }

    metric scope(T id)
    {
        return metric(id, *this);
    }

    metric operator ()(T id)
    {
        return scope(id);
    }

    /*report_t report()
    {
        report_t res;
        trie_.foreach([&res](auto key, auto& val)
        {
            res[{key}] = val.elapsed();
        });

        return res;
    }*/

    std::string report_json()
    {
        /* auto data = report(); */
        /* return haisu::to_json(report()); */
        return "";
    }
    
private:
    using timer = aggregate_timer;
    trie<T, aggregate_timer> trie_;
};

} // namespace metric
