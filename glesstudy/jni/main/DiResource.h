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

    // �첽��Դ����
    //
    // ��Դ�������߳�֮��շת��һ����GL�̣߳�һ����worker�̡߳�
    // ������Դ��Ϊ�������裺
    //   1. ��GL�̣߳�����һЩ�����ĳ�ʼ��
    //   2. ��worker�̣߳�����ʵ�ʵ���Դ����
    //   3. ��GL�̣߳���ɼ��ض���
    //
    // ����˵����
    // ���ļ����ؾ�̬ͼƬ����1������GL�̵߳���glGenTexture��������Ĭ�����أ���2����worker�̼߳���ͼƬ���أ���3����GL�߳���������
    // ������Ƶ����Ҳ���Բ���������ʽ����δ�������1����GL�̵߳���glGenTexture����2����worker�̼߳���һ֡����3����GL�߳��������أ�֮�󷵻ص�2��
    // ��ʾ������ʱ������ȷ�Ļ���glTexSubImage2D���ܱ�glTexImage2D������Ҫע�⣬����PowerVR���Կ�����Ƶ������ܲ�����������RGB565֮��ĸ�ʽ��
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

        // ����������ResourceManager���á������ط���Ҫ��
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

        // Ӧ��ֻ��GL�߳�ʹ�����singleton
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
            YUV, // !?  �����ǲ��ǿ�����shader��YUVתΪRGB
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
