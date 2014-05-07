#ifndef DI_RESOURCE_H_INCLUDED
#define DI_RESOURCE_H_INCLUDED

#include "di_gl_header.h"
#include "DiBase.h"
#include "SDL.h"

#include <deque>
#include <queue>

namespace di
{
    DI_TYPEDEF_PTR(Resource);
    DI_TYPEDEF_PTR(ImageAsTexture);

    // 异步资源基类
    //
    // 资源在两个线程之间辗转。一个是GL线程，一个是worker线程。
    // 加载资源分为三个步骤：
    //   1. 在GL线程，进行一些基本的初始化
    //   2. 在worker线程，进行实际的资源加载
    //   3. 在GL线程，完成加载动作
    //
    // 举例说明：
    // 从文件加载静态图片：第1步，在GL线程调用glGenTexture并且设置默认像素；第2步，worker线程加载图片像素；第3步，GL线程设置像素
    // 播放视频估计也可以采用类似形式（暂未深究）：第1步，GL线程调用glGenTexture；第2步，worker线程加载一帧；第3步，GL线程设置像素；之后返回第2步
    // 提示：调用时机不正确的话，glTexSubImage2D可能比glTexImage2D更慢，要注意，尤其PowerVR的显卡。视频如果性能不够，考虑用RGB565之类的格式。
    //
    class Resource : public Obj
    {
    public:
        enum State
        {
            Init,
            Prepared,
            Loaded,
            Finished,
            Failed,
            Timeout,
        };

        Resource(const string& name, float priority = 0) : m_state(State::Init), m_name(name), m_priority(priority), m_lastUsedTick(SDL_GetTicks()), m_timeoutTicks(uint32_t(1000 * 300)) {}
        virtual ~Resource() { DI_ASSERT_IN_DESTRUCTOR(m_state == State::Failed || m_state == State::Timeout); }

        bool IsResourceOK() const { return m_state == State::Finished; }
        State GetState() const { return m_state; }
        float GetPriority() const { return m_priority; }
        const string& GetName() const { return m_name; }

        void Invalidate() { if (m_state == State::Finished) { DI_SAVE_CALLSTACK(); m_state = State::Timeout; Timeout_InGlThread(); } }
        void UpdateTimeoutTick() { m_lastUsedTick = SDL_GetTicks(); }
        void CheckTimeout() { if (m_state != State::Finished) return; if (SDL_GetTicks() - m_lastUsedTick >= m_timeoutTicks) { DI_SAVE_CALLSTACK(); m_state = State::Timeout; Timeout_InGlThread(); } }

        void SetTimeoutTicks(uint32_t ticks) { m_timeoutTicks = ticks; }

        // 以下内容在ResourceManager调用。其它地方不要动
        void Prepare() { DI_SAVE_CALLSTACK(); DI_ASSERT(m_state == State::Init || m_state == State::Timeout); m_state = Prepare_InGlThread() ? State::Prepared : State::Failed; }
        void Load() { DI_SAVE_CALLSTACK(); DI_ASSERT(m_state == State::Prepared); m_state = Load_InWorkThread() ? State::Loaded : State::Failed; }
        void Finish() { DI_SAVE_CALLSTACK(); DI_ASSERT(m_state == State::Loaded); m_state = Finish_InGlThread() ? State::Finished : State::Failed; }

    private:
        virtual bool Prepare_InGlThread() = 0;
        virtual bool Load_InWorkThread() = 0;
        virtual bool Finish_InGlThread() = 0;
        virtual void Timeout_InGlThread() = 0;

        State m_state;
        float m_priority;
        uint32_t m_lastUsedTick;
        uint32_t m_timeoutTicks;
        string m_name;
    };


    class ResourceManager : public Obj
    {
    public:
        ResourceManager();
        ~ResourceManager();

        void AsyncLoadResource(ResourcePtrCR resource);

        void CheckAsyncFinishedResources();
        void CheckTimeoutResources();

        // 应该只在GL线程使用这个singleton
        static ResourceManager& GetSingleton() { if (!s_singleton) { s_singleton.reset(new ResourceManager()); } return *s_singleton; }
        static void DestroySingleton() { s_singleton.reset(); }

        template <typename T>
        shared_ptr<T> GetResource(const string& name, float priority = 0) { const ResourcePtr* r = HashFindResource(name); if (r) { DI_ASSERT(dynamic_pointer_cast<T>(*r)); return static_pointer_cast<T>(*r); } shared_ptr<T> ret(new T(name, priority)); AddResource(ret); return ret; }

    private:
        const ResourcePtr* HashFindResource(const string& name);
        void AddResource(ResourcePtrCR resource);

        struct ResourcePriorityComp
        {
            bool operator() (ResourcePtrCR r1, ResourcePtrCR r2)
            {
                return r1->GetPriority() < r2->GetPriority();
            }
        };

        struct Fields
        {
            priority_queue<ResourcePtr, vector<ResourcePtr>, ResourcePriorityComp> queueToWorker;
            ThreadConditionVariable cvToWorker;
            ThreadLock lockToWorker;

            deque<ResourcePtr> queueToGL;
            ThreadLock lockToGL;

            bool threadWillEnd;
            unordered_map<string, ResourcePtr> resourceHash;
        };

        shared_ptr<Fields> m_fields;

        static unique_ptr<ResourceManager> s_singleton;
    };


    class TextureProtocol
    {
    public:
        enum InnerFormat
        {
            RGBA_8888,
            RGBA_5551,
            RGBA_4444,
            RGB_888,
            RGB_565,
            Red_8,
            Gray_8,
            RGBA_8888_Palette_256,
            YUV, // !?  看看是不是可以用shader把YUV转为RGB
        };

        InnerFormat GetInnerFormat() const { return m_innerFormat; }
        int GetWidth() const { return m_width; }
        int GetHeight() const { return m_height; }
        GLuint GetGlTexture() const { return m_glTexture; }

    protected:
        InnerFormat m_innerFormat;
        int m_width;
        int m_height;
        GLuint m_glTexture;
    };


    class ImageAsTexture : public Resource, public TextureProtocol
    {
    public:
        ImageAsTexture(const string& name, float priority = 0) : Resource(name, priority), m_pixels(nullptr) { m_innerFormat = RGBA_8888; m_width = 0; m_height = 0; m_glTexture = 0; }
        ~ImageAsTexture();

    private:
        virtual bool Prepare_InGlThread();
        virtual bool Load_InWorkThread();
        virtual bool Finish_InGlThread();
        virtual void Timeout_InGlThread();

        GLubyte* m_pixels;
    };


//     class VideoAsTexture : public Resource, public TextureProtocol
//     {
//     public:
//         VideoAsTexture(const string& name, float priority = 0) : Resource(name, priority) { m_innerFormat = RGBA_8888; m_width = 0; m_height = 0; m_glTexture = 0; }
// 
//     private:
//         virtual bool Prepare_InGlThread();
//         virtual bool Load_InWorkThread();
//         virtual bool Finish_InGlThread();
//         virtual void Timeout_InGlThread();
//     };
}

#endif
