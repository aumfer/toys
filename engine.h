#include <platform.h>

#ifndef __EMSCRIPTEN__
#include <curl/curl.h>
#else
#include <emscripten/fetch.h>
#endif
//#include <hb.h>
#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>
#include <unordered_map>

#ifdef __EMSCRIPTEN__
extern "C" void em_glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height);
#endif

inline HMM_Mat4 HMM_TranslateRotateScale(HMM_Mat4 t, HMM_Mat4 r, HMM_Mat4 s) {
    return HMM_MulM4(t,HMM_MulM4(r,s));
}
namespace engine {
    template<int Count=1>
    struct fetchs {
#ifndef __EMSCRIPTEN__
    CURLM *fetch;
    CURLMcode readyState;
    struct fetch_request {
        struct fetchs<Count> *fetch;
        CURL *curl;
        char *data;
        int datalen;
        int done;
    } request[Count];
#else
    struct fetch_request {
        emscripten_fetch_attr_t emscripten;
        emscripten_fetch_t *fetch;
        int init,done;
    } request[Count];
#endif
#ifndef __EMSCRIPTEN__
static size_t curl_write_fetch(void *contents, size_t size, size_t nmemb, void *userp) {
    struct fetch_request *r = (struct fetch_request *)userp;
    int len = r->datalen + size*nmemb;
    r->data = (char *)realloc(r->data, len); //len+1,\0 term?
    memcpy(r->data+r->datalen, contents, nmemb*size);
    r->datalen = len;
    return size*nmemb;
}
static size_t curl_write_fetch_header(char* buffer, size_t size, size_t nitems, void* userp) {
    struct fetch_request *r = (struct fetch_request *)userp;
    size_t contentLength;
    if (sscanf(buffer, "Content-Length: %zu", &contentLength)&&contentLength>r->datalen) {
        r->data = (char *)realloc(r->data,contentLength);
    }
    return size*nitems;
}
#else
static void onreadystatechange(emscripten_fetch_t *fetch) {
    struct fetch_request *r = (struct fetch_request *)fetch->userData;
    r->fetch = fetch;
    r->done = 1;
}
#endif
    template<typename R=void(const char *data, int len)>
    inline void operator()(int i, const char *url, R &&r, int len=0) {
#ifndef __EMSCRIPTEN__
        int running;
        int msgq;
        CURLMsg *m;
#else
#endif
#ifndef __EMSCRIPTEN__
        if (!fetch) goto create;
        if (readyState != CURLM_OK) goto error;
#else
#endif
        alive:
#ifndef __EMSCRIPTEN__
        if (!request[i].curl) {
            request[i].fetch = this;
            request[i].curl = curl_easy_init();
            curl_easy_setopt(request[i].curl, CURLOPT_URL, url);
            curl_easy_setopt(request[i].curl, CURLOPT_WRITEFUNCTION, curl_write_fetch);
            curl_easy_setopt(request[i].curl, CURLOPT_WRITEDATA, &request[i]);
            curl_easy_setopt(request[i].curl, CURLOPT_HEADERFUNCTION, curl_write_fetch_header);
            curl_easy_setopt(request[i].curl, CURLOPT_HEADERDATA, &request[i]);
            curl_multi_add_handle(fetch,request[i].curl);
            if (len) request[i].data = (char *)malloc(len);
        }
        readyState = curl_multi_perform(fetch,&running);
        if (readyState != CURLM_OK) {
            fprintf(stderr, "%s\n", curl_multi_strerror(readyState));
            goto error;
        }
        m = curl_multi_info_read(fetch,&msgq);
        if (m && (m->msg == CURLMSG_DONE)) {
            for (int _i = 0; _i < Count; ++_i) {
                if (m->easy_handle == request[_i].curl) {
                    request[_i].done = 1;
                    break;
                }
            }
        }
        if (request[i].done) {
            if (!request[i].data || !request[i].datalen) {
                //char *url = NULL;
                //curl_easy_getinfo(request[i].curl, CURLINFO_EFFECTIVE_URL, &url);
                fprintf(stderr, "curl no data for %s\n",url);
            }
            r(request[i].data,request[i].datalen);
            fprintf(stdout, "fetched %d bytes from %s\n",request[i].datalen,url);
            //free(request[i].data); //free here to match emscripten?
            request[i].done = 0;
        }
#else
        if (!request[i].init){
            request[i].init = 1;
            emscripten_fetch_attr_init(&request[i].emscripten);
            strcpy(request[i].emscripten.requestMethod,"GET");
            request[i].emscripten.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
            request[i].emscripten.onsuccess = onreadystatechange;
            request[i].emscripten.userData = &request[i];
            emscripten_fetch(&request[i].emscripten,url);
        }
        if (request[i].done) {
            if (!request[i].fetch->data || !request[i].fetch->numBytes) {
                fprintf(stderr, "fetch no data for %s\n",url);
            }
            r(request[i].fetch->data,request[i].fetch->numBytes);
            emscripten_fetch_close(request[i].fetch);
            request[i].done = 0;
        }
#endif
        return;
        create:
#ifndef __EMSCRIPTEN__
        this->fetch = curl_multi_init();
#else
#endif
        goto alive;
        return;
        error:
        return;
    }
    };
    struct fetch : fetchs<1> {
        template<typename R=void(const char *data, int len)>
        inline void operator()(const char *url, R &&r, int len=0) {
            fetchs<1>::operator()(0,url,r,len);
        }
    };
    struct window {
        GLFWwindow *window;
        int width,height;
        unsigned input[64];
        int input_cursor = 0;
        static struct Init {
            int glfw = 0;
        } init;
        operator GLFWwindow*() const {return window;}
        inline void operator()(int x, int y, int w, int h, const char *title) {
            GLFWmonitor *monitor;
            const GLFWvidmode *vidmode;
            double _mx,_my;
            if (!init.glfw) init.glfw = glfwInit();
            if (!init.glfw) goto error;
            if (!window) goto create;
            alive:
            glfwSwapBuffers(window);
#ifndef __EMSCRIPTEN__
            glfwGetFramebufferSize(window,&width,&height);
#else
            em_glfwGetFramebufferSize(window,&width,&height);
#endif
            glfwPollEvents();
            return;
            create:
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
            //glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#if _DEBUG
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
            monitor = glfwGetPrimaryMonitor();
            vidmode = glfwGetVideoMode(monitor);
            //glfwWindowHint(GLFW_SAMPLES, 4);
            glfwWindowHint(GLFW_RED_BITS, vidmode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, vidmode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, vidmode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, vidmode->refreshRate);
            glfwWindowHint(GLFW_DEPTH_BITS, 16);
            glfwWindowHint(GLFW_STENCIL_BITS, 0);
            //glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
            window = glfwCreateWindow(w,h,title,nullptr,0);
            glfwSetWindowUserPointer(window,this);
            //glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);
            glfwMakeContextCurrent(window);
#ifndef __EMSCRIPTEN__
#if _DEBUG
            glfwSwapInterval(1);
#else
            glfwSwapInterval(0);
#endif
#endif
#ifndef __EMSCRIPTEN__
            gladLoadGLES2(glfwGetProcAddress);
#endif
            //glfwSetCharCallback(window, [](GLFWwindow *window, unsigned codepoint) {
            //    struct window *w = (struct window *)glfwGetWindowUserPointer(window);
            //    switch (codepoint) {
            //    default:w->input[w->input_cursor++ & 63] = codepoint; break;
            //        case '\r':w->input[w->input_cursor&63]=w->input_cursor=0;break;
            //        case '\n':break;
            //    }
            //});
            goto alive;
            error:{}
        }
    };
    struct buffer {
        GLuint buffer;
        GLenum _target;
        int _size;
        operator GLuint() const {return buffer;}
        inline void operator()(GLenum target, int size, GLenum usage=GL_STATIC_DRAW) {
            if (_target!=target) _target=target;
            if (_size!=size) _size=size;
            if (!buffer) {
                glGenBuffers(1,&buffer);
                glBindBuffer(target,buffer);
                glBufferData(target,size,0,usage);
                printf("buffer %d\n",size);
            } else {
                glBindBuffer(target,buffer);
            }
        }
        inline void upload(const void *data, int offset=0, int len=0) {
            if (!len) len=_size-offset;
            if (offset+len>_size||offset<0) {
                printf("buffer upload oob %d+%d in %d\n",offset,len,_size);
                return;
            }
            glBufferSubData(_target,offset,len,data);
        }
    };
#define MAKE_FOURCC(a, b, c, d) \
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define DXT1 MAKE_FOURCC('D','X','T','1')
#define PVRTC4 MAKE_FOURCC('P','T','C','4')
#define PVR3 MAKE_FOURCC('P','V','R','3')
    inline uint32_t read_little_endian_uint32(const uint8_t *ptr) {
        return (uint32_t)ptr[0] |
            ((uint32_t)ptr[1] << 8) |
            ((uint32_t)ptr[2] << 16) |
            ((uint32_t)ptr[3] << 24);
    }
    struct texturebase {
        union { GLuint texture, _texture; };//compiler workaround
        int width,height;
        GLint _format;
        GLint _min_filter, _mag_filter;
        GLint _wrap_s, _wrap_t;
        operator GLuint() const {return texture;}
        inline void operator()(GLint min_filter=GL_NEAREST, GLint mag_filter=GL_NEAREST, GLint wrap_s=GL_CLAMP_TO_EDGE, GLint wrap_t=GL_CLAMP_TO_EDGE) {
            if (!texture) goto create;
            alive:
            glBindTexture(GL_TEXTURE_2D,texture);
            if (_min_filter != min_filter) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
                _min_filter = min_filter;
            }
            if (_mag_filter != mag_filter) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
                _mag_filter = mag_filter;
            }
            if (_wrap_s != wrap_s) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
                _wrap_s = wrap_s;
            }
            if (_wrap_t != wrap_t) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
                _wrap_t = wrap_t;
            }
            return;
            create:
            glGenTextures(1, &texture);
            goto alive;
            error:
            return;
        }
        inline void upload_dds(const void *data) {
            uint8_t *_data = (uint8_t*)data;
            uint32_t offset = read_little_endian_uint32(_data+4);
            uint32_t flags = read_little_endian_uint32(_data+8);
            height = read_little_endian_uint32(_data+12);
            width = read_little_endian_uint32(_data+16);
            union {
                unsigned u;
                char s[4];
            } magic;
            magic.u = MAKE_FOURCC(_data[84],_data[85],_data[86],_data[87]);
            uint32_t dwFlags = read_little_endian_uint32(_data+80);
            uint32_t dwABitMask = read_little_endian_uint32(_data+108);
            bool hasAlpha = dwFlags&1||dwABitMask;
            bool isPVRTC4 = (magic.u == MAKE_FOURCC('P', 'T', 'C', '4'));
            if (isPVRTC4) {
                _format = hasAlpha ? GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG : GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
            } else if (hasAlpha) {
                switch (magic.s[3]) {
                    case '1': _format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
                    case '3': _format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
                    case '5': _format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
                }
            } else {
                switch (magic.s[3]) {
                    case '1': _format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT; break;
                }
            }
            GLint miplevel = miplevel = read_little_endian_uint32(_data+7*4);
            if (!(flags&0x20000)) miplevel = 1;

            size_t start = offset+4;
            GLsizei w = width;
            GLsizei h = height;
            for (int i = 0; i < miplevel; ++i) {
                GLuint imageSize;
                if (isPVRTC4) {
                    // PVRTC 4bpp requires a minimum allocation of 8x8 pixels (32 bytes) per mip level
                    GLsizei blockW = std::max(8, w);
                    GLsizei blockH = std::max(8, h);
                    imageSize = (blockW * blockH * 4 + 7) / 8;
                } else {
                    //imageSize = std::max(1,w/4) * std::max(1,h/4) * (magic.u == DXT1 ? 8 : 16);
                    imageSize = std::max(1, (w + 3) / 4) * std::max(1, (h + 3) / 4) 
                        * (magic.u == DXT1 ? 8 : 16);
                }
                glCompressedTexImage2D(GL_TEXTURE_2D, i, _format, w, h, 0, imageSize, (unsigned char*)_data+start);
                w = std::max(1, w / 2);
                h = std::max(1, h / 2);
                start += imageSize;
            }
            //if (miplevel>1) _min_filter = GL_NEAREST_MIPMAP_LINEAR;
        }
        inline void upload_pvr(const void *data) {
            const uint8_t *_data = (const uint8_t*)data;

            // PVRv3 Header (52 bytes)
            uint32_t version = read_little_endian_uint32(_data + 0);
            if (version != 0x03525650) { // 'P', 'V', 'R', 3 in little-endian
                return; // Not a valid PVRv3 file
            }

            // Pixel format is a 64-bit value split into two 32-bit words (offsets 8 and 12)
            uint32_t pixelFormatLow  = read_little_endian_uint32(_data + 8);
            uint32_t pixelFormatHigh = read_little_endian_uint32(_data + 12);

            height = read_little_endian_uint32(_data + 24);
            width  = read_little_endian_uint32(_data + 28);
            GLint miplevel = read_little_endian_uint32(_data + 44);
            if (miplevel == 0) miplevel = 1;

            uint32_t metaDataSize = read_little_endian_uint32(_data + 48);
            size_t start = 52 + metaDataSize;

            // Block dimensions and bits-per-pixel for size calculation
            GLsizei blockWidth = 4;
            GLsizei blockHeight = 4;
            GLuint bpp = 4;
            GLsizei minBlocksX = 1;
            GLsizei minBlocksY = 1;

            // If pixelFormatHigh == 0, pixelFormatLow contains a standard PVRv3 enum
            if (pixelFormatHigh == 0) {
                switch (pixelFormatLow) {
                    case 0: // PVRTC 2bpp RGB
                        _format = GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
                        blockWidth = 8; blockHeight = 4; bpp = 2;
                        minBlocksX = 2; minBlocksY = 2;
                        break;
                    case 1: // PVRTC 2bpp RGBA
                        _format = GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
                        blockWidth = 8; blockHeight = 4; bpp = 2;
                        minBlocksX = 2; minBlocksY = 2;
                        break;
                    case 2: // PVRTC 4bpp RGB
                        _format = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
                        blockWidth = 4; blockHeight = 4; bpp = 4;
                        minBlocksX = 2; minBlocksY = 2;
                        break;
                    case 3: // PVRTC 4bpp RGBA
                        _format = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
                        blockWidth = 4; blockHeight = 4; bpp = 4;
                        minBlocksX = 2; minBlocksY = 2;
                        break;
                    case 6: // ETC1
                        _format = GL_ETC1_RGB8_OES;
                        bpp = 4;
                        break;
                    case 7: // DXT1
                        _format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                        bpp = 4;
                        break;
                    case 9: // DXT3
                        _format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                        bpp = 8;
                        break;
                    case 11: // DXT5
                        _format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                        bpp = 8;
                        break;
                    case 22: // ETC2 RGB
#ifndef __EMSCRIPTEN__
                        fprintf(stderr, "GL_COMPRESSED_RGB8_ETC2 not supported\n");
                        return;
#else
                        _format = GL_COMPRESSED_RGB8_ETC2;
                        bpp = 4;
                        break;
#endif
                    case 23: // ETC2 RGBA
#ifndef __EMSCRIPTEN__
                        fprintf(stderr, "GL_COMPRESSED_RGBA8_ETC2_EAC not supported\n");
                        return;
#else
                        _format = GL_COMPRESSED_RGBA8_ETC2_EAC;
                        bpp = 8;
                        break;
#endif
                    case 24: // ETC2 RGB A1
#ifndef __EMSCRIPTEN__
                        fprintf(stderr, "GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 not supported\n");
                        return;
#else
                        _format = GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
                        bpp = 4;
                        break;
#endif
                    case 27: // ASTC 4x4
                        _format = GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
                        blockWidth = 4; blockHeight = 4; bpp = 8;
                        break;
                    case 31: // ASTC 6x6
                        _format = GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
                        blockWidth = 6; blockHeight = 6; bpp = 128 / 36; // Handled via block bytes below
                        break;
                    case 34: // ASTC 8x8
                        _format = GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
                        blockWidth = 8; blockHeight = 8;
                        break;
                    default:
                        return; // Unsupported format
                }
            } else {
                return; // Uncompressed / custom channel layouts (pixelFormatHigh != 0)
            }

            // Bytes per block (all standard compressed blocks above are either 8 bytes or 16 bytes)
            GLuint bytesPerBlock = (blockWidth * blockHeight * bpp) / 8;
            if (_format >= GL_COMPRESSED_RGBA_ASTC_4x4_KHR && _format <= GL_COMPRESSED_RGBA_ASTC_12x12_KHR) {
                bytesPerBlock = 16; // Every ASTC block size is strictly 16 bytes (128 bits)
            }

            GLsizei w = width;
            GLsizei h = height;

            for (int i = 0; i < miplevel; ++i) {
                GLsizei blocksX = std::max(minBlocksX, (w + blockWidth - 1) / blockWidth);
                GLsizei blocksY = std::max(minBlocksY, (h + blockHeight - 1) / blockHeight);
                GLuint imageSize = blocksX * blocksY * bytesPerBlock;

                glCompressedTexImage2D(GL_TEXTURE_2D, i, _format, w, h, 0, imageSize, _data + start);

                w = std::max(1, w / 2);
                h = std::max(1, h / 2);
                start += imageSize;
            }
        }
    };
    struct texture : texturebase {
        inline void operator()(int w, int h, GLint format=GL_RGBA, GLint min_filter=GL_NEAREST, GLint mag_filter=GL_NEAREST, GLint wrap_s=GL_CLAMP_TO_EDGE, GLint wrap_t=GL_CLAMP_TO_EDGE) {
            texturebase::operator()(min_filter,mag_filter,wrap_s,wrap_t);
            if (_format != format) _format = format;
            if (width!=w||height!=h) {
                glTexImage2D(GL_TEXTURE_2D,0, format,w,h,0, format,GL_UNSIGNED_BYTE,0);
                width=w;height=h;
                printf("texture %dx%d\n",w,h);
            }
        }
        inline void upload(const void *data, int x=0, int y=0, int w=0, int h=0) {
            if (!w) w=width-x;
            if (!h) h=height-y;
            if (x+w>width||y+h>height||x<0||y<0) {
                printf("texture upload oob %d %d %d %d in %d %d\n",x,y,w,h,width,height);
                return;
            }
            if (w&&h) {
                glTexSubImage2D(GL_TEXTURE_2D,0, x,y,w,h, _format,GL_UNSIGNED_BYTE, data);
                if (!_min_filter || _min_filter == GL_NEAREST_MIPMAP_NEAREST || _min_filter == GL_NEAREST_MIPMAP_LINEAR || _min_filter == GL_LINEAR_MIPMAP_NEAREST || _min_filter == GL_LINEAR_MIPMAP_LINEAR) {
                    glGenerateMipmap(GL_TEXTURE_2D);
                }
            }
        }
    };
    inline bool endswith(const char *str, const char *suffix) {
        if (!str || !suffix) {
            return false;
        }

        size_t str_len = strlen(str);
        size_t suffix_len = strlen(suffix);

        if (suffix_len > str_len) {
            return false;
        }

        return memcmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
    }
    struct textureimage : texture {
        engine::fetch fetch;
        struct decoded {
            int width,height,depth;
            void *data;
        } decoded; 
        inline void operator()(const char *url, GLint min_filter=GL_LINEAR, GLint mag_filter=GL_LINEAR, GLint wrap_s=GL_CLAMP_TO_EDGE, GLint wrap_t=GL_CLAMP_TO_EDGE, int compressed=0) {
            texturebase::operator()(min_filter,mag_filter,wrap_s,wrap_t);
            fetch(url,[url,this](const char *data, int len) {
                decode d;
                if (endswith(url,".dds")) {
                    upload_dds(data);
#ifndef __EMSCRIPTEN__
                    free((void *)data);
#endif
                } else if (endswith(url,".pvr")) {
                    upload_pvr(data);
#ifndef __EMSCRIPTEN__
                    free((void *)data);
#endif
                } else {
#ifndef __EMSCRIPTEN__
                d.data = (stbi_uc*)data;
#else
                d.data = (stbi_uc *)malloc(len);
                memcpy(d.data,data,len);
#endif
                d.len = len;
                d.texture = this;
                init.decoder.q.get_subscriber().on_next(d);
                }
            });
            if (decoded.data) {
                static GLenum formats[] = {
                    0,
                    GL_LUMINANCE,
                    GL_LUMINANCE_ALPHA,
                    GL_SRGB_EXT,
                    GL_RGBA
                };
                texture::operator()(decoded.width,decoded.height,formats[decoded.depth],min_filter,mag_filter,wrap_s,wrap_t);
                upload(decoded.data);
                stbi_image_free(decoded.data); decoded.data = 0;
            }
        }
        struct decode {
            stbi_uc *data;
            int len;
            struct textureimage *texture;
        };
        struct decoder {
            decoder() {
                q.get_observable()
                .observe_on(rxcpp::observe_on_new_thread())
                .subscribe(subscription, [](struct decode decode) {
                    decode.texture->decoded.data = stbi_load_from_memory(decode.data,decode.len,&decode.texture->decoded.width,&decode.texture->decoded.height,&decode.texture->decoded.depth,0);
                    if (!decode.texture->decoded.data) fprintf(stderr,"decoder err %s\n",stbi_failure_reason());
                    free((void *)decode.data);
                });
            }
            ~decoder() {
                subscription.unsubscribe();
            }
            rxcpp::subjects::subject<struct decode> q;
            private:
            rxcpp::composite_subscription subscription;
        };
        static struct Init {
            struct decoder decoder;
        } init;
    };
    struct framebuffer : texture {
        int error;
        GLuint framebuffer;
        operator GLuint() const {return framebuffer;}
        void operator()(int w, int h, GLint format=GL_RGBA) {
            if (error) goto error;
            texture::operator()(w,h);
            if (!framebuffer) {
                GLenum _framebuffer_status;
                glGenFramebuffers(1,&framebuffer);
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texturebase::texture, 0);
                _framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                if (_framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
                    fputs("framebuffer error\n", stderr);
                    error=1;
                    goto error;
                }
            }
            return;
            error:
            return;
        }
        inline void download(void *pixels, int x=0, int y=0, int w=0, int h=0) {
            if (!w) w=width;
            if (!h) h=height;
            if (x+w>width||y+h>height||x<0||y<0) {
                printf("framebuffer download oob %d %d %d %d in %d %d\n",x,y,w,h,width,height);
                return;
            }
            glReadPixels(x,y,w,h,_format,GL_UNSIGNED_BYTE,pixels);
        }
    };
    struct program {
        int error;
        GLuint program,vs,fs;
        operator GLuint() const {return program;}
        inline void operator()(std::vector<const char *> vs_src, std::vector<const char *> fs_src) {
            GLint _program_link, _vs_compile, _fs_compile;
            if (!program) goto create;
            if (error) goto error;
            alive:
            glUseProgram(program);
            return;
            create:
            program = glCreateProgram();
            vs = glCreateShader(GL_VERTEX_SHADER);
            fs = glCreateShader(GL_FRAGMENT_SHADER);
            fs_src.insert(fs_src.begin(), "precision highp float;\n"); //
            glShaderSource(vs, vs_src.size(), &vs_src.front(), 0);
            glShaderSource(fs, fs_src.size(), &fs_src.front(), 0);
            glCompileShader(vs);
            glGetShaderiv(vs, GL_COMPILE_STATUS, &_vs_compile);
            if (!_vs_compile) {
                GLint _info_log_length;
                glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &_info_log_length);
                if (_info_log_length) {
                    GLchar *_info_log;
                    _info_log = (GLchar *)malloc(_info_log_length);
                    glGetShaderInfoLog(vs, _info_log_length, &_info_log_length, _info_log);
                    fputs(_info_log, stderr);
                    free(_info_log);
                }
                fputs("vertex shader compile error\n", stderr);
                error=1;
                goto error;
            }
            glCompileShader(fs);
            glGetShaderiv(fs, GL_COMPILE_STATUS, &_fs_compile);
            if (!_fs_compile) {
                GLint _info_log_length;
                glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &_info_log_length);
                if (_info_log_length) {
                    GLchar *_info_log;
                    _info_log = (GLchar *)malloc(_info_log_length);
                    glGetShaderInfoLog(fs, _info_log_length, &_info_log_length, _info_log);
                    fputs(_info_log, stderr);
                    free(_info_log);
                }
                fputs("fragment shader compile error\n", stderr);
                error=1;
                goto error;
            }
            glAttachShader(program, vs);
            glAttachShader(program, fs);
            glLinkProgram(program);
            glGetProgramiv(program, GL_LINK_STATUS, &_program_link);
            if (!_program_link) {
                GLint _info_log_length;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &_info_log_length);
                if (_info_log_length) {
                    GLchar *_info_log;
                    _info_log = (GLchar *)malloc(_info_log_length);
                    glGetProgramInfoLog(program, _info_log_length, &_info_log_length, _info_log);
                    fputs(_info_log, stderr);
                    free(_info_log);
                }
                fputs("program link error\n", stderr);
                error=1;
                goto error;
            }
            goto alive;
            error:return;
        }
    };
    struct attribute {
        int error;
        GLint location = -1;
        operator GLint() const {return location;}
        inline void operator()(const char *name, engine::program &p) {
            if (error) goto error;
            if (location < 0) {
                location = glGetAttribLocation(p,name);
            }
            if (location<0) {
                fprintf(stderr,"missing attribute %s\n",name);
                error=1;
                goto error;
            }
            return;
            error:;
        }
        inline void enable(GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
            if (error) return;
            if (location<0) return;
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location,size,type,normalized,stride,pointer);
        }
        inline void disable() {
            if (error) return;
            if (location<0) return;
            glDisableVertexAttribArray(location);
        }
        inline void set4f(float x, float y, float z, float w) {
            if (error) return;
            if (location<0) return;
            glVertexAttrib4f(location,x,y,z,w);
        }
    };
    struct draw_box {
        int error;
        engine::buffer buffer;
        engine::attribute position_attribute;
        inline void operator()(int x, int y, int w, int h, engine::program &p) {
            static GLbyte quaddata[] = {SCHAR_MIN,SCHAR_MIN,SCHAR_MAX,SCHAR_MIN,SCHAR_MIN,SCHAR_MAX,SCHAR_MAX,SCHAR_MAX};
            if (error) goto error;
            if (!buffer) {
                buffer(GL_ARRAY_BUFFER,sizeof(quaddata)/sizeof(quaddata[0]));
                buffer.upload(quaddata);
            } else {
                buffer(GL_ARRAY_BUFFER,sizeof(quaddata)/sizeof(quaddata[0]));
            }
            position_attribute("position",p);
            position_attribute.enable(2,GL_BYTE,GL_TRUE,0,0);
            glViewport(x,y,w,h);
            glDrawArrays(GL_TRIANGLE_STRIP,0,4);
            return;
            error:return;
        }
    };
    struct uniform {
        int error;
        GLint location = -1;
        operator GLint() const {return location;}
        inline void operator()(const char *name, engine::program &p) {
            if (error) goto error;
            if (location<0) {
                location = glGetUniformLocation(p,name);
            }
            if (location<0) {
                fprintf(stderr,"missing uniform %s\n",name);
                error=1;
                goto error;
            }
            return;
            error:return;
        }
    };
    struct uniform1i : uniform {
        inline void operator()(const char *name, engine::program &p, GLint value) {
            uniform::operator()(name,p);
            if (location>=0) glUniform1i(location,value);
        }
    };
    struct uniform1f : uniform {
        inline void operator()(const char *name, engine::program &p, GLfloat value) {
            uniform::operator()(name,p);
            if (location>=0) glUniform1f(location,value);
        }
    };
    struct uniform2f : uniform {
        inline void operator()(const char *name, engine::program &p, GLfloat x, GLfloat y) {
            uniform::operator()(name,p);
            if (location>=0) glUniform2f(location,x,y);
        }
    };
    struct uniform4fv : uniform {
        inline void operator()(const char *name, engine::program &p, const GLfloat *v, int count=1) {
            uniform::operator()(name,p);
            if (location>=0) glUniform4fv(location,count,v);
        }
    };
    struct uniformMatrix4fv : uniform {
        inline void operator()(const char *name, engine::program &p, const GLfloat *v, int count=1) {
            uniform::operator()(name,p);
            if (location>=0) glUniformMatrix4fv(location,count,GL_FALSE,v);
        }
    };
    struct blitprogram {
        engine::program program;
        engine::draw_box draw_box;
        engine::uniform1i blit_uniform;
        engine::uniform2f blitsize_uniform;
        engine::uniform1f turns_uniform;
        inline void operator()(int x, int y, int w, int h, engine::texture &texture, float turns=0) {
            program({_TEXT(
            attribute vec2 position;
            varying vec2 uv;
            void main() {
                uv = position*0.5+0.5;
                gl_Position = vec4(position, 0.0, 1.0);
            }
            )},{_TEXT(
            varying vec2 uv;
            uniform sampler2D blit;
            uniform vec2 blitsize;
            uniform float turns;
            void main() {
                vec3 color;
                vec2 _uv;
                float _turns = fract(turns);//mod(turns,1.0); //0,1
                if (_turns<0.25) { //north
                    _uv.x=uv.x;
                    _uv.y=uv.y;
                } else if (_turns < 0.5) { //east
                    _uv.x=1.0-uv.y;
                    _uv.y=1.0-uv.x;
                } else if (_turns < 0.75) { //south
                    _uv.x=uv.x;
                    _uv.y=1.0-uv.y;
                } else { //west
                    _uv.x=uv.y;
                    _uv.y=uv.x;
                }
                color = texture2D(blit,_uv).rgb;
                color = pow(color,vec3(2.2));

                vec2 gridCoord = (uv);
                vec2 gridSize = vec2(1.0)/blitsize;
                vec2 grid = mod(gridCoord,gridSize);
                grid = min(grid,gridSize-grid);
                float d = min(grid.x,grid.y);
                float lineSize = max(gridSize.x,gridSize.y)/10.0;
                //float lineSize = grid.x>grid.y?gridSize.x/10.0:gridSize.y/10.0;
                color = mix(max(vec3(0.1),color*0.1),color, step(lineSize,(d)));

                color = pow(color, vec3(1.0/2.2));
                gl_FragColor = vec4(color,1.0);
            }
            )});
            glBindTexture(GL_TEXTURE_2D,texture);
            blit_uniform("blit",program,0);
            blitsize_uniform("blitsize",program,texture.width,texture.height);
            turns_uniform("turns",program,turns);
            draw_box(x,y,w,h,program);
        }
    };
    struct spriteprogram {
        engine::program program;
        engine::draw_box draw_box;
        engine::uniform1i sprite_uniform;
        engine::uniform2f spritesize_uniform;
        engine::uniformMatrix4fv view_uniform,projection_uniform;
        inline void operator()(int x, int y, int w, int h, engine::texture &texture, float *view=0, float *projection=0) {
        static HMM_Mat4 identity = HMM_M4D(1.0);
            glBindTexture(GL_TEXTURE_2D,texture);
            program({_TEXT(
                uniform mat4 view;
                uniform mat4 projection;
                attribute vec2 position;
                varying vec2 uv;
                void main() {
                    uv = position*0.5+0.5;
                    gl_Position = projection*view*vec4(position, 0.0, 1.0);
                }
            )},{_TEXT(
                varying vec2 uv;
                uniform sampler2D sprite;
                uniform vec2 spritesize;
                void main() {
                    vec3 color;
                    color = texture2D(sprite,uv).rgb;
                    color = pow(color,vec3(2.2));

                    color = pow(color, vec3(1.0/2.2));
                    gl_FragColor = vec4(color,1.0);
                }
            )});
            sprite_uniform("sprite",program,0);
            spritesize_uniform("spritesize",program,texture.width,texture.height);
            view_uniform("view",program,view?view:&identity.Elements[0][0]);
            projection_uniform("projection",program,projection?projection:&identity.Elements[0][0]);
            draw_box(x,y,w,h,program);
        }
    };
    #define ROBOTO_MONO "https://raw.githubusercontent.com/google/fonts/refs/heads/main/ofl/robotomono/RobotoMono%5Bwght%5D.ttf"
    #define INTEL_MONO "https://raw.githubusercontent.com/intel/intel-one-mono/refs/heads/main/fonts/intel-one-mono-ttf/IntelOneMono-Regular.ttf"
    struct textprogram {
        int error;
        engine::fetch fetch;
        engine::texture texture;
        stbtt_bakedchar cdata['~'-' '];
        engine::buffer vertex;
        engine::program program;
        engine::attribute position_attribute;
        engine::attribute texCoord_attribute;
        int offset;
        engine::uniformMatrix4fv projection_uniform;
        engine::uniform1i font_uniform;
        int _size;
        inline void operator()(int x, int y, int w, int h, const char *font, int size) {
            HMM_Mat4 projection;
            if (error) goto error;
            if (_size != size) _size=size;
            fetch(font, [this](const char *data, int len) {
                int texsize=(_size/2)*(_size/2);
                unsigned char *_data = (unsigned char *)malloc(texsize*texsize*1);
                stbtt_BakeFontBitmap((const unsigned char *)data,0,_size,_data,texsize,texsize,' ','~'-' ',cdata);
                this->texture(texsize,texsize,GL_ALPHA);
                this->texture.upload(_data);
                free(_data);
            });
            vertex(GL_ARRAY_BUFFER,16384,GL_STREAM_DRAW);
            if (!texture) goto done;
            if (!offset) return;
            program({_TEXT(
                uniform mat4 projection;
                attribute vec2 position;
                attribute vec2 texCoord;
                varying vec2 uv;
                void main() {
                    uv = texCoord;
                    gl_Position = projection*vec4(position,0.0,1.0);
                }
            )},{_TEXT(
                uniform sampler2D font;
                varying vec2 uv;
                void main() {
                    gl_FragColor = texture2D(font,uv).aaaa;
                }
            )});
            position_attribute("position",program);
            texCoord_attribute("texCoord",program);
            projection = HMM_Orthographic_RH_NO(x,x+w,y+h,y,-1,+1);
            projection_uniform("projection",program,&projection.Elements[0][0]);
            glBindTexture(GL_TEXTURE_2D,texture);
            font_uniform("font",program,0);
            position_attribute.enable(2,GL_FLOAT,GL_FALSE,sizeof(float)*4,0);
            texCoord_attribute.enable(2,GL_FLOAT,GL_FALSE,sizeof(float)*4,(void*)(sizeof(float)*2));
            glViewport(x,y,w,h);
            for (int i = 0; i < offset; i += sizeof(float)*16) {
                glDrawArrays(GL_TRIANGLE_STRIP,i/(sizeof(float)*4),4);
            }
            //glDrawArrays(GL_TRIANGLE_STRIP,0,offset/(sizeof(float)*4));
            done:
            offset = 0;
            return;
            error:return;
        }
        inline void print(int x, int y, int w, int h, const char *text) {
            float _x,_y;
            _x = x+10;
            _y = y+_size;
            while (*text) {
                if (*text >= ' ' && *text < '~') {
                    GLfloat quaddata[16];
                    stbtt_aligned_quad q;
                    stbtt_GetBakedQuad(cdata, texture.width,texture.height, *text-' ', &_x,&_y,&q,1);//1=opengl & d3d10+,0=d3d9
                    quaddata[0] = q.x0;
                    quaddata[1] = q.y1;
                    quaddata[2] = q.s0;
                    quaddata[3] = q.t1;
                    quaddata[4] = q.x1;
                    quaddata[5] = q.y1;
                    quaddata[6] = q.s1;
                    quaddata[7] = q.t1;
                    quaddata[8] = q.x0;
                    quaddata[9] = q.y0;
                    quaddata[10] = q.s0;
                    quaddata[11] = q.t0;
                    quaddata[12] = q.x1;
                    quaddata[13] = q.y0;
                    quaddata[14] = q.s1;
                    quaddata[15] = q.t0;
                    vertex.upload(quaddata,offset,sizeof(quaddata));
                    offset += sizeof(quaddata);
                }
                ++text;
            }
        }
    };
    struct fpscounter {
        double fps;
        double deltas[512];
        unsigned index = 0;
        double time;
        char text[64];
        textprogram program;
        inline void operator()() {
            double total;
            unsigned count;
            double _time = glfwGetTime();
            if (!time) {time=_time;return;}
            deltas[index++&511] = _time-time;
            time = _time;
            count = std::min(index,512u);
            total = 0;
            for (int i = 0; i < count; ++i) {
                total += deltas[i];
            }
            total /= count;
            fps = 1.0/total;
        }
        inline void draw(int x, int y, int w, int h) {
            snprintf(text,sizeof(text),"%.0f fps",fps);
            program(x,y,w,h,INTEL_MONO,32);
            program.print(x,y,w,h,text);
        }
    };
    struct terrariumprogram {
        static const int WIDTH=4096/256;
        static const int HEIGHT=WIDTH;
        engine::program program;
        engine::draw_box draw_box;
        engine::uniform1i terrarium_uniform;
        engine::uniformMatrix4fv inv_view_uniform,inv_projection_uniform;
        fetchs<WIDTH*HEIGHT> fetch;
        texture terrarium;
        struct decoded {
            int width,height,depth;
            void *data;
        } decoded[WIDTH*HEIGHT];
        inline void operator()(int x, int y, int w, int h, float *view=0, float *projection=0) {
        static HMM_Mat4 identity = HMM_M4D(1.0);
            HMM_Mat4 inv_view,inv_projection;
            for (int i = 0; i < HEIGHT; ++i) {
                for (int j = 0; j < WIDTH; ++j) {
                    fetch(i*HEIGHT+j, init.terrarium_src[i*WIDTH+j], [i,j,this](const char *data, int len) {
                        struct decode d;
#ifndef __EMSCRIPTEN__
                        d.data = (stbi_uc *)data;
#else
                        d.data = (stbi_uc *)malloc(len);
                        memcpy(d.data,data,len);
#endif
                        d.len = len;
                        d.terrarium = this;
                        d.i = i;
                        d.j = j;
                        init.decoder.q.get_subscriber().on_next(d);
                    });
                    if (decoded[i*WIDTH+j].data) {
                        static GLenum formats[] = {
                            0,
                            GL_LUMINANCE,
                            GL_LUMINANCE_ALPHA,
                            GL_RGB,
                            GL_RGBA
                        };
                        terrarium(4096,4096,formats[decoded[i*WIDTH+j].depth]);
                        terrarium.upload(decoded[i*WIDTH+j].data,j*256,i*256,256,256);
                        stbi_image_free(decoded[i*WIDTH+j].data); decoded[i*WIDTH+j].data = 0;
                    }
                }
            }
            program({_TEXT(
                attribute vec2 position;
                varying vec2 uv;
                void main() {
                    uv = position*0.5+0.5;
                    //uv.y=1.0-uv.y;
                    //uv.x=1.0-uv.x;
                    gl_Position = vec4(position, 0.0, 1.0);
                }
            )},{_TEXT(
                float terrariumElevation(vec3 color) {
                    vec3 encoded = color * 256.0;
                    float elevation = ((encoded.r * 256.0 + encoded.g + (encoded.b / 256.0)) - 32768.0);
                    return elevation;
                }
                vec3 terrariumColor(float elevation) {
                    float v = elevation + 32768.0;
                    vec3 encoded = vec3(
                        floor(v / 256.0),
                        floor(mod(v,256.0)),
                        floor((v - floor(v)) * 256.0)
                    );
                    vec3 color = encoded / 256.0;
                    return color;
                }
                float terrariumNormal(float elevation) {
                    float s = elevation < 0.0 ? elevation / 11000.0 : elevation / 8900.0;
                    return s*0.5+0.5;
                    //return (elevation + 11000.0) / (8900.0 + 11000.0);
                }),"\n#extension GL_EXT_frag_depth : require\n",_TEXT(
                const float PI = 3.14159265359;
                
                varying vec2 uv;
                uniform sampler2D terrarium;
                uniform mat4 inv_projection;
                uniform mat4 inv_view;
                void main() {
                    // 1. Generate World Space Direction
                    vec2 ndc = uv * 2.0 - 1.0;
                    vec4 clip = vec4(ndc, -1.0, 1.0); // -1.0 is the near plane
                    vec4 view = inv_projection * clip;
                    view.w = 0.0; // Direction, not position
                    vec3 dir = normalize((inv_view * view).xyz);

                    // 2. Convert ECEF Direction to Lat/Lng (Geocentric)
                    // Z is up (North Pole), X/Y is equatorial plane
                    float lng = atan(dir.y, dir.x);
                    float lat = asin(clamp(dir.z, -1.0, 1.0));

                    // 3. Convert to Web Mercator [0, 1]
                    // Standard Mercator projection maps:
                    // Lon [-PI, PI]  -> [0, 1]
                    // Lat [-PI/2, PI/2] -> Web Mercator Y [0, 1]
                    float u = (lng + PI) / (2.0 * PI);
                    float v = 0.5 - log(tan(PI/4.0 + lat/2.0)) / (2.0 * PI);

                    vec2 wmuv = vec2(u, v);
                    //wmuv.y = 1.0-wmuv.y;
                    wmuv.x = 1.0-wmuv.x;
                    wmuv.y = wmuv.y*2.0-1.0;
                    wmuv.y = wmuv.y*(90.0/85.051129);
                    wmuv.y = wmuv.y*0.5+0.5;

                    vec3 color;
                    color = vec3(1.0);
                    color *= terrariumNormal(terrariumElevation(texture2D(terrarium,wmuv).rgb));
                    
                    vec3 eye = inv_view[3].xyz;
                    gl_FragDepthEXT = length(eye);

                    vec2 gridCoord = vec2(lat,lng)*(180.0/PI);
                    vec2 gridSize = vec2(7.5);
                    vec2 grid = mod(gridCoord,gridSize);
                    grid = min(grid,gridSize-grid);
                    float d = min(grid.x,grid.y);
                    float lineSize = max(gridSize.x,gridSize.y)/60.0;
                    color = mix(max(vec3(0.1),color*0.5),color, step(lineSize,(d)));
                    
                    color = pow(color, vec3(1.0/2.2));
                    gl_FragColor = vec4(color,1.0);
                }
            )});
            glBindTexture(GL_TEXTURE_2D,terrarium);
            terrarium_uniform("terrarium",program,0);
            inv_view = view?HMM_InvGeneralM4(*(HMM_Mat4*)view):identity;
            inv_projection = projection?HMM_InvGeneralM4(*(HMM_Mat4*)projection):identity;
            inv_view_uniform("inv_view",program,&inv_view.Elements[0][0]);
            inv_projection_uniform("inv_projection",program,&inv_projection.Elements[0][0]);
            draw_box(x,y,w,h,program);
        }

        struct decode {
            stbi_uc *data;
            int len;
            struct terrariumprogram *terrarium;
            int i,j;
        };
        struct decoder {
            decoder() {
                q.get_observable()
                .observe_on(rxcpp::observe_on_new_thread())
                .subscribe(subscription, [](struct decode decode) {
                    struct decoded *decoded = &decode.terrarium->decoded[decode.i*decode.terrarium->WIDTH+decode.j];
                    decoded->data = stbi_load_from_memory(decode.data,decode.len,&decoded->width,&decoded->height,&decoded->depth,0);
                    if (!decoded->data) fprintf(stderr,"decoder err %s\n",stbi_failure_reason());
                    free(decode.data);
                });
            }
            ~decoder() {
                subscription.unsubscribe();
            }
            rxcpp::subjects::subject<struct decode> q;
            private:
            rxcpp::composite_subscription subscription;
        };
        static struct Init {
            char terrarium_src[WIDTH*HEIGHT][128];
            struct decoder decoder;
            Init() {
                for (int i = 0; i < HEIGHT; ++i) {
                    for (int j = 0; j < WIDTH; ++j) {
                        snprintf(terrarium_src[i*WIDTH+j], sizeof(terrarium_src[i*WIDTH+j]), "https://elevation-tiles-prod.s3.us-east-1.amazonaws.com/%s/4/%d/%d.png", "terrarium", j, i);
                    }
                }
            }
        } init;
    };
    struct marbleprogram {
        HMM_Vec4 gridColor = {1.0/64,1.0/64,1.0/64,1.0};
        engine::program program;
        engine::draw_box draw_box;
        engine::uniform1i marble_uniform;
        engine::uniformMatrix4fv inv_view_uniform,inv_projection_uniform;
        engine::textureimage marble;
        engine::uniform4fv gridColor_uniform;
        inline void operator()(int x, int y, int w, int h, float *view=0, float *projection=0, const char *url="https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/bmng-base/january/world.200401.3x5400x2700.jpg") {
        static HMM_Mat4 identity = HMM_M4D(1.0);
        HMM_Mat4 inv_view,inv_projection;
            program({_TEXT(
                attribute vec2 position;
                varying vec2 uv;
                void main() {
                    uv = position*0.5+0.5;
                    //uv.y=1.0-uv.y;
                    //uv.x=1.0-uv.x;
                    gl_Position = vec4(position, 0.0, 1.0);
                }
            )},{_TEXT(
                const float PI = 3.14159265359;
                
                varying vec2 uv;
                uniform sampler2D marble;
                uniform mat4 inv_projection;
                uniform mat4 inv_view;
                uniform vec4 gridColor;
                void main() {
                    // 1. Generate World Space Direction
                    vec2 ndc = uv * 2.0 - 1.0;
                    vec4 clip = vec4(ndc, -1.0, 1.0); // -1.0 is the near plane
                    vec4 view = inv_projection * clip;
                    view.w = 0.0; // Direction, not position
                    vec3 dir = normalize((inv_view * view).xyz);

                    // 2. Convert ECEF Direction to Lat/Lng (Geocentric)
                    // Z is up (North Pole), X/Y is equatorial plane
                    float lng = atan(dir.y, dir.x);
                    float lat = asin(clamp(dir.z, -1.0, 1.0));

                    // 3. Convert to Equirectangular [0, 1]
                    float u = (lng + PI) / (2.0 * PI);
                    float v = (lat + PI/2.0) / PI;

                    vec2 eruv = vec2(u,v);
                    eruv.y = 1.0 - eruv.y;
                    eruv.x = 1.0 - eruv.x;

                    vec3 color = texture2D(marble,eruv).rgb;
                    //color = pow(color,vec3(2.2)); // srgb uncompressed, linear compressed
                    
                    vec2 gridCoord = vec2(lat,lng)*(180.0/PI);
                    vec2 gridSize = vec2(7.5);
                    vec2 grid = mod(gridCoord,gridSize);
                    grid = min(grid,gridSize-grid);
                    float d = min(grid.x,grid.y);
                    //float lineSize = max(gridSize.x,gridSize.y)/60.0;
                    float lineSize = grid.x>grid.y?gridSize.x/60.0:gridSize.y/120.0;
                    color = mix(gridColor.rgb,color, clamp((1.0-gridColor.a)+step(lineSize,(d)),0.0,1.0));
                    
                    color = pow(color, vec3(1.0/2.2));
                    gl_FragColor = vec4(color,1.0);
                }
            )});
            marble(url);
            glBindTexture(GL_TEXTURE_2D,marble);
            marble_uniform("marble",program,0);
            inv_view = view?HMM_InvGeneralM4(*(HMM_Mat4*)view):identity;
            inv_projection = projection?HMM_InvGeneralM4(*(HMM_Mat4*)projection):identity;
            inv_view_uniform("inv_view",program,&inv_view.Elements[0][0]);
            inv_projection_uniform("inv_projection",program,&inv_projection.Elements[0][0]);
            gridColor_uniform("gridColor",program,&gridColor.Elements[0]);
            draw_box(x,y,w,h,program);
        }
    };
    struct gltfbuffer {
        std::string url;
        fetch _fetch;
        uint8_t *data;
        size_t size;
        inline void operator()() {
            _fetch(url.c_str(),[this](const char *_data, int len) {
#ifndef __EMSCRIPTEN__
                data = (uint8_t *)_data;
#else
                data = (uint8_t*)realloc(data,len);
                memcpy(data,_data,len);
#endif
            },size);
        }
        static struct Init {
            UT_icd array = {sizeof(struct gltfbuffer)};
        } init;
    };
    struct gltfbufferview : buffer {
        GLenum target,usage;
        struct gltfbuffer *gltf;
        GLsizeiptr length,offset;
        GLsizeiptr _uploaded;
        inline void operator()() {
            if (!length) return;
            buffer::operator()(target,length,usage);
            if (_uploaded) return;
            if (!gltf->data) return;
            upload(gltf->data+(offset),0,length);
            _uploaded=length;
        }
        static struct Init {
            UT_icd array = {sizeof(struct gltfbufferview)};
        } init;
    };
    struct gltftexture : textureimage {
        std::string src;
        GLenum min_filter, mag_filter;
        GLenum wrap_s, wrap_t;
        inline void operator()() {
            textureimage::operator()(src.c_str(),min_filter,mag_filter,wrap_s,wrap_t);
        }
        static struct Init {
            UT_icd array = {sizeof(struct gltftexture)};
        } init;
    };
    struct gltfmaterial {
        HMM_Vec3 emissiveFactor;
        struct gltftexture *emissiveTexture;
        struct gltftexture *occlusionTexture;
        struct gltftexture *normalTexture;
        struct {            
            HMM_Vec4 baseColorFactor;
            struct gltftexture *baseColorTexture;
            float metallicFactor;
            float roughnessFactor;
            struct gltftexture *metallicRoughnessTexture;
        } pbrMetallicRoughness;
        static struct Init {
            UT_icd array = {sizeof(struct gltfmaterial)};
        } init;
    };
    struct gltfaccessor {
        struct gltfbufferview *bufferview;
        GLenum type;
        GLuint componentCount;
        GLuint count;
        GLsizeiptr offset;
        GLuint stride;
        GLboolean normalized;
        union {
            float boundf[16][2];
            int boundi[16][2];
            unsigned boundu[16][2];
            short bounds[16][2];
            unsigned short boundus[16][2];
            char boundb[16][2];
            unsigned char boundub[16][2];
        };
        inline size_t size() const {
            switch (type) {
                default: fprintf(stderr, "unknown vertex type %d\n", type);
                return 0;
                case GL_FLOAT:
                return sizeof(GLfloat);
                case GL_INT:
                return sizeof(GLint);
                case GL_UNSIGNED_INT:
                return sizeof(GLuint);
                case GL_SHORT:
                return sizeof(GLshort);
                case GL_UNSIGNED_SHORT:
                return sizeof(GLushort);
                case GL_BYTE:
                return sizeof(GLbyte);
                case GL_UNSIGNED_BYTE:
                return sizeof(GLubyte);
            }
        }
    };
    struct gltfattribute : gltfaccessor {
        char name[16];
        static struct Init {
            UT_icd array = {sizeof(struct gltfattribute)};
        } init;
    };
    struct gltfprimitivetarget {
        UT_array *attributes;
        int position_attribute;
        int normal_attribute;
        int tangent_attribute;
        int texCoord0_attribute;
        int joints0_attribute;
        int weights0_attribute;
        static struct Init {
            UT_icd array = {sizeof(struct gltfprimitivetarget)};
        } init;
    };
    struct gltfprimitive : gltfaccessor {
        struct gltfmaterial *material;
        GLenum mode;
        UT_array *attributes;
        int position_attribute;
        int normal_attribute;
        int tangent_attribute;
        int texCoord0_attribute;
        int joints0_attribute;
        int weights0_attribute;
        UT_array *targets;
        static struct Init {
            UT_icd array = {sizeof(struct gltfprimitive)};
        } init;
    };
    struct gltfmeshweight {
        float weight;
        static struct Init {
            UT_icd array = {sizeof(struct gltfmeshweight)};
        } init;
    };
    struct gltfmesh {
        UT_array *primitives;
        UT_array *weights;
        static struct Init {
            UT_icd array = {sizeof(struct gltfmesh)};
        } init;
    };
    struct gltfjointref {
        int index;
        static struct Init {
            UT_icd array = {sizeof(struct gltfjointref)};
        } init;
    };
    struct gltfskin {
        struct gltfaccessor inverseBindMatrices;
        UT_array *joints;
        static struct Init {
            UT_icd array = {sizeof(struct gltfskin)};
        } init;
    };
    struct gltfnoderef {
        int index;
        static struct Init {
            UT_icd array = {sizeof(struct gltfnoderef)};
        } init;
    };
    struct gltfnode {
        HMM_Mat4 transform;//global
        HMM_Vec3 translation;
        HMM_Quat rotation;
        HMM_Vec3 scale;
        HMM_Mat4 matrix;
        UT_array *children;
        struct gltfmaterial *material;
        struct gltfmesh *mesh;
        struct gltfskin *skin;
        static struct Init {
            UT_icd array = {sizeof(struct gltfnode)};
        } init;
    };
    struct gltfanimationsampler {
        struct gltfaccessor input;
        struct gltfaccessor output;
        //interpolation
        static struct Init {
            UT_icd array = {sizeof(struct gltfanimationsampler)};
        } init;
    };
    struct gltfanimationchannel {
        struct gltfanimationsampler *sampler;
        struct {
            int node;
            int path;
        } target;
        static struct Init {
            UT_icd array = {sizeof(struct gltfanimationchannel)};
        } init;
    };
    struct gltfanimation {
        char name[32];
        UT_hash_handle hh;
        UT_array *samplers;
        UT_array *channels;
    };
    struct gltfscene {
        UT_array *nodes;
        static struct Init {
            UT_icd array = {sizeof(struct gltfscene)};
        } init;
    };
    struct gltfasset {
        using json = nlohmann::json;
        int error;
        UT_array *buffers;
        UT_array *bufferviews;
        UT_array *textures;
        UT_array *materials;
        UT_array *meshes;
        UT_array *skins;
        UT_array *nodes;
        struct gltfanimation *animations;
        struct gltfanimation *_animations_array;
        UT_array *scenes;
        gltfscene *scene;
        engine::fetch fetch;
        std::string _folder;
        inline void operator()(const char *file, const char *folder) {
            std::string url;
            std::string _file;
            struct gltfbuffer *b;
            if (error) goto error;
            if (!buffers) {
                utarray_new(buffers, &gltfbuffer::init.array);
                utarray_new(bufferviews, &gltfbufferview::init.array);
                utarray_new(textures, &gltftexture::init.array);
                utarray_new(materials, &gltfmaterial::init.array);
                utarray_new(meshes, &gltfmesh::init.array);
                utarray_new(skins, &gltfskin::init.array);
                utarray_new(nodes, &gltfnode::init.array);
                animations = 0;
                utarray_new(scenes, &gltfscene::init.array);
            }
            _folder = folder ? std::string(folder) : std::string();
            if (_folder.length() && (_folder.back() != '/')) _folder+='/';
            _file = std::string(file);
            url = _folder + _file;
            fetch(url.c_str(),[this](const char *data, int len) {
                struct parse p;
                p.asset = this;
#ifndef __EMSCRIPTEN__
                p.data = (char *)data;
#else
                p.data = (char *)malloc(len);
                memcpy(p.data,data,len);
#endif
                p.len = len;
                init.parser.q.get_subscriber().on_next(p);
            });
            b = 0;
            while ((b = (gltfbuffer *)utarray_next(buffers,b))) {
                (*b)();
            }
            return;
            error:
            return;
        }
        private: void parse(const char *data, int len) {
            json j = json::parse(data,data+len);
            //fputs(data, stdout);
            auto &_buffers = j["buffers"];
            utarray_reserve(buffers,_buffers.size());
            for (auto &b : _buffers) {
                utarray_extend_back(buffers);
                gltfbuffer *buffer = (gltfbuffer *)utarray_back(buffers);
                buffer->size = b.contains("byteLength") ? b["byteLength"].get<size_t>() : 0;
                buffer->data = 0;// buffer->size ? (uint8_t *)calloc(1,buffer->size) : 0;
                auto uri = b["uri"].get_ref<const std::string&>();
                if (!strncmp(uri.c_str(),"data:",sizeof("data:"))) {
                    // todo
                    fprintf(stderr, "bufferView data uri not supported\n");
                    continue;
                } else {
                    buffer->url = _folder + uri;
                }
            }
            auto &_bufferViews = j["bufferViews"];
            utarray_reserve(bufferviews, _bufferViews.size());
            for (auto &bv : _bufferViews) {
                utarray_extend_back(bufferviews);
                struct gltfbufferview *_bv = (gltfbufferview *)utarray_back(bufferviews);
                if (bv.contains("target")) {
                    _bv->target = bv["target"];
                } else {
                    _bv->target = 0;
                }
                _bv->gltf = (struct gltfbuffer *)utarray_eltptr(buffers, bv["buffer"].get<int>());
                _bv->offset = bv.contains("byteOffset") ? bv["byteOffset"].get<int>() : 0;
                _bv->length = bv["byteLength"].get<int>();
                _bv->usage = GL_STATIC_DRAW;
            }
            auto &accessors = j["accessors"];
            if (j.contains("textures")) {
                auto &_textures = j["textures"];
                utarray_reserve(textures, _textures.size());
                auto &images = j["images"];
                for (auto &t : _textures) {
                    if (!t.contains("source")) {
                        fprintf(stderr, "no source for texture %s\n", t["name"].get<std::string>().c_str());
                        continue;
                    }
                    auto &img = images[t["source"].get<int>()];
                    if (img.contains("bufferView")) {
                        // todo
                        fprintf(stderr, "bufferView textures not supported\n");
                        continue;
                    } else if (img.contains("uri")) {
                        utarray_extend_back(textures);
                        struct gltftexture *_t = (struct gltftexture *)utarray_back(textures);
                        auto _src = img["uri"].get_ref<const std::string &>();
                        if (!strncmp(_src.c_str(),"data:",sizeof("data:"))) {
                            // todo
                            fprintf(stderr, "texture data uri not supported\n");
                            continue;
                        } else {
                            _t->src = (_folder+_src).c_str();
                        }
                        if (t.contains("sampler")) {
                            auto sampler = j["samplers"][t["sampler"].get<int>()];
                            _t->min_filter = sampler.contains("minFilter") ? sampler["minFilter"].get<int>() : 0;
                            _t->mag_filter = sampler.contains("magFilter") ? sampler["magFilter"].get<int>() : 0;
                            _t->wrap_s = sampler.contains("wrapS") ? sampler["wrapS"].get<int>() : 0;
                            _t->wrap_t = sampler.contains("wrapT") ? sampler["wrapT"].get<int>() : 0;
                        } else {
                            _t->min_filter = _t->mag_filter = GL_NEAREST;
                            _t->wrap_s = _t->wrap_t = GL_CLAMP_TO_EDGE;
                        }
                    } else {
                        fprintf(stderr, "json data-uri images are not supported\n");
                    }
                }
            }
            if (j.contains("materials")) {
                auto &_materials = j["materials"];
                utarray_reserve(materials, _materials.size());
                for (auto &m : _materials) {
                    utarray_extend_back(materials);
                    struct gltfmaterial *_m = (gltfmaterial *)utarray_back(materials);
                    if (m.contains("pbrMetallicRoughness")) {
                        auto &pbr = m["pbrMetallicRoughness"];
                        if (pbr.contains("baseColorFactor")) {
                            auto v = pbr["baseColorFactor"];
                            _m->pbrMetallicRoughness.baseColorFactor = HMM_V4(v[0],v[1],v[2],v[3]);
                        } else {
                            _m->pbrMetallicRoughness.baseColorFactor = HMM_V4(1,1,1,1);
                        }
                        if (pbr.contains("baseColorTexture")) {
                            _m->pbrMetallicRoughness.baseColorTexture = (gltftexture *)utarray_eltptr(textures, pbr["baseColorTexture"]["index"].get<int>());
                        }
                        if (pbr.contains("metallicFactor")) {
                            _m->pbrMetallicRoughness.metallicFactor = pbr["metallicFactor"];
                        }
                        if (pbr.contains("roughnessFactor")) {
                            _m->pbrMetallicRoughness.roughnessFactor = pbr["roughnessFactor"];
                        }
                        if (pbr.contains("metallicRoughnessTexture")) {
                            _m->pbrMetallicRoughness.metallicRoughnessTexture = (gltftexture *)utarray_eltptr(textures, pbr["metallicRoughnessTexture"]["index"].get<int>());
                        }
                    }
                    if (m.contains("normalTexture")) {
                        _m->normalTexture = (gltftexture *)utarray_eltptr(textures, m["normalTexture"]["index"].get<int>());
                    }
                    if (m.contains("occlusionTexture")) {
                        _m->occlusionTexture = (gltftexture *)utarray_eltptr(textures, m["occlusionTexture"]["index"].get<int>());
                    }
                    if (m.contains("emissiveTexture")) {
                        _m->emissiveTexture = (gltftexture *)utarray_eltptr(textures, m["emissiveTexture"]["index"].get<int>());
                    }
                    if (m.contains("emissiveFactor")) {
                        auto v = m["emissiveFactor"];
                        _m->emissiveFactor = HMM_V3(v[0],v[1],v[2]);
                    }
                }
            }
            auto &_meshes = j["meshes"];
            utarray_reserve(meshes, _meshes.size());
            for (auto &m : _meshes) {
                utarray_extend_back(meshes);
                gltfmesh *_m = (gltfmesh *)utarray_back(meshes);
                auto &_primitives = m["primitives"];
                utarray_new(_m->primitives, &gltfprimitive::init.array);
                utarray_reserve(_m->primitives, _primitives.size());
                for (auto &p : _primitives) {
                    utarray_extend_back(_m->primitives);
                    gltfprimitive *_p = (gltfprimitive *)utarray_back(_m->primitives);
                    if (p.contains("material")) {
                        _p->material = (gltfmaterial *)utarray_eltptr(materials,p["material"].get<int>());
                    }
                    _p->position_attribute = -1;
                    _p->normal_attribute = -1;
                    _p->tangent_attribute = -1;
                    _p->texCoord0_attribute = -1;
                    _p->joints0_attribute = -1;
                    _p->weights0_attribute = -1;
                    auto &_attributes = p["attributes"];
                    utarray_new(_p->attributes, &gltfattribute::init.array);
                    utarray_reserve(_p->attributes,_attributes.size());
                    for (auto &a : _attributes.items()) {
                        utarray_extend_back(_p->attributes);
                        gltfattribute *_a = (gltfattribute *)utarray_back(_p->attributes);
                        auto &ac = accessors[a.value().get<int>()];
                        gltfbufferview *_bv = (gltfbufferview *)utarray_eltptr(bufferviews,ac["bufferView"].get<int>());
                        _bv->target = GL_ARRAY_BUFFER;
                        _a->bufferview = _bv;
                        parse_accessor(_a,ac);
                        strncpy(_a->name, a.key().c_str(), sizeof(_a->name));
                        if (!strncmp(_a->name,"POSITION",sizeof(_a->name))) _p->position_attribute = utarray_eltidx(_p->attributes,_a);
                        else if (!strncmp(_a->name,"NORMAL",sizeof(_a->name))) _p->normal_attribute = utarray_eltidx(_p->attributes,_a);
                        else if (!strncmp(_a->name,"TANGENT",sizeof(_a->name))) _p->tangent_attribute = utarray_eltidx(_p->attributes,_a);
                        else if (!strncmp(_a->name,"TEXCOORD_0",sizeof(_a->name))) _p->texCoord0_attribute = utarray_eltidx(_p->attributes,_a);
                        else if (!strncmp(_a->name,"JOINTS_0",sizeof(_a->name))) _p->joints0_attribute = utarray_eltidx(_p->attributes,_a);
                        else if (!strncmp(_a->name,"WEIGHTS_0",sizeof(_a->name))) _p->weights0_attribute = utarray_eltidx(_p->attributes,_a);
                        else fprintf(stderr, "gltfprimitive unsupported attribute %s\n", a.key().c_str());
                    }
                    auto &_targets = p["targets"];
                    utarray_new(_p->targets, &gltfprimitivetarget::init.array);
                    utarray_reserve(_p->targets,_targets.size());
                    for (auto &t : _targets) {
                        utarray_extend_back(_p->targets);
                        gltfprimitivetarget *_t = (gltfprimitivetarget*)utarray_back(_p->targets);
                        utarray_new(_t->attributes,&gltfattribute::init.array);
                        utarray_reserve(_t->attributes,t.size());
                        for (auto &a : t.items()) {
                            utarray_extend_back(_t->attributes);
                            gltfattribute *_a = (gltfattribute*)utarray_back(_t->attributes);
                            auto &ac = accessors[a.value().get<int>()];
                        gltfbufferview *_bv = (gltfbufferview *)utarray_eltptr(bufferviews,ac["bufferView"].get<int>());
                        _bv->target = GL_ARRAY_BUFFER;
                        _a->bufferview = _bv;
                        parse_accessor(_a,ac);
                        strncpy(_a->name, a.key().c_str(), sizeof(_a->name));
                        if (!strncmp(_a->name,"POSITION",sizeof(_a->name))) _t->position_attribute = utarray_eltidx(_p->attributes,_a);
                        else if (!strncmp(_a->name,"NORMAL",sizeof(_a->name))) _t->normal_attribute = utarray_eltidx(_p->attributes,_a);
                        else if (!strncmp(_a->name,"TANGENT",sizeof(_a->name))) _t->tangent_attribute = utarray_eltidx(_p->attributes,_a);
                        else if (!strncmp(_a->name,"TEXCOORD_0",sizeof(_a->name))) _t->texCoord0_attribute = utarray_eltidx(_p->attributes,_a);
                        else if (!strncmp(_a->name,"JOINTS_0",sizeof(_a->name))) _t->joints0_attribute = utarray_eltidx(_p->attributes,_a);
                        else if (!strncmp(_a->name,"WEIGHTS_0",sizeof(_a->name))) _t->weights0_attribute = utarray_eltidx(_p->attributes,_a);
                        else fprintf(stderr, "gltfprimitivetarget unsupported attribute %s\n", a.key().c_str());
                        }
                    }
                    if (p.contains("mode")) {
                        _p->mode = p["mode"];
                    } else {
                        _p->mode = GL_TRIANGLES;
                    }
                    if (p.contains("indices")) {
                        auto &ac = accessors[p["indices"].get<int>()];
                        gltfbufferview *_bv = (gltfbufferview *)utarray_eltptr(bufferviews,ac["bufferView"].get<int>());
                        _bv->target = GL_ELEMENT_ARRAY_BUFFER;
                        _p->bufferview = _bv;
                        parse_accessor(_p,ac);
                        assert(_p->componentCount == 1);
                        assert(_bv->length / init.COMPONENT_SIZE[_p->type] == _p->count && "size check 123");
                        if (_p->stride) fprintf(stderr,"gltfprimitive indices stride not supported\n");
                        if (_p->normalized) fprintf(stderr, "gltfprimitive indices normalized not supported");
                    } else {
                        gltfattribute *a = (gltfattribute *)utarray_front(_p->attributes);
                        _p->count = a->count;
                    }
                }
                auto &_weights = m["weights"];
                utarray_new(_m->weights, &gltfmeshweight::init.array);
                utarray_reserve(_m->weights, _weights.size());
                for (auto &w : _weights) {
                    gltfmeshweight _w = {w};
                    utarray_push_back(_m->weights,&_w);
                }
            }
            auto &_skins = j["skins"];
            utarray_reserve(skins, _skins.size());
            for (auto &s : _skins) {
                utarray_extend_back(skins);
                struct gltfskin *_s = (struct gltfskin *)utarray_back(skins);
                auto &_joints = s["joints"];
                utarray_new(_s->joints, &gltfjointref::init.array);
                utarray_reserve(_s->joints,_joints.size());
                for (auto &j : _joints) {
                    gltfjointref _j = {j};
                    utarray_push_back(_s->joints,&_j);
                }
                if (s.contains("inverseBindMatrices")) {
                    auto &ac = accessors[s["inverseBindMatrices"].get<int>()];
                    gltfbufferview *_bv = (gltfbufferview *)utarray_eltptr(bufferviews,ac["bufferView"].get<int>());
                    _s->inverseBindMatrices.bufferview = _bv;
                    parse_accessor(&_s->inverseBindMatrices,ac);
                    assert(_s->inverseBindMatrices.componentCount == 16);
                    if (_s->inverseBindMatrices.stride) fprintf(stderr,"gltfskin inverseBindMatrices stride not supported\n");
                    if (_s->inverseBindMatrices.normalized) fprintf(stderr, "gltfskin inverseBindMatrices normalized not supported\n");
                }
            }
            auto &_nodes = j["nodes"];
            utarray_reserve(nodes, _nodes.size());
            for (auto &n : _nodes) {
                utarray_extend_back(nodes);
                struct gltfnode *_n = (struct gltfnode *)utarray_back(nodes);
                if (n.contains("matrix")) {
                    auto v = n["matrix"];
                    float *_v = &_n->matrix.Elements[0][0];
                    for (int i = 0; i < 16; ++i) _v[i] = v[i].get<float>();
                    //for (int i = 0; i < 4; ++i) {
                    //    for (int j = 0; j < 4; ++j) {
                    //        _n->matrix[i][j] = v[i * 4 + j].get<float>();
                    //    }
                    //}
                } else {
                    _n->matrix = HMM_M4D(1.0);
                }
                if (n.contains("translation")) {
                    auto v = n["translation"];
                    _n->translation = HMM_V3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
                } else {
                    _n->translation = HMM_V3(0.0,0.0,0.0);
                }
                if (n.contains("rotation")) {
                    auto v = n["rotation"];
                    _n->rotation = HMM_Q(v[0].get<float>(), v[1].get<float>(), v[2].get<float>(), v[3].get<float>());
                } else {
                    _n->rotation = HMM_Q(0.0, 0.0, 0.0, 1.0);
                }
                if (n.contains("scale")) {
                    auto v = n["scale"];
                    _n->scale = HMM_V3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
                } else {
                    _n->scale = HMM_V3(1.0,1.0,1.0);
                }
                _n->matrix = HMM_MulM4(_n->matrix,
                    HMM_MulM4(HMM_Translate(_n->translation),
                        HMM_MulM4(HMM_QToM4(_n->rotation),HMM_Scale(_n->scale))
                    ));
                HMM_Vec3 skew;
                HMM_Vec4 perspective;
                HMM_Decompose(_n->matrix, &_n->scale, &_n->rotation, &_n->translation, &skew, &perspective);
                if (n.contains("material")) {
                    _n->material = (gltfmaterial *)utarray_eltptr(materials,n["material"].get<int>());
                }
                if (n.contains("mesh")) {
                    _n->mesh = (gltfmesh *)utarray_eltptr(meshes,n["mesh"].get<int>());
                }
                if (n.contains("skin")) {
                    _n->skin = (gltfskin *)utarray_eltptr(skins,n["skin"].get<int>());
                }
                if (n.contains("children")) {
                    auto _children = n["children"];
                    utarray_new(_n->children,&gltfnoderef::init.array);
                    utarray_reserve(_n->children, _children.size());
                    for (auto &c : _children) {
                        gltfnoderef _c = { c };
                        utarray_push_back(_n->children, &_c);
                    }
                }
                //fprintf(stdout, "\t%s (%.2f %.2f)\n", n.contains("name")?n["name"].get<std::string>().c_str():"unnamed",_n->scale.X, _n->scale.Y);
            }
            auto _animations = j["animations"];
            _animations_array = (gltfanimation *)calloc(_animations.size(),sizeof(gltfanimation));
            int ai = 0;
            for (auto &a : _animations) {
                gltfanimation *_a = &_animations_array[ai++];
                if (a.contains("name")) strncpy(_a->name,a["name"].get_ref<const std::string&>().c_str(),sizeof(_a->name));
                else snprintf(_a->name,sizeof(_a->name),"%d",ai);
                HASH_ADD_STR(animations,name,_a);
                auto _samplers = a["samplers"];
                utarray_new(_a->samplers,&gltfanimationsampler::init.array);
                utarray_reserve(_a->samplers,_samplers.size());
                for (auto &s : _samplers) {
                    utarray_extend_back(_a->samplers);
                    gltfanimationsampler *_s = (gltfanimationsampler*)utarray_back(_a->samplers);
                    {
                    auto &ac = accessors[s["input"].get<int>()];
                    gltfbufferview *_bv = (gltfbufferview *)utarray_eltptr(bufferviews,ac["bufferView"].get<int>());
                    _s->input.bufferview = _bv;
                    parse_accessor(&_s->input,ac);
                    assert(_s->input.componentCount == 1);
                    if (_s->input.stride) fprintf(stderr,"gltfanimation sampler stride not supported\n");
                    if (_s->input.normalized) fprintf(stderr, "gltfanimation sampler normalized not supported\n");
                    }
                    {
                    auto &ac = accessors[s["output"].get<int>()];
                    gltfbufferview *_bv = (gltfbufferview *)utarray_eltptr(bufferviews,ac["bufferView"].get<int>());
                    _s->output.bufferview = _bv;
                    parse_accessor(&_s->output,ac);
                    if (_s->output.stride) fprintf(stderr,"gltfanimation sampler stride not supported\n");
                    if (_s->output.normalized) fprintf(stderr, "gltfanimation sampler normalized not supported\n");
                    }
                }
                auto _channels = a["channels"];
                utarray_new(_a->channels,&gltfanimationchannel::init.array);
                utarray_reserve(_a->channels,_channels.size());
                for (auto &c : _channels) {
                    utarray_extend_back(_a->channels);
                    gltfanimationchannel *_c = (gltfanimationchannel*)utarray_back(_a->channels);
                    _c->sampler = (gltfanimationsampler*)utarray_eltptr(_a->samplers,c["sampler"].get<int>());
                    auto _target = c["target"];
                    _c->target.node = _target["node"].get<int>();
                    auto _path = _target["path"].get_ref<const std::string&>().c_str();
                    switch (*_path) {
                        default: fprintf(stderr, "animation channel unsupported path %s\n", _path); continue;
                        case 'r':
                        _c->target.path = offsetof(gltfnode,rotation);
                        assert(_c->sampler->output.componentCount==4);
                        break;
                        case 't':
                        _c->target.path = offsetof(gltfnode,translation);
                        assert(_c->sampler->output.componentCount==3);
                        break;
                        case 's':
                        _c->target.path = offsetof(gltfnode,scale);
                        assert(_c->sampler->output.componentCount==3);
                        break;
                    }
                }
            }
            auto _scenes = j["scenes"];
            utarray_reserve(scenes,_scenes.size());
            for (auto &s : _scenes) {
                utarray_extend_back(scenes);
                gltfscene *_s = (gltfscene *)utarray_back(scenes);
                if (s.contains("nodes")) {
                    utarray_new(_s->nodes,&gltfnoderef::init.array);
                    auto &_n = s["nodes"];
                    utarray_reserve(_s->nodes,_n.size());
                    for (auto &n : _n) {
                        gltfnoderef __n ={n};
                        utarray_push_back(_s->nodes,&__n);
                    }
                }
            }
            if (j.contains("scene")) {
                scene = (gltfscene *)utarray_eltptr(scenes,j["scene"].get<int>());
            } else {
                scene = (gltfscene *)utarray_next(scenes,scene);
            }
        }
        static void parse_accessor(gltfaccessor *_a, const json &ac) {
            _a->type = ac["componentType"].get<GLenum>();
            _a->componentCount = init.COMPONENT_COUNT[ac["type"]];
            _a->count = ac["count"].get<GLuint>();
            _a->offset = ac.contains("byteOffset") ? ac["byteOffset"].get<GLuint>() : 0;
            _a->stride = ac.contains("byteStride") ? ac["byteStride"].get<GLuint>() : 0;
            _a->normalized = ac.contains("normalized") ? ac["normalized"].get<GLboolean>() : 0;
            for (int i = 0; i < _a->componentCount; ++i) {
                switch (_a->type) {
                    case GL_FLOAT:
                    _a->boundf[i][0] = FLT_MAX;
                    _a->boundf[i][1] = FLT_MIN;
                    break;
                    case GL_INT:
                    _a->boundi[i][0] = INT_MAX;
                    _a->boundi[i][1] = INT_MIN;
                    break;
                    case GL_UNSIGNED_INT:
                    _a->boundu[i][0] = UINT_MAX;
                    _a->boundu[i][1] = 0;
                    break;
                    case GL_SHORT:
                    _a->bounds[i][0] = SHRT_MAX;
                    _a->bounds[i][1] = SHRT_MIN;
                    break;
                    case GL_UNSIGNED_SHORT:
                    _a->boundus[i][0] = USHRT_MAX;
                    _a->boundus[i][1] = 0;
                    break;
                    case GL_BYTE:
                    _a->boundb[i][0] = CHAR_MAX;
                    _a->boundb[i][1] = CHAR_MIN;
                    break;
                    case GL_UNSIGNED_BYTE:
                    _a->boundub[i][0] = UCHAR_MAX;
                    _a->boundub[i][1] = 0;
                    break;
                }
            }
            if (ac.contains("max")) {
                auto &maxes = ac["max"];
                int i = 0;
                for (auto &m : maxes) {
                    if (++i == 16) break;
                    switch (_a->type) {
                        case GL_FLOAT:
                        _a->boundf[i][1] = std::max(_a->boundf[i][1], m.get<GLfloat>());
                        break;
                        case GL_INT:
                        _a->boundi[i][1] = std::max(_a->boundi[i][1], m.get<GLint>());
                        break;
                        case GL_UNSIGNED_INT:
                        _a->boundu[i][1] = std::max(_a->boundu[i][1], m.get<GLuint>());
                        break;
                        case GL_SHORT:
                        _a->bounds[i][1] = std::max(_a->bounds[i][1], m.get<GLshort>());
                        break;
                        case GL_UNSIGNED_SHORT:
                        _a->boundus[i][1] = std::max(_a->boundus[i][1], m.get<GLushort>());
                        break;
                        case GL_BYTE:
                        _a->boundb[i][1] = std::max<GLbyte>(_a->boundb[i][1], m.get<GLbyte>());
                        break;
                        case GL_UNSIGNED_BYTE:
                        _a->boundub[i][1] = std::max(_a->boundub[i][1], m.get<GLubyte>());
                        break;
                    }
                }
            }
            if (ac.contains("min")) {
                auto &mins = ac["min"];
                int i = 0;
                for (auto &m : mins) {
                    if (++i == 16) break;
                    switch (_a->type) {
                        case GL_FLOAT:
                        _a->boundf[i][0] = std::min(_a->boundf[i][1], m.get<GLfloat>());
                        break;
                        case GL_INT:
                        _a->boundi[i][0] = std::min(_a->boundi[i][1], m.get<GLint>());
                        break;
                        case GL_UNSIGNED_INT:
                        _a->boundu[i][0] = std::min(_a->boundu[i][1], m.get<GLuint>());
                        break;
                        case GL_SHORT:
                        _a->bounds[i][0] = std::min(_a->bounds[i][1], m.get<GLshort>());
                        break;
                        case GL_UNSIGNED_SHORT:
                        _a->boundus[i][0] = std::min(_a->boundus[i][1], m.get<GLushort>());
                        break;
                        case GL_BYTE:
                        _a->boundb[i][0] = std::min<GLbyte>(_a->boundb[i][1], m.get<GLbyte>());
                        break;
                        case GL_UNSIGNED_BYTE:
                        _a->boundub[i][0] = std::min(_a->boundub[i][1], m.get<GLubyte>());
                        break;
                    }
                }
            }
        }
        struct parse {
            char *data;
            int len;
            struct gltfasset *asset;
        };
        struct parser {
            parser() {
                q.get_observable()
                .observe_on(rxcpp::observe_on_new_thread())
                .subscribe(subscription, [](struct parse parse) {
                    try {
                        parse.asset->parse(parse.data,parse.len);
                    } catch (std::exception e) {
                        fprintf(stderr,"parse error! %s\n", e.what());
                        if (parse.data) fputs(parse.data,stderr);
                        parse.asset->error = 1;
                    }
                    free(parse.data);
                });
            }
            ~parser() {
                subscription.unsubscribe();
            }
            rxcpp::subjects::subject<struct parse> q;
            private:
            rxcpp::composite_subscription subscription;
        };
        static struct Init {
            std::unordered_map<std::string, int> COMPONENT_COUNT = {
                {"SCALAR", 1},
                {"VEC2", 2},
                {"VEC3", 3},
                {"VEC4", 4},
                {"MAT2", 4},
                {"MAT3", 9},
                {"MAT4", 16}
            };
            std::unordered_map<GLenum, size_t> COMPONENT_SIZE = {
                {GL_BYTE,sizeof(GLbyte)},
                {GL_UNSIGNED_BYTE,sizeof(GLubyte)},
                {GL_SHORT,sizeof(GLshort)},
                {GL_UNSIGNED_SHORT,sizeof(GLushort)},
                {GL_UNSIGNED_INT,sizeof(GLuint)},
                {GL_FLOAT,sizeof(float)}
            };
            struct parser parser;
        } init;
    };
    struct gltfprogram {
        int error;
        engine::program program;
        engine::uniformMatrix4fv projection_uniform,view_uniform,model_uniform;
        engine::uniform4fv baseColorFactor_uniform;
        engine::uniform1i baseColorTexture_uniform;
        engine::uniformMatrix4fv joints_uniform;;
        engine::attribute position_attribute;
        engine::attribute normal_attribute;
        engine::attribute tangent_attribute;
        engine::attribute texCoord0_attribute;
        engine::attribute joints0_attribute;
        engine::attribute weights0_attribute;
        engine::texture defaultTexture;
        engine::gltfanimation *animation;
        void operator()(int x, int y, int w, int h, gltfasset &a, float *view=0, float *projection=0, float *model=0) {
        static HMM_Mat4 identity = HMM_M4D(1.0);
            if (error) goto error;
            if (!a.scene) return;
            if (!defaultTexture) {
                static uint32_t color = RGBA(255,255,255,255);
                defaultTexture(1,1,GL_RGBA);
                defaultTexture.upload(&color);
            }
            program({_TEXT(
                uniform mat4 joints[128];
                attribute vec3 POSITION;
                attribute vec3 NORMAL;
                attribute vec3 TANGENT;
                attribute vec2 TEXCOORD_0;
                attribute vec4 JOINTS_0;
                attribute vec4 WEIGHTS_0;
                uniform mat4 model;
                uniform mat4 view;
                uniform mat4 projection;
                varying vec2 texCoord0;
                void main() {
                    texCoord0 = TEXCOORD_0;
                    mat4 skin =
                        WEIGHTS_0.x * joints[int(JOINTS_0.x)] +
                        WEIGHTS_0.y * joints[int(JOINTS_0.y)] +
                        WEIGHTS_0.z * joints[int(JOINTS_0.z)] +
                        WEIGHTS_0.w * joints[int(JOINTS_0.w)];
                    float w = WEIGHTS_0.x+WEIGHTS_0.y+WEIGHTS_0.z+WEIGHTS_0.w;
                    if (w<1.0) {
                        skin += (1.0-w)*model;
                    }
                    gl_Position = projection * view * skin * vec4(POSITION, 1.0);
                }
            )},{_TEXT(
                uniform vec4 baseColorFactor;
                uniform sampler2D baseColorTexture;
                varying vec2 texCoord0;
                void main() {
                    gl_FragColor = baseColorFactor * texture2D(baseColorTexture,texCoord0);
                }
            )});
            position_attribute("POSITION",program);
            normal_attribute("NORMAL",program);
            tangent_attribute("TANGENT",program);
            texCoord0_attribute("TEXCOORD_0",program);
            joints0_attribute("JOINTS_0",program);
            weights0_attribute("WEIGHTS_0",program);
            projection_uniform("projection",program,projection?projection:&identity.Elements[0][0]);
            view_uniform("view",program,view?view:&identity.Elements[0][0]);
            glViewport(x,y,w,h);
            {
            struct gltfnoderef *n=0;
            while ((n = (struct gltfnoderef *)utarray_next(a.scene->nodes, n))) {
                struct gltfnode *node = (struct gltfnode *)utarray_eltptr(a.nodes, n->index);
                draw(a,node,model?*(HMM_Mat4*)model:identity);
            }
            }
            return;
            error:;
        }
        private: void draw(const gltfasset &a, gltfnode *node, HMM_Mat4 transform=HMM_M4D(1.0)) {
            node->transform = HMM_MulM4(transform,node->matrix);
            if (node->mesh) {
                gltfskin *skin = node->skin;
                if (skin) {
                    HMM_Mat4 joints[128];
                    gltfjointref *j = 0;
                    while ((j = (gltfjointref *)utarray_next(skin->joints,j))) {
                        gltfnode *jn = (gltfnode *)utarray_eltptr(a.nodes,j->index);
                        int ji = utarray_eltidx(skin->joints,j);
                        if (skin->inverseBindMatrices.bufferview) {
                            if (!skin->inverseBindMatrices.bufferview->gltf->data) return;
                            HMM_Mat4 *inverseBindMatrices = (HMM_Mat4*)(skin->inverseBindMatrices.bufferview->gltf->data+skin->inverseBindMatrices.bufferview->offset);
                            joints[ji] = HMM_MulM4(jn->transform,inverseBindMatrices[ji]);
                        } else {
                            joints[ji] = jn->transform; //HMM_Invert?
                            puts("debug no inverseBindMatrices for skin");
                        }
                    }
                    joints_uniform("joints",program,(float*)joints,utarray_len(skin->joints));
                }
                model_uniform("model",program,&node->transform.Elements[0][0]);
                struct gltfprimitive *p = 0;
                while ((p = (struct gltfprimitive *)utarray_next(node->mesh->primitives, p))) {
                    static float defaultBaseColorFactor[] = {1,1,1,1};
                    int activeTexture = 0;
                    baseColorFactor_uniform("baseColorFactor",program, p->material?&p->material->pbrMetallicRoughness.baseColorFactor.Elements[0]:defaultBaseColorFactor);
                    if (p->material&&p->material->pbrMetallicRoughness.baseColorTexture) (*p->material->pbrMetallicRoughness.baseColorTexture)();
                    glActiveTexture(GL_TEXTURE0+activeTexture);
                    glBindTexture(GL_TEXTURE_2D,p->material&&p->material->pbrMetallicRoughness.baseColorTexture ? *p->material->pbrMetallicRoughness.baseColorTexture : defaultTexture);
                    baseColorTexture_uniform("baseColorTexture",program, activeTexture);
                    ++activeTexture;
                    struct gltfattribute *a = 0;
                    a = (gltfattribute*)utarray_eltptr(p->attributes,p->position_attribute);
                    if (a) {
                        assert(!strcmp(a->name, "POSITION"));
                        (*a->bufferview)();
                        if (!a->bufferview->buffer) continue;
                        position_attribute.enable(a->componentCount,a->type,a->normalized,a->stride,0);
                    }
                    a = (gltfattribute*)utarray_eltptr(p->attributes,p->normal_attribute);
                    if (a) {
                        assert(!strcmp(a->name, "NORMAL"));
                        (*a->bufferview)();
                        if (!a->bufferview->buffer) continue;
                        normal_attribute.enable(a->componentCount,a->type,a->normalized,a->stride,0);
                    }
                    a = (gltfattribute*)utarray_eltptr(p->attributes,p->tangent_attribute);
                    if (a) {
                        assert(!strcmp(a->name, "TANGENT"));
                        (*a->bufferview)();
                        if (!a->bufferview->buffer) continue;
                        tangent_attribute.enable(a->componentCount,a->type,a->normalized,a->stride,0);
                    }
                    a = (gltfattribute*)utarray_eltptr(p->attributes,p->texCoord0_attribute);
                    if (a) {
                        assert(!strcmp(a->name, "TEXCOORD_0"));
                        (*a->bufferview)();
                        if (!a->bufferview->buffer) continue;
                        texCoord0_attribute.enable(a->componentCount,a->type,a->normalized,a->stride,0);
                    }
                    a = (gltfattribute*)utarray_eltptr(p->attributes,p->joints0_attribute);
                    if (a) {
                        assert(!strcmp(a->name, "JOINTS_0"));
                        (*a->bufferview)();
                        if (!a->bufferview->buffer) continue;
                        joints0_attribute.enable(a->componentCount,a->type,a->normalized,a->stride,0);
                    }
                    a = (gltfattribute*)utarray_eltptr(p->attributes,p->weights0_attribute);
                    if (a) {
                        assert(!strcmp(a->name, "WEIGHTS_0"));
                        (*a->bufferview)();
                        if (!a->bufferview->buffer) continue;
                        weights0_attribute.enable(a->componentCount,a->type,a->normalized,a->stride,0);
                    } else {
                        weights0_attribute.set4f(0,0,0,0);
                    }
                    if (p->bufferview) {
                        (*p->bufferview)();
                        if (!p->bufferview->gltf->data) continue;
                        glDrawElements(p->mode,p->count,p->type,(const void*)p->offset);
                    } else {
                        glDrawArrays(p->mode,0,p->count);
                    }
                }
            }
            if (node->children) {
                struct gltfnoderef *c=0;
                while ((c = (struct gltfnoderef *)utarray_next(node->children, c))) {
                    struct gltfnode *child = (struct gltfnode *)utarray_eltptr(a.nodes, c->index);
                    draw(a,child,node->transform);
                }
            }
        }
    };
    struct gltfanimate {
        int error;
        gltfanimation *animation;
        double start;
        inline void operator()(gltfasset &a, const char *name, float t, bool loop=true) {
            gltfanimationchannel *c;
            if (error) goto error;
            if (!a.scene) return;
            if (!animation) {
                HASH_FIND_STR(a.animations,name,animation);
                if (!animation) {
                    fprintf(stderr, "couldn't find animation %s\n", name);
                    error = 1;
                    goto error;
                }
            }
            if (!start) start=glfwGetTime();
            t = glfwGetTime()-start;
            //t -= 1;
            //t *= 0.25;
            c = 0;
            while ((c = (gltfanimationchannel*)utarray_next(animation->channels,c))) {
                if (!c->target.path) continue;
                gltfanimationsampler *s = c->sampler;
                if (!s->input.bufferview->gltf->data) continue;
                if (!s->output.bufferview->gltf->data) continue;
                gltfnode *n = (gltfnode *)utarray_eltptr(a.nodes,c->target.node);
                float *time = (float*)(s->input.bufferview->gltf->data+s->input.bufferview->offset);
                float *output = (float*)(s->output.bufferview->gltf->data+s->output.bufferview->offset);
                float end = time[s->input.count-1];
                if (loop) t = fmod(t,end);
                int i,j;
                i=0;j=0;
                if (s->input.count>1) for (j=1; j<s->input.count && time[j]<t; ++i,++j) {}
                float *path = (float*)((uint8_t*)n+c->target.path);
                assert(s->output.size()==sizeof(float));
                // todo slerp
                for (int c = 0; c < s->output.componentCount; ++c) {
                    float a = output[i*s->output.componentCount+c];
                    float b = output[j*s->output.componentCount+c];
                    float l = HMM_ABS(time[j]-time[i])<FLT_EPSILON?(t-time[i])/(time[j]-time[i]):1;
                    path[c] = HMM_Lerp(a,HMM_Clamp(0,1,l),b);
                }
                //memcpy(path,&output[i*s->output.componentCount],s->output.componentCount*sizeof(float));

                // if path != weights
                n->matrix = HMM_MulM4(HMM_Translate(n->translation),
                    HMM_MulM4(HMM_QToM4(n->rotation),HMM_Scale(n->scale))
                );
            }
            return;
            error:;
        }
    };
    struct cubestorage {};
