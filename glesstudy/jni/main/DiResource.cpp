#include "DiResource.h"

// #undef _UNICODE
// #include <IL/il.h>
// #pragma comment (lib, "DevIL.lib")

namespace di
{
    unique_ptr<ResourceManager> ResourceManager::s_singleton;


    ResourceManager::ResourceManager()
        : m_fields(new Fields)
    {
        m_fields->threadWillEnd = false;

        shared_ptr<Fields> fields = m_fields;

        ThreadEventHandlers handlers;

        handlers.onInit = []()
        {
//            ilInit();
//            ilEnable(IL_ORIGIN_SET);
//            ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
        };

        handlers.onLoop = [fields](bool* willEndThread, uint32_t* willWaitMillis)
        {
            // onLoop lambda begin

            Fields* f = fields.get();

            *willWaitMillis = 0;

            vector<ResourcePtr> vec;

            {
                // ThreadLockGuard lockToWorker(f->lockToWorker);
                f->cvToWorker.WaitUntil(
                    f->lockToWorker,
                    [f]() { return f->threadWillEnd || !f->queueToWorker.empty(); },
                    [willEndThread, f, &vec]()
                    {
                        *willEndThread = f->threadWillEnd;
                        if (*willEndThread)
                        {
                            return;
                        }

                        while (!f->queueToWorker.empty())
                        {
                            vec.push_back(f->queueToWorker.top());
                            f->queueToWorker.pop();
                        }
                    });
            }

            for (auto iter = vec.begin(); iter != vec.end(); ++iter)
            {
                if (*willEndThread)
                {
                    return;
                }

                ResourcePtrCR r = *iter;
                r->Load();

                ThreadLockGuard lock(f->lockToGL);
                f->queueToGL.push_back(r);
            }

            // onLoop lambda end
        };

        StartThread("Resource Loader", handlers);
    }


    ResourceManager::~ResourceManager()
    {
        m_fields->threadWillEnd = true;
    }


    void ResourceManager::AsyncLoadResource(ResourcePtrCR resource)
    {
        DI_SAVE_CALLSTACK();

        Resource::State state = resource->GetState();
        if (state != Resource::State::Init && state != Resource::State::Timeout)
        {
            if (state == Resource::State::Finished)
            {
                resource->UpdateTimeoutTick();
            }

            return;
        }

        resource->Prepare();
        if (resource->GetState() != Resource::State::Prepared)
        {
            return;
        }

        ThreadLockGuard lock(m_fields->lockToWorker);
        m_fields->queueToWorker.push(resource);
        m_fields->cvToWorker.Notify();
    }


    void ResourceManager::CheckAsyncFinishedResources()
    {
        DI_SAVE_CALLSTACK();

        ThreadLockGuard lock(m_fields->lockToGL);
        deque<ResourcePtr> queueToGL;
        queueToGL.swap(m_fields->queueToGL);
        lock.Unlock();

        for (auto iter = queueToGL.begin(); iter != queueToGL.end(); ++iter)
        {
            ResourcePtrCR resource = *iter;
            if (resource->GetState() == Resource::State::Loaded)
            {
                resource->Finish();
                if (resource->GetState() == Resource::State::Finished)
                {
                    resource->UpdateTimeoutTick();
                }
            }
        }
    }


    void ResourceManager::CheckTimeoutResources()
    {
        DI_SAVE_CALLSTACK();

        auto& resourceHash = m_fields->resourceHash;
        for (auto iter = resourceHash.begin(); iter != resourceHash.end(); ++iter)
        {
            ResourcePtrCR resource = (*iter).second;
//             if (resource.unique())
//             {
                resource->CheckTimeout();
//             }
        }
    }


    const ResourcePtr* ResourceManager::HashFindResource(const string& name)
    {
        DI_SAVE_CALLSTACK();

        auto& hash = m_fields->resourceHash;

        auto iter = hash.find(name);
        if (iter == hash.end())
        {
            return nullptr;
        }

        ResourcePtrCR resource = (*iter).second;

        Resource::State state = resource->GetState();
        DI_ASSERT(state != Resource::State::Init);

        AsyncLoadResource(resource);
        return &resource;
    }


    void ResourceManager::AddResource(ResourcePtrCR resource)
    {
        DI_SAVE_CALLSTACK();
        DI_ASSERT(resource->GetState() == Resource::State::Init);

        m_fields->resourceHash[resource->GetName()] = resource;

        AsyncLoadResource(resource);
    }


//    ImageAsTexture::~ImageAsTexture()
//    {
//        glDeleteTextures(1, &m_glTexture);
//
//        delete[] m_pixels;
//        m_pixels = nullptr;
//    }
//
//
//    bool ImageAsTexture::Prepare_InGlThread()
//    {
//        if (m_glTexture == 0)
//        {
//            glGenTextures(1, &m_glTexture);
//            glBindTexture(GL_TEXTURE_2D, m_glTexture);
//            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//            DI_DBG_CHECK_GL_ERRORS();
//        }
//
//        return true;
//    }
//
//
//    bool ImageAsTexture::Load_InWorkThread()
//    {
//        // TODO: not finished
//        // 暂时直接读取为RGBA
//
//        ILuint img = ilGenImage();
//        ilBindImage(img);
//
//        auto imgDeleter = MakeCallAtScopeExit([img]()
//        {
//            ilBindImage(0);
//            ilDeleteImage(img);
//        });
//
//        if (!ilLoad(IL_TYPE_UNKNOWN, GetName().c_str()))
//        {
//            return false;
//        }
//
//        ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);
//
//        m_width = ilGetInteger(IL_IMAGE_WIDTH);
//        m_height = ilGetInteger(IL_IMAGE_HEIGHT);
//        m_pixels = new GLubyte[m_width * m_height * 4];
//        ilCopyPixels(0, 0, 0, m_width, m_height, 1, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels);
//
//        return true;
//    }
//
//
//    bool ImageAsTexture::Finish_InGlThread()
//    {
//        glBindTexture(GL_TEXTURE_2D, m_glTexture);
//        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels);
//        DI_DBG_CHECK_GL_ERRORS();
//
//        delete[] m_pixels;
//        m_pixels = nullptr;
//
//        return true;
//    }
//
//
//    void ImageAsTexture::Timeout_InGlThread()
//    {
//        glDeleteTextures(1, &m_glTexture);
//        m_glTexture = 0;
//        m_width = 0;
//        m_height = 0;
//    }
}
