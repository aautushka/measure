/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

#pragma once

#include <sys/time.h>
#include <map>
#include <iomanip>
#include <sstream>

namespace metric
{

template <typename Key, typename Val>
class tree
{
    using leaves_t = std::map<Key, std::unique_ptr<tree>>;    

public:
    using self_type = tree<Key, Val>;
    using value_type = Val;
    using key_type = Key;

    using iterator = typename leaves_t::iterator;
    using const_iterator = typename leaves_t::const_iterator;

    tree()
    {
    }

    tree(const tree& other) = delete;
    tree& operator =(const tree& other) = delete;

    tree(tree&& other)
    {
        *this = std::move(other);
    }

    tree& operator =(tree&& other)
    {
        _val = std::move(other._val);
        _leaves = std::move(other._leaves);
        return *this;
    }

    template <typename T>
    value_type& operator [](const T& key)
    {
        return child(key)->get();
    }

    template <typename T>
    const value_type& operator [](const T& key) const
    {
        return child(key)->get();
    }

    value_type& operator [](const key_type& key)
    {
        return child(key)->get();
    }

    const value_type& operator [](const key_type& key) const
    {
        return child(key)->get();
    }

    void insert(const key_type& key, const value_type& val)
    {
        child(key) = val;
    }

    bool empty() const
    {
        return _leaves.empty();
    }

    iterator begin() 
    {
        return iterator(_leaves.begin());
    }

    iterator end()
    {
        return iterator(_leaves.end());
    }

    const_iterator begin() const
    {
        return _leaves.begin();
    }

    const_iterator end() const
    {
        return _leaves.end();
    }

    template <typename Func>
    void foreach(Func func) const
    {
        for (auto i = begin(); i != end(); ++i)
        {
            func(i->first, i->second->get());    
            i->second->foreach(func);
        }
    }

    value_type& get()
    {
        return _val;
    }

    const value_type& get() const
    {
        return _val;
    }

    template <typename T> 
    size_t count(const T& key) const
    {
        return find(key) ? 1 : 0;
    }

    void erase(const key_type& key)
    {
        _leaves.erase(key);
    }

    template <typename T> 
    void erase(const T& key)
    {
        tree* child = this;
        tree* parent = this;
        key_type leaf;
        for (auto k: key)
        {
            if (child != parent)
            {
                parent = child;
            }

            auto i = parent->_leaves.find(k);
            if (i == parent->_leaves.end())
            {
                // can't find a node to remove
                return;
            }
            else
            {
                child = i->second.get();
                leaf = k;
            }
        }

        parent->erase(leaf);
    }

    void clear()
    {
        _leaves.clear();
        _val = value_type();
    }

    size_t size() const
    {
        size_t sum = 0;
        foreach([&](Key&, Val&) { ++sum; });
        return sum;
    }

private:
    // TODO: gls::not_null
    tree* child(const key_type& key)
    {
        auto i = _leaves.find(key);
        if (i != _leaves.end())
        {
            return i->second.get();
        }
        else
        {
            std::unique_ptr<self_type> res(new tree);
            auto ptr = res.get();
            _leaves[key] = std::move(res);
            return ptr;
        }
    }

    // TODO: gsl::not_null
    const tree* child(const key_type& key) const
    {
        auto i = _leaves.find(key);
        if (i != _leaves.end())
        {
            return i->second.get();
        }

        throw std::exception();
    }

    // TODO: gsl::not_null
    template <typename T>
    tree* child(const T& key)
    {
        self_type* tr = this;
        for (auto k : key) tr = tr->child(k);
        return tr;
    }

    // TODO: gls::not_null
    template <typename T>
    const tree* child(const T& key) const
    {
        const self_type* tr = this;
        for (auto& k : key) tr = tr->child(k);
        return tr;
    }

    template <typename T> 
    const tree* find(T& key) const
    {
        const self_type* tr = this;
        for (auto k: key)
        {
            auto i = tr->_leaves.find(k);
            if (i != tr->_leaves.end())
            {
                tr = i->second.get();
            }
            else
            {
                return nullptr;
            }
        }

        return tr;
    }

    self_type* find(const key_type& key) const
    {
        auto i = _leaves.find(key);
        return i == _leaves.end() ? nullptr : i->second.get();
    }

