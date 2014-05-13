#ifndef DI_BASE_H_INCLUDED
#define DI_BASE_H_INCLUDED

#include <memory>
#include <vector>
#include <unordered_map>

#include <string>
#include <stdexcept>
#include <cstdarg>

#include "di_vec.h"
#include "di_mat.h"

#include "SDL.h"

#define DI_TYPEDEF_PTR(cls) \
    class cls; \
    typedef std::shared_ptr<cls> cls##Ptr; \
    typedef std::shared_ptr<const cls> cls##CPtr; \
    typedef const std::shared_ptr<cls>& cls##PtrCR; \
    typedef const std::shared_ptr<const cls>& cls##CPtrCR; \
    typedef std::weak_ptr<cls> cls##WPtr; \
    typedef std::weak_ptr<const cls> cls##WCPtr; \
    typedef const std::weak_ptr<cls>& cls##WPtrCR; \
    typedef const std::weak_ptr<const cls>& cls##WCPtrCR; \

#define DI_DISABLE_COPY(cls) \
    private: cls(const cls&); cls& operator=(const cls&);

#define DI_LOG_DEBUG(fmt, ...)      di::LogDebug(fmt, __VA_ARGS__)
#define DI_LOG_INFO(fmt, ...)       di::LogInfo(fmt, __VA_ARGS__)
#define DI_LOG_WARN(fmt, ...)       di::LogWarn(fmt, __VA_ARGS__)
#define DI_LOG_ERROR(fmt, ...)      di::LogError(fmt, __VA_ARGS__)

