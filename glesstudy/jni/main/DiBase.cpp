#define _CRT_SECURE_NO_WARNINGS

#include "DiBase.h"
#include "SDL.h"

#include <algorithm>

namespace di
{
    string String_Format(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        string ret = String_FormatV(fmt, ap);
        va_end(ap);
        return ret;
    }


    string String_FormatV(const char* fmt, va_list ap)
    {
        string ret;

        if (fmt)
        {
            int n = vsnprintf(nullptr, 0, fmt, ap);
            if (n > 0)
            {
                ret.resize(size_t(n));
                vsnprintf(&ret[0], size_t(n), fmt, ap);
            }
        }

        return ret;
    }


    void LogV(int lev, const char* fmt, va_list ap)
    {
        SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, (SDL_LogPriority)lev, fmt, ap);
    }


    void LogDebug(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        LogV(SDL_LOG_PRIORITY_DEBUG, fmt, ap);
        va_end(ap);
    }


    void LogInfo(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        LogV(SDL_LOG_PRIORITY_INFO, fmt, ap);
        va_end(ap);
    }


    void LogWarn(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        LogV(SDL_LOG_PRIORITY_WARN, fmt, ap);
        va_end(ap);
    }


    void LogError(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        LogV(SDL_LOG_PRIORITY_ERROR, fmt, ap);
        va_end(ap);
    }


#ifdef _WIN32
    static __declspec(thread) FuncCallInfoStack* s_threadFuncCallInfoStack;
#else
    static __thread FuncCallInfoStack* s_threadFuncCallInfoStack;