    value_type _val;
    leaves_t _leaves;    
}; 

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
T to_json(T t)
{
    return t;
}

inline std::string to_json(std::string t)
{
    // TODO: escaping!
    return t;
}

template <typename U, typename V>
void to_json(const tree<U, V>& tr, std::ostream& stream, int depth, bool write_default_value = false)
{
    auto pad = [depth, &stream](int delta = 0) {stream << std::string((depth + delta) * 4, ' ');};

    stream << '{' << '\n';

    if (write_default_value)
    {
        pad(1);
        stream << "\"#\": \"" << to_json(tr.get()) << '"';
    }

    for (auto i = tr.begin(); i != tr.end(); ++i)
    {
        if (write_default_value || i != tr.begin())
        {
            stream << ", \n";
        }

        pad(1);
        stream << '"' << i->first << "\":";
        
        auto& child = *i->second;
        if (child.begin() != child.end())
        {
            to_json(*i->second, stream, depth + 1, true);
        }
        else
        {
            stream << '"' << to_json(child.get()) << '"';
        }
    }

    stream << '\n';
    pad();
    stream << "}";
}


template <typename U, typename V>
std::string to_json(const tree<U, V>& tr)
{
    std::stringstream ret;
    to_json(tr, ret, 0); 
    return ret.str();
}


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