#if !_WIN32
    struct videotexture : texture {
        void operator()(){}
    };
#else
    #include "_imf.h"
    struct videotexture : texture {
        IMFMediaSource *webcam;
        int error;
        IMFSourceReader *webvideo;
        IMFTransform* colorConverter;
        inline void operator()() {
            HRESULT hr;
            if (error) return;
            if (!webcam) {
                hr = CreateVideoCaptureDevice(&webcam);
            }
#if _DEBUG
            hr = EnumerateCaptureFormats(webcam);
#endif
            if (!webcam) return;
            if (!reader.self) {
                IMFAttributes *pAttributes = NULL;
                hr = MFCreateAttributes(&pAttributes, 1);
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }
#if 1
                IMFSourceReaderCallback *webcallback = &reader;
                hr = pAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, webcallback);
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }
#endif
                hr = MFCreateSourceReaderFromMediaSource(webcam, pAttributes, &webvideo);
                SafeRelease(&pAttributes);
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }
#if 0
                hr = ConfigureDecoder(webvideo, MF_SOURCE_READER_FIRST_VIDEO_STREAM);
                if (FAILED(hr)) {
                    fprintf(stderr, "[VIDEO] failed to configure decoder\n");
                    error = 1;
                    break;
                }
#elif 0
                hr = CoCreateInstance(CLSID_CColorConvertDMO,0,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&colorConverter));
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }
#else
                hr = CoCreateInstance(CLSID_VideoProcessorMFT,0,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&colorConverter));
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }
#endif
                IMFMediaType *inputType = 0;
                for (int mediaTypeIndex = 0; hr == S_OK; ++mediaTypeIndex) {
                    IMFMediaType *mediaType;
                    hr = webvideo->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, mediaTypeIndex, &mediaType);
                    if (FAILED(hr)) {
                        break;
                    }
                    GUID subtype;
                    hr = mediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
                    if (FAILED(hr)) {
                        continue;
                    }
                    UINT32 w, h;
                    hr = MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &w, &h);
                    if (FAILED(hr)) {
                        continue;
                    }
                    inputType = mediaType;
                    break;
                }
                if (!inputType) {
                    fprintf(stderr, "[VIDEO] couldn't find media compatible input\n");
                    error = 1;
                    return;
                }
                LogMediaType(inputType);
                UINT32 w, h;
                hr = MFGetAttributeSize(inputType, MF_MT_FRAME_SIZE, &w, &h);
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }

                IMFMediaType *outputType;
                hr = MFCreateMediaType(&outputType);
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }
                hr = inputType->CopyAllItems(outputType);
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }
                hr =outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }

#if 1
                hr = colorConverter->SetInputType(0,inputType,0);
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }
                hr = colorConverter->SetOutputType(0,outputType,0);
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }
                MFT_OUTPUT_STREAM_INFO outputStreamInfo;
                hr = colorConverter->GetOutputStreamInfo(0, &outputStreamInfo);
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }
#else
                hr = webvideo->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, outputType);
                if (FAILED(hr)) {
                    error = 1;
                    return;
                }
#endif

                width = w;
                height = h;
                reader.self = this;
            }
            hr = webvideo->ReadSample(reader._convert.dwStreamID, 0,
                    //&streamIndex, &streamFlags, &timestamp, &sample);
                    NULL, NULL, NULL, NULL);
            texture::operator()(width,height,GL_BGRA_EXT);
            reader();
        }
        struct _IMFSourceReaderCallback : IMFSourceReaderCallback {
            struct videotexture *self;
            MFT_OUTPUT_DATA_BUFFER _convert = { (DWORD)0 };
            DWORD _status = 0;
            void *_texdata = 0;
            unsigned _framecount = 0;
            unsigned _uploadcount = 0;
            void operator()() {
                if (!(_uploadcount < _framecount)) return;
                self->upload(_texdata);
                _uploadcount = _framecount;
            }
            virtual HRESULT STDMETHODCALLTYPE OnReadSample( 
                _In_  HRESULT hrStatus,
                _In_  DWORD dwStreamIndex,
                _In_  DWORD dwStreamFlags,
                _In_  LONGLONG llTimestamp,
                _In_opt_  IMFSample *pSample) {
                HRESULT hr;
                    if (FAILED(hrStatus)) {
                        return hrStatus;
                    }
                    if (!_convert.pSample) {
                        MFCreateSample(&_convert.pSample);
                        IMFMediaBuffer *buffer;
                        MFCreateMemoryBuffer(self->width * self->height * 4, &buffer);
                        _convert.pSample->AddBuffer(buffer);
                    }
                    OUTPUT:
                    DWORD outputStatus;
                    do {
                        hr = self->colorConverter->GetOutputStatus(&outputStatus);
                        if (FAILED(hr)) {
                            break;
                        }
                        if (!(outputStatus & MFT_OUTPUT_STATUS_SAMPLE_READY)) {
                            goto INPUT;
                        }
                    } while (!(outputStatus & MFT_OUTPUT_STATUS_SAMPLE_READY));
                    _convert.dwStatus = 0;
                    _convert.pEvents = 0;
                    hr = self->colorConverter->ProcessOutput(0, 1, &_convert, &_status);
                    switch (hr) {
                    case MF_E_TRANSFORM_NEED_MORE_INPUT:
                        goto INPUT;
                    }
                    if (FAILED(hr)) {
                        return hr;
                    }
                    IMFMediaBuffer *buffer;
                    hr = _convert.pSample->GetBufferByIndex(0, &buffer);
                    if (FAILED(hr)) {
                        return hr;
                    }
                    BYTE *data;
                    DWORD size;
                    buffer->GetCurrentLength(&size);
                    DWORD length;
                    hr = buffer->Lock(&data, &size, &length);
                    if (SUCCEEDED(hr)) {
                        _texdata = realloc(_texdata, length);
                        memcpy(_texdata, data, length);
                        buffer->Unlock();
                        ++_framecount;
                    }
                    
                    INPUT:
                    if (!pSample) {
                        return S_OK;
                    }
                    DWORD inputStatus;
                    do {
                        hr = self->colorConverter->GetInputStatus(0,&inputStatus);
                        if (FAILED(hr)) {
                            break;
                        }
                        if (!(inputStatus & MFT_INPUT_STATUS_ACCEPT_DATA)) {
                            goto OUTPUT;
                        }
                    } while (!(inputStatus & MFT_INPUT_STATUS_ACCEPT_DATA));
                    hr = self->colorConverter->ProcessInput(0, pSample, 0);
                    switch (hr) {
                    case MF_E_NOTACCEPTING:
                        goto OUTPUT;
                    }
                    if (FAILED(hr)) {
                        fprintf(stderr, "[VIDEO] processing failed\n");
                        return hr;
                    }
                    
                    return hr;
                }
            
            virtual HRESULT STDMETHODCALLTYPE OnFlush( 
                _In_  DWORD dwStreamIndex) {
                    return S_OK;
                }
            
            virtual HRESULT STDMETHODCALLTYPE OnEvent( 
                _In_  DWORD dwStreamIndex,
                _In_  IMFMediaEvent *pEvent) {
                    return S_OK;
                }

            virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
                REFIID riid,
                _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) {
                static const QITAB qit[] =
                {
                    QITABENT(_IMFSourceReaderCallback, IMFSourceReaderCallback),
                    { 0 },
                };
                HRESULT hr;
                hr = QISearch(this, qit, riid, ppvObject);
                return hr;
                }

            virtual ULONG STDMETHODCALLTYPE AddRef( void) {
                return InterlockedIncrement(&refCount);
            }

            virtual ULONG STDMETHODCALLTYPE Release( void) {
                ULONG count = InterlockedDecrement(&refCount);
                return count;
            }
            //private:
            ULONG refCount;
        } reader;
        static struct Init {
            Init() {
                MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
            }
        } init;
    };
#endif
}