#endif


    FuncCallInfoStack& FuncCallInfoStack::GetThreadStack()
    {
        if (!s_threadFuncCallInfoStack)
        {
            s_threadFuncCallInfoStack = new FuncCallInfoStack();
        }

        return *s_threadFuncCallInfoStack;
    }


    void FuncCallInfoStack::DeleteThreadStack()
    {
        if (!s_threadFuncCallInfoStack)
        {
            return;
        }

        DI_ASSERT(s_threadFuncCallInfoStack->m_stack.empty());

        delete s_threadFuncCallInfoStack;
        s_threadFuncCallInfoStack = nullptr;
    }


    void FuncCallInfoStack::OutputToLog()
    {
        // TODO: not finished
    }


    void PerformanceProfileData::OutputToLog()
    {
        LogInfo("========== PerformanceProfileData ==========");
        for (auto iter = m_items.begin(); iter != m_items.end(); ++iter)
        {
            const string& item = (*iter).first;
            const ItemData& itemData = (*iter).second;

            LogInfo("    [%28s] - millis: %15f, count: %10d", item.c_str(), itemData.seconds * 1000.0, itemData.times);
        }
    }


    int SDLCALL ThreadEntry(void* arg)
    {
        unique_ptr<ThreadEventHandlers> handlers(static_cast<ThreadEventHandlers*>(arg));

        try
        {
            if (handlers->onInit) handlers->onInit();
        }
        catch (const exception& ex)
        {
            LogError("thread onInit caught exception: %s", ex.what());
            LogWarn("thread will end");
            return 0;
        }
        catch (...)
        {
            LogError("thread onInit caught unknown exception");
            LogWarn("thread will end");
            return 0;
        }

        int exceptionInConsecutive = 0;
        for (;;)
        {
            bool haveException = false;
            bool willEndThread = false;
            uint32_t willWaitMillis = 0;

            if (handlers->onLoop)
            {
                try
                {
                    handlers->onLoop(&willEndThread, &willWaitMillis);
                }
                catch (const exception& ex)
                {
                    haveException = true;
                    LogError("thread loop caught exception: %s", ex.what());
                }
                catch (...)
                {
                    haveException = true;
                    LogError("thread loop caught unknown exception");
                }

                if (haveException)
                {
                    ++exceptionInConsecutive;

                    if (exceptionInConsecutive >= 3)
                    {
                        willEndThread = true;
                        LogWarn("thread loop will end because of 3 consecutive exceptions");
                    }
                    else
                    {
                        willWaitMillis = 2000;
                        LogWarn("thread loop will wait 2 seconds because of exception");
                    }
                }
                else
                {
                    exceptionInConsecutive = 0;
                }

                if (willEndThread)
                {
                    break;
                }

                if (willWaitMillis)
                {
                    SDL_Delay(willWaitMillis);
                }
            }
            else
            {
                break;
            }
        }

        try
        {
            if (handlers->onEnd) handlers->onEnd();
        }
        catch (const exception& ex)
        {
            LogError("thread onEnd caught exception: %s", ex.what());
        }
        catch (...)
        {
            LogError("thread onEnd caught unknown exception");
        }

        FuncCallInfoStack::DeleteThreadStack();
        return 0;
    }


    void StartThread(const char* name, const ThreadEventHandlers& handlers)
    {
        ThreadEventHandlers* newHandlers = new ThreadEventHandlers(handlers);
        SDL_CreateThread(ThreadEntry, name, newHandlers);
    }


    ThreadLock::ThreadLock()
    {
        m_data = SDL_CreateMutex();
    }


    ThreadLock::~ThreadLock()
    {
        SDL_DestroyMutex((SDL_mutex*)m_data);
    }


    void ThreadLock::Lock()
    {
        DI_SAVE_CALLSTACK();
        SDL_LockMutex((SDL_mutex*)m_data);
    }


    void ThreadLock::Unlock()
    {
        DI_SAVE_CALLSTACK();
        SDL_UnlockMutex((SDL_mutex*)m_data);
    }


    ThreadConditionVariable::ThreadConditionVariable()
    {
        m_data = SDL_CreateCond();
    }


    ThreadConditionVariable::~ThreadConditionVariable()
    {
        SDL_DestroyCond((SDL_cond*)m_data);
    }


    void ThreadConditionVariable::WaitUntil(ThreadLock& l, const function<bool()>& cond, const function<void()>& func)
    {
        DI_SAVE_CALLSTACK();

        ThreadLockGuard lock(l);
        SDL_CondWait((SDL_cond*)m_data, (SDL_mutex*)l.m_data);
        func();
    }


    void ThreadConditionVariable::Notify()
    {
        SDL_CondBroadcast((SDL_cond*)m_data);
    }


    Node::Node()
    {
        m_needSortChildren = false;
        m_drawPriority = 0.0f;
        m_position = MakeVec3(0.0f, 0.0f, 0.0f);
        m_anchor = MakeVec3(0.0f, 0.0f, 0.0f);
        m_scale = MakeVec3(0.0f, 0.0f, 0.0f);
        m_rotate = MakeVec4(0.0f, 0.0f, 0.0f, 0.0f);
        m_matrixDirty = true;
        m_matrixInParent = matrix_identity<float, 4>();
        m_matrixInWorld = m_matrixInParent;
    }


    void Node::AddChild(NodePtrCR child)
    {
        DI_SAVE_CALLSTACK();

        DI_ASSERT(child);
        DI_ASSERT(child->m_parent.expired());
        m_children.push_back(child);
        child->m_parent = This<Node>();
        m_needSortChildren = true;

        child->UpdateMatrixInWorld();
    }


    void Node::RemoveFromParent()
    {
        DI_SAVE_CALLSTACK();

        NodePtr parent = m_parent.lock();
        DI_ASSERT(parent);

        vector<NodePtr>& parentChildren = parent->m_children;
        parentChildren.erase(
            remove(parentChildren.begin(), parentChildren.end(), This<Node>()),
            parentChildren.end());

        m_parent.reset();
        UpdateMatrixInWorld();
    }


    void Node::RemoveAllChild()
    {
        DI_SAVE_CALLSTACK();

        for (auto iter = m_children.begin(); iter != m_children.end(); ++iter)
        {
            NodePtrCR child = *iter;
            DI_ASSERT(child);

            child->m_parent.reset();
            child->UpdateMatrixInWorld();
        }

        m_children.clear();
    }


    void Node::VisitAndDraw()
    {
        if (m_needSortChildren)
        {
            std::sort(m_children.begin(), m_children.end(), [](NodePtrCR c1, NodePtrCR c2)
            {
                return c1->m_drawPriority < c2->m_drawPriority;
            });

            m_needSortChildren = false;
        }

        if (m_matrixDirty)
        {
            UpdateMatrixInParent();
            m_matrixDirty = false;
        }
    }


    void Node::Draw()
    {
        // empty
    }


    void Node::UpdateMatrixInParent()
    {
        m_matrixInParent = quaternion_to_matrix(m_rotate)
                         * matrix_scale(m_scale)
                         * matrix_translate(m_position);

        UpdateMatrixInWorld();
    }


    void Node::UpdateMatrixInWorld()
    {
        NodePtr parent = m_parent.lock();
        if (parent)
        {
            m_matrixInWorld = m_matrixInParent * parent->m_matrixInWorld;
        }
        else
        {
            m_matrixInWorld = m_matrixInParent;
        }
    }
}