    timer& operator +=(const timer& other) 
    {
        _elapsed += other._elapsed;
        return *this;
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
    using num_t = unsigned long;

    void start()
    {
        _elapsed = now() - _elapsed;
    }

    void stop()
    {
        ++_calls;
        _elapsed = now() - _elapsed;
    }

    usec_t elapsed() const
    {
        return _elapsed;
    }

    num_t calls() const
    {
        return _calls;
    }

    double avg() const
    {
        return _calls ? (double)_elapsed / _calls : 0;
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

    aggregate_timer& operator +=(const aggregate_timer& other)
    {
        _elapsed += other._elapsed;
        _calls += other._calls;
        return *this;
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

    heap_pool(const heap_pool&) = delete;
    heap_pool& operator =(const heap_pool&) = delete;

    heap_pool(heap_pool&& other)
    {
        move_from(std::move(other));
    }

    heap_pool& operator =(heap_pool&& other)
    {
        free_memory();
        move_from(std::move(other));
        return *this;
    }

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

    static const T* at(const T* t)
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

    void move_from(heap_pool&& other)
    {
        head_ = other.head_;
        tail_ = other.tail_;
        flist_ = other.flist_;
        slab_size_ = other.slab_size_;
        capacity_ = other.capacity_;
        size_ = other.size_;

        other.set_defaults();
    }

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

        set_defaults();
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

    void set_defaults()
    {
        head_ = tail_ = nullptr;
        flist_ = nullptr;
        slab_size_ = 1;
        capacity_ = 0;
        size_ = 0;
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
// 4. make trie copy-able
// 5. perfect forwarding of functor args (lambdas)
// 6. better sampling options: medians? 
template <typename K, typename V, int N = 254>
class trie
{
public:
    using key_type = K;
    using value_type = V;
    using self_type = trie<K, V, N>;

    ~trie()
    {
        /* foreach_node(root, [](auto n){ delete n; }); */
    }

    trie() = default;
    trie(trie&&) = default;
    trie& operator =(trie&&) = default;


    trie(const trie&) = delete;
    trie& operator =(const trie&) = delete;

    value_type& up()
    {
        assert(cursor != nullidx);
        assert(trie_depth > 0);

        auto& res = at(cursor).value;
        cursor = at(cursor).parent;
        --trie_depth;
        return res;
    }

    template <typename F>
    void up(F&& value_func)
    {
        assert(cursor != nullidx);
        assert(trie_depth > 0);

        at(cursor).value = value_func(at(cursor).value);
        cursor = at(cursor).parent;
        --trie_depth;
    }

    value_type& down(key_type key)
    {
        if (cursor != nullidx)
        {
            cursor = add_child(cursor, key);
        }
        else
        {
            cursor = root;
            while (cursor != nullidx && key != at(cursor).key)
            {
                cursor = cursor->sibling;
            }

            if (cursor == nullidx)
            {
                cursor = new_node(key);
                cursor->sibling = root;
                root = cursor;
            }
        }

        ++trie_depth;
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
        foreach_node(root, [&func, this](index_type node) { func(at(node).key, at(node).value); });
    }

    template <typename F>
    void foreach_path(F&& func)
    {
        foreach_node(root, [&func, this](index_type node)
        {
            auto root = node;
            std::vector<key_type> path;
            path.push_back(node->key);
            while (node->parent)
            {
                node = node->parent;
                path.insert(path.begin(), node->key);
            }
            
            func(path, at(root).value);
        });
    }

    unsigned depth() const
    {
        return trie_depth;
    }

    self_type clone() const
    {
        self_type result;

        recursive_clone(result, root);

        return result;
    }

    self_type combine(const self_type& other) const
    {
        self_type result;
        recursive_clone(result, root);
        other.recursive_clone(result, other.root);
        return result;
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

    const node& at(index_type idx) const
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
            auto res = root;
            while (res != nullidx && key != at(res).key)
            {
                res = res->sibling;
            }
            return res;
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

    void recursive_clone(self_type& result, index_type p) const
    {
        if (p != nullidx)
        {
            // TODO what if operator += is not defined? use templates to fix this
            result.down(at(p).key) += at(p).value;

            recursive_clone(result, at(p).child);
            result.up();
            recursive_clone(result, at(p).sibling);
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
    unsigned trie_depth = 0;
    heap_pool<node> pool;
};

enum class report_type : int
{
    averages,
    calls,
    percentages,
    totals,
    full
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
    
    using report_t = tree<T, std::string>;

    void start(T id)
    {
        if (trie_.depth() > 0)
        {
            return trie_.down(id).start();
        }
        
        if (sample_start_ > 0)
        {
            if (--sample_start_)
            {
                return;
            }
        }

        if (sample_limit_ > 0)
        {
            --sample_limit_;
            return trie_.down(id).start();
        }
    }

    void stop()
    {
        if (sample_start_ == 0 && (trie_.depth() > 0 || sample_limit_ > 0))
        {
            trie_.up().stop();
        }
    }

    metric scope(T id)
    {
        return metric(id, *this);
    }

    metric operator ()(T id)
    {
        return scope(id);
    }

    report_t report(report_type type = report_type::averages)
    {
        report_t res;
        if (type != report_type::percentages && type != report_type::full)
        {
            trie_.foreach_path([&res, type](const std::vector<T> path, aggregate_timer& val)
            {
                switch (type)
                {
                case report_type::averages: res[path] = str(val.avg()); break;
                case report_type::calls: res[path] = str(val.calls()); break;
                case report_type::totals: res[path] = str(val.elapsed()); break;
                default: res[path] = ""; break;
                }
            });
        } 
        else
        {
            uint64_t total_time = 0;
            trie_.foreach_path([&total_time](const std::vector<T> path, aggregate_timer& val)
            {
                if (path.size() == 1)
                {
                    total_time += val.elapsed();
                }
            });

            trie_.foreach_path([&res, total_time, type](const std::vector<T> path, aggregate_timer& val)
            {
                auto percentage = val.elapsed() / (double)total_time * 100;
                if (type == report_type::percentages)
                {
                    res[path] = str(percentage) + "%";
                }
                else
                {
                    std::stringstream ss;
                    ss << percentage << "% [" << val.elapsed() << "/ " << val.calls() << " = " << val.avg() << " us]";
                    res[path] = ss.str();
                }
            });
        }

        return res;
    }

    template <typename F>
    void foreach(F&& f)
    {
        // TODO perfect forwarding for f
        trie_.foreach([&f](T key, timer& val)
        {
            f(key, val.elapsed(), val.calls());
        });
    }

    template <typename F>
    void foreach_path(F&& f)
    {
        trie_.foreach_path([&f](const char* key, timer& val)
        {
            f(key, val.elapsed(), val.calls());
        });
    }

    std::string report_json(report_type type = report_type::averages)
    {
        return to_json(report(type));
    }

    void stop_sampling_after(unsigned samples_num)
    {
        sample_limit_ = samples_num;
    }
    
    void start_sampling_after(unsigned samples_num)
    {
        sample_start_ = samples_num + 1;
    }

    monitor clone() const
    {
        monitor result;
        result.trie_ = std::move(trie_.clone());
        return result;
    }

    monitor combine(const monitor& other) const
    {
        monitor result;
        result.trie_ = std::move(trie_.combine(other.trie_));
        return result;
    }
    
private:
    using timer = aggregate_timer;
    trie<T, aggregate_timer> trie_;
    unsigned sample_limit_ = 0xffffffff;
    unsigned sample_start_ = 1;

    template <typename Type>
    static std::string str(Type&& t)
    {
        std::stringstream ss;
        ss << t;
        return ss.str();
    }
};

} // namespace metric