#define DI_ASSERT(expr) \
    if (expr) {} else { DI_LOG_ERROR("ASSERT '%s' failed at %s:%d (%s)", #expr, __FILE__, __LINE__, __FUNCTION__); throw std::runtime_error("assert failed"); }

#define DI_ASSERT_IN_DESTRUCTOR(expr) \
    if (expr) {} else { DI_LOG_ERROR("ASSERT '%s' failed at %s:%d (%s)", #expr, __FILE__, __LINE__, __FUNCTION__); }

#define DI_SAVE_CALLSTACK() \
    di::FuncCallInfoSaver di_callstack_saver(__FILE__, __LINE__, __FUNCTION__)

#define DI_PROFILE(item) \
    di::PerformanceProfileGuard di_profile_guard_ ## item(#item);

#define DI_PROFILE_STR(varName, itemName) \
    di::PerformanceProfileGuard varName(itemName);

namespace di
{
    using namespace std;


    template <typename T>
    T clamp(const T& t, const T& tMin, const T& tMax) { if (t <= tMin) return tMin; if (t >= tMax) return tMax; return t; }


    uint64_t HighClock_Get();
    double HighClock_ToSeconds(uint64_t clocks);
    string String_FormatV(const char* fmt, va_list ap);


#ifdef __GNUC__
    string String_Format(const char* fmt, ...) __attribute__ ((format(printf, 1, 2)));
    void LogDebug(const char* fmt, ...) __attribute__ ((format(printf, 1, 2)));
    void LogInfo(const char* fmt, ...) __attribute__ ((format(printf, 1, 2)));
    void LogWarn(const char* fmt, ...) __attribute__ ((format(printf, 1, 2)));
    void LogError(const char* fmt, ...) __attribute__ ((format(printf, 1, 2)));
#else
    string String_Format(_In_z_ _Printf_format_string_ const char* fmt, ...);
    void LogDebug(_In_z_ _Printf_format_string_ const char* fmt, ...);
    void LogInfo(_In_z_ _Printf_format_string_ const char* fmt, ...);
    void LogWarn(_In_z_ _Printf_format_string_ const char* fmt, ...);
    void LogError(_In_z_ _Printf_format_string_ const char* fmt, ...);
#endif


    template <typename F>
    class ExitScopeCall
    {
    public:
        ExitScopeCall(const F& f) : m_f(f) {}
        ExitScopeCall(ExitScopeCall&& other) : m_f(move(other.m_f)) {}
        ~ExitScopeCall() { m_f(); }

    private:
        F m_f;

        DI_DISABLE_COPY(ExitScopeCall);
    };


    template <typename F>
    ExitScopeCall<F> MakeCallAtScopeExit(const F& f) { return ExitScopeCall<F>(f); }


    struct FuncCallInfo
    {
        const char* file;
        int line;
        const char* function;
    };


    class FuncCallInfoStack
    {
    public:
        FuncCallInfoStack() { m_stack.reserve(1024); }
        ~FuncCallInfoStack() {}

        static FuncCallInfoStack& GetThreadStack();
        static void DeleteThreadStack();

        void Push(const FuncCallInfo& info) { m_stack.push_back(info); }
        void Pop() { m_stack.pop_back(); }
        void OutputToLog();
        string OutputToString();

    private:
        vector<FuncCallInfo> m_stack;
    };


    class FuncCallInfoSaver
    {
    public:
        FuncCallInfoSaver(const char* file, int line, const char* function) { FuncCallInfo info = { file, line, function }; FuncCallInfoStack::GetThreadStack().Push(info); }
        ~FuncCallInfoSaver() { FuncCallInfoStack::GetThreadStack().Pop(); }

        DI_DISABLE_COPY(FuncCallInfoSaver);
    };


    class PerformanceProfileData
    {
    public:
        PerformanceProfileData() {}
        void Add(const string& item, double seconds)    { ItemData& d = m_items[item]; d.seconds += seconds; d.times++; }
        void OutputToLog();

        static PerformanceProfileData& Singleton() { if (!s_singleton) { s_singleton.reset(new PerformanceProfileData()); } return *s_singleton; }
        static void DestroySingleton() { s_singleton.reset(); }

    private:
        struct ItemData
        {
            double seconds;
            long times;
        };

        unordered_map<string, ItemData> m_items;

        static unique_ptr<PerformanceProfileData> s_singleton;

        DI_DISABLE_COPY(PerformanceProfileData);
    };


    class PerformanceProfileGuard
    {
    public:
        PerformanceProfileGuard(const string& item) : m_item(item), m_tick(SDL_GetPerformanceCounter()) {}
        ~PerformanceProfileGuard() { PerformanceProfileData::Singleton().Add(m_item, (SDL_GetPerformanceCounter() - m_tick) / (double)(SDL_GetPerformanceFrequency())); }

    private:
        string m_item;
        uint64_t m_tick;

        DI_DISABLE_COPY(PerformanceProfileGuard);
    };


    struct ThreadEventHandlers
    {
        string threadName;
        function<void()> onInit;
        function<void()> onEnd;
        function<void(bool* /*willEndThread*/, uint32_t* /*willWaitMillis*/)> onLoop;
    };

    void StartThread(const ThreadEventHandlers& handlers);


    class ThreadLock
    {
    public:
        friend class ThreadConditionVariable;

        ThreadLock();
        ~ThreadLock();

        void Lock();
        void Unlock();

    private:
        void* m_data;

        DI_DISABLE_COPY(ThreadLock);
    };


    class ThreadLockGuard
    {
    public:
        ThreadLockGuard(ThreadLock& l) : m_lock(l), m_locked(false) { m_lock.Lock(); m_locked = true; }
        ~ThreadLockGuard() { Unlock(); }

        void Unlock() { if (m_locked) { m_lock.Unlock(); m_locked = false; } }

    private:
        bool m_locked;
        ThreadLock& m_lock;

        DI_DISABLE_COPY(ThreadLockGuard);
    };


    // ThreadConditionVariable：主要用于生产者消费者。
    // 用法：
    // 1. 创建一个ThreadLock对象，生产消费线程共用
    // 2. 创建一个ThreadConditionVariable对象，生产消费线程共用
    // 3. 定义一个函数function<bool()>，如果返回true说明生产者已经生产完毕。
    //    简单情况下，这个函数可能直接返回一个bool变量的值
    //    初始化时让函数返回false
    // 4. 在生产者线程：先对ThreadLock对象加锁，然后进行生产。生产完毕后设法让函数返回true，然后解锁，最后调用ThreadConditionVariable::Notify
    // 5. 消费者线程：直接调用ThreadConditionVariable::WaitUntil。这里传入锁，一个判断函数，一个执行函数。判断函数和执行函数都是在加锁情况下调用的，返回之后会解锁。
    // 6. 一个ThreadConditionVariable可以反复多次使用。生产者、消费者的身份也可以反转。
    //
    // 参见：http://en.cppreference.com/w/cpp/thread/condition_variable
    //
    class ThreadConditionVariable
    {
    public:
        ThreadConditionVariable();
        ~ThreadConditionVariable();

        void WaitUntil(ThreadLock& l, const function<bool()>& cond, const function<void()>& func);
        void Notify();

    private:
        void* m_data;

        DI_DISABLE_COPY(ThreadConditionVariable);
    };


    DI_TYPEDEF_PTR(Obj);
    DI_TYPEDEF_PTR(Node);


    class Obj : public enable_shared_from_this<Obj>
    {
    public:
        Obj() {}
        virtual ~Obj() {}

        template <typename T> shared_ptr<T> This() { return static_pointer_cast<T>(shared_from_this()); }
        template <typename T> shared_ptr<const T> This() const { return static_pointer_cast<const T>(shared_from_this()); }

        DI_DISABLE_COPY(Obj);
    };


    class Node : public Obj
    {
    public:
        Node();

        void AddChild(NodePtrCR child);
        void RemoveFromParent();
        void RemoveAllChild();
        NodePtr GetParent() const                   { return m_parent.lock(); }
        const vector<NodePtr>& GetChildren() const  { return m_children; }

        void SetDrawPriority(float drawPriority)    { m_drawPriority = drawPriority; NodePtr p = m_parent.lock(); if (p) { p->m_needSortChildren = true; } }
        void SetPosition(const Vec3& position)      { m_position = position; m_matrixDirty = true; }
        void SetAnchor(const Vec3& anchor)          { m_anchor = anchor; m_matrixDirty = true; }
        void SetScale(const Vec3& scale)            { m_scale = scale; m_matrixDirty = true; }
        void SetRotate(const Vec4& rotate)          { m_rotate = rotate; m_matrixDirty = true; }

        float GetDrawPriority() const               { return m_drawPriority; }
        const Vec3& GetPosition() const             { return m_position; }
        const Vec3& GetAnchor() const               { return m_anchor; }
        const Vec3& GetScale() const                { return m_scale; }
        const Vec4& GetRotate() const               { return m_rotate; }
        const Mat4& GetMatrixInParent() const       { return m_matrixInParent; }
        const Mat4& GetMatrixInWorld() const        { return m_matrixInWorld; }

        virtual void VisitAndDraw();
        virtual void Draw();

    private:
        void UpdateMatrixInParent();
        void UpdateMatrixInWorld();

        NodeWPtr m_parent;
        vector<NodePtr> m_children;
        bool m_needSortChildren;

        float m_drawPriority;
        Vec3 m_position;
        Vec3 m_anchor;
        Vec3 m_scale;
        Vec4 m_rotate;

        bool m_matrixDirty;
        Mat4 m_matrixInParent;
        Mat4 m_matrixInWorld;
    };


    class Director : public Obj
    {
    public:
        // void InjectTouchInput();

        void PushScene(NodePtrCR scene);
        void PopScene();
        NodePtrCR CurrentScene() const;
    };
}

#endif
