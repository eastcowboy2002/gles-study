#include "DiResource.h"
#include "SDL_image.h"

#include <ctime>

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

            f->cvToWorker.WaitUntil(
                f->lockToWorker,
                [f]() { return f->threadWillEnd || !f->queueToWorker.empty(); },
                [willEndThread, f, &vec]()
                {
                    *willEndThread = f->threadWillEnd;
                    if (*willEndThread)
                    {
                        LogInfo("willEndThread");
                        return;
                    }

                    // while (!f->queueToWorker.empty())
                    if (!f->queueToWorker.empty())
                    {
                        vec.push_back(f->queueToWorker.top());
                        f->queueToWorker.pop();
                    }
                });

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

        handlers.threadName = "Resource Loader";
        StartThread(handlers);
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
            else
            {
                // note:
                // Normally we only need to call cvToWorker.Notify() after queueToWorker.push(),
                // which is done at the end of this function.
                //
                // But if cvToWorker.Notify() is called before worker thread's "cvToWorker.WaitUntil" called,
                // we need to call cvToWorker.Notify() somewhere again.
                // Or the worker thread will wait for a long time

                ThreadLockGuard lock(m_fields->lockToWorker);
                if (!m_fields->queueToWorker.empty())
                {
                    m_fields->cvToWorker.Notify();
                }
            }

            return;
        }

        LogInfo("AsyncLoadResource '%s'", resource->GetName().c_str());

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


    class SDLTextureLoader : public BaseTextureLoader
    {
    public:
        SDLTextureLoader(const string& name) : BaseTextureLoader(name), m_imageSurface(nullptr) {}

        ~SDLTextureLoader()
        {
            glDeleteTextures(1, &m_glTexture);

            SDL_FreeSurface(m_imageSurface);
            m_imageSurface = nullptr;
        }

    private:
        virtual bool Prepare_InGlThread()
        {
            DI_SAVE_CALLSTACK();

            if (m_glTexture == 0)
            {
                glGenTextures(1, &m_glTexture);
                glBindTexture(GL_TEXTURE_2D, m_glTexture);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                DI_DBG_CHECK_GL_ERRORS();
            }

            return true;
        }


        virtual bool Load_InWorkThread()
        {
            DI_SAVE_CALLSTACK();

            if (m_imageSurface)
            {
                LogWarn("load new SDL surface while the old surface is still exist. resource: '%s'", GetName().c_str());
                m_imageSurface = nullptr;
            }

            m_imageSurface = IMG_Load(GetName().c_str());

            if (m_imageSurface)
            {
                m_width = m_imageSurface->w;
                m_height = m_imageSurface->h;

                if (m_imageSurface->format->Amask != 0 || SDL_GetColorKey(m_imageSurface, NULL))
                {
                    m_innerFormat = TextureProtocol::RGBA_8888;
                }
                else
                {
                    m_innerFormat = TextureProtocol::RGB_888;
                }

                return true;
            }
            else
            {
                LogError("IMG_Load('%s') failed", GetName().c_str());
                return false;
            }
        }


        virtual bool Finish_InGlThread()
        {
            DI_SAVE_CALLSTACK();

            DI_ASSERT(m_imageSurface);
            DI_ASSERT(m_glTexture);

            Uint32 sdlFormat;
            GLenum glFormat;
            if (m_imageSurface->format->Amask != 0 || SDL_GetColorKey(m_imageSurface, NULL) == 0)
            {
                sdlFormat = SDL_PIXELFORMAT_ABGR8888;   // surface has alpha, so use GL_RGBA
                glFormat = GL_RGBA;
            }
            else
            {
                sdlFormat = SDL_PIXELFORMAT_RGB24;      // surface has no alpha, so use GL_RGB
                glFormat = GL_RGB;
            }

            SDL_Surface* readingSurface;
            SDL_Surface* tmpSurface = nullptr;
            if (sdlFormat == m_imageSurface->format->format)    // no need to change format
            {
                readingSurface = m_imageSurface;
            }
            else                                                // need to change format
                                                                // Normally we don't go here because blit operation has some performance cost
                                                                // maybe move SDL_BlitSurface to ResourceManager's work thread is an optimization
            {
                LogWarn("SDL surface type need change. origin: %s, destination: %s",
                    SDL_GetPixelFormatName(m_imageSurface->format->format), SDL_GetPixelFormatName(sdlFormat));

                int pixelDepth;
                Uint32 Rmask, Gmask, Bmask, Amask;
                SDL_PixelFormatEnumToMasks(sdlFormat, &pixelDepth, &Rmask, &Gmask, &Bmask, &Amask);

                tmpSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, m_width, m_height, pixelDepth, Rmask, Gmask, Bmask, Amask);
                if (!tmpSurface)
                {
                    LogError("SDL_CreateRGBSurface failed, resource: '%s'", GetName().c_str());
                    SDL_FreeSurface(m_imageSurface);
                    m_imageSurface = nullptr;
                    return false;
                }

                SDL_BlitSurface(m_imageSurface, nullptr, tmpSurface, nullptr);

                readingSurface = tmpSurface;
            }

            if (readingSurface->pitch % 4 != 0)
            {
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
            }

            if (SDL_MUSTLOCK(readingSurface))
            {
                SDL_UnlockSurface(readingSurface);
            }

            glTexImage2D(GL_TEXTURE_2D, 0, glFormat, m_width, m_height, 0, glFormat, GL_UNSIGNED_BYTE, readingSurface->pixels);

            if (SDL_MUSTLOCK(readingSurface))
            {
                SDL_UnlockSurface(readingSurface);
            }

            if (readingSurface->pitch % 4 != 0)
            {
                glPixelStorei(GL_PACK_ALIGNMENT, 4);
            }

            SDL_FreeSurface(tmpSurface);
            SDL_FreeSurface(m_imageSurface);
            m_imageSurface = nullptr;
            return true;
        }


        virtual void Timeout_InGlThread()
        {
            glDeleteTextures(1, &m_glTexture);
            m_glTexture = 0;
            m_width = 0;
            m_height = 0;

            SDL_FreeSurface(m_imageSurface);
            m_imageSurface = nullptr;
        }

        SDL_Surface* m_imageSurface;
    };


    class KTXTextureLoader : public BaseTextureLoader
    {
    public:
        KTXTextureLoader(const string& name) : BaseTextureLoader(name) {}

    private:
        virtual bool Prepare_InGlThread()
        {
            return true;
        }


        virtual bool Load_InWorkThread()
        {
            DI_SAVE_CALLSTACK();

            if (!m_bytes.empty())
            {
                LogWarn("load new KTX bytes while the old bytes is still exist. resource: '%s'", GetName().c_str());
                m_bytes.clear();
            }

            SDL_RWops* rw = SDL_RWFromFile(GetName().c_str(), "rb");
            if (!rw)
            {
                LogError("SDL_RWFromFile('%s') failed", GetName().c_str());
                return false;
            }

            auto rwDeleter = MakeCallAtScopeExit([rw](){ SDL_RWclose(rw); });

            m_bytes.resize(size_t(SDL_RWsize(rw)));
            if (!m_bytes.empty())
            {
                if (SDL_RWread(rw, &m_bytes[0], m_bytes.size(), 1) != 1)
                {
                    vector<uint8_t>().swap(m_bytes);
                    LogError("SDL_RWread('%s') failed", GetName().c_str());
                    return false;
                }
            }
            else
            {
                LogError("KTX file '%s' is empty", GetName().c_str());
                return false;
            }

            return true;
        }


        virtual bool Finish_InGlThread()
        {
            DI_SAVE_CALLSTACK();

            DI_ASSERT(!m_bytes.empty());

            GLuint tex;
            GLenum target;
            GLenum glerr;
            GLboolean isMipmap;
            KTX_dimensions dimensions;
            KTX_error_code ktxErr = ktxLoadTextureM(&m_bytes[0], m_bytes.size(), &tex, &target, &dimensions, &isMipmap, &glerr, 0, NULL);

            vector<uint8_t>().swap(m_bytes);

            if (ktxErr != KTX_SUCCESS || glerr != GL_NO_ERROR)
            {
                LogError("ktxLoadTextureM('%s') failed. ktxErr = 0x%X, glerr = 0x%X", GetName().c_str(), ktxErr, glerr);
                return false;
            }

            if (target != GL_TEXTURE_2D)
            {
                glBindTexture(target, 0);
                glDeleteTextures(1, &target);
                LogError("ktxLoadTextureM('%s') not a 2D texture", GetName().c_str());
                return false;
            }

	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, isMipmap ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		    m_width = dimensions.width;
		    m_height = dimensions.height;
            m_glTexture = tex;
            m_innerFormat = InnerFormat::RGB_888;

            return true;
        }


        virtual void Timeout_InGlThread()
        {
            DI_SAVE_CALLSTACK();

            glDeleteTextures(1, &m_glTexture);
            m_glTexture = 0;
            m_width = 0;
            m_height = 0;

            vector<uint8_t>().swap(m_bytes);
        }


        vector<uint8_t> m_bytes;
    };


    ImageAsTexture::ImageAsTexture(const string& name, float priority /* = 0 */ )
        : Resource(name, priority), m_loader(nullptr)
    {
        size_t pos = name.find_last_of('.');
        if (pos != string::npos && SDL_strncasecmp(name.c_str() + pos + 1, "ktx", 3) == 0)
        {
            m_loader.reset(new KTXTextureLoader(name));
        }
        else
        {
            m_loader.reset(new SDLTextureLoader(name));
        }
    }


    bool ImageAsTexture::Prepare_InGlThread()
    {
        return m_loader->Prepare_InGlThread();
    }


    bool ImageAsTexture::Load_InWorkThread()
    {
        return m_loader->Load_InWorkThread();
    }


    bool ImageAsTexture::Finish_InGlThread()
    {
        return m_loader->Finish_InGlThread();
    }


    void ImageAsTexture::Timeout_InGlThread()
    {
        m_loader->Timeout_InGlThread();
    }
}
