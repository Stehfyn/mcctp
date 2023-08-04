#include "mcctp.h"
#include <system_error>
namespace mcctp {
	namespace {
#pragma warning(push)
#pragma warning(disable: 4996)
        using converter = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>;

        const char* s_DefaultVertexShaderSrc = R"(
#version 330 core

const vec2 vertices[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2(1.0,  1.0)
);

void main() {
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
}
)";

        const char* s_DefaultFragmentShaderSrc = R"(
#version 330 core

uniform sampler2D tex;

out vec4 FragColor;

void main() {
    FragColor = texture(tex, gl_FragCoord.xy / textureSize(tex, 0));
    //FragColor = vec4(1.0, 0.0, 0.0, 1.0);  // Render everything red
}
)";

        static GLuint CompileShader(GLenum type, const char* src) {
            GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &src, NULL);
            glCompileShader(shader);

            GLint status;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
            if (!status) {
                char infoLog[512];
                glGetShaderInfoLog(shader, 512, NULL, infoLog);
                std::cerr << "Error compiling shader:\n" << infoLog << std::endl;
            }

            return shader;
        }

        static GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader) {
            GLuint program = glCreateProgram();
            glAttachShader(program, vertexShader);
            glAttachShader(program, fragmentShader);
            glLinkProgram(program);

            GLint status;
            glGetProgramiv(program, GL_LINK_STATUS, &status);
            if (!status) {
                char infoLog[512];
                glGetProgramInfoLog(program, 512, NULL, infoLog);
                std::cerr << "Error linking program:\n" << infoLog << std::endl;
            }

            return program;
        }
	}

    void Initialize(std::filesystem::path path, TexturePackFlags flags) {
        ctx::Instance()->SetPathPrefix(path);
        ctx::Instance()->SetField(flags);
    }

    bool MemoryMapTexturePacks(void) {
        TexturePackField field = ctx::Instance()->GetField();
        std::cout << "Field: " << field << std::endl;
        for (uint8_t i = 0; i < TexturePackCount; ++i) {
            TexturePackFlags flag = (TexturePackFlags)(1 << i);
            if (field.HasFlag(flag)) {
                std::cout << "Mapping " << FlagToBasename.at(flag) << "..." << std::flush;
                bool success = ctx::Instance()->MemoryMapTexturePack(
                    flag, converter().from_bytes(FlagToBasename.at(flag)));
            }
        }
        return true;
    }

    void UnmapTexturePacks(void) { 
        for (uint8_t i = 0; i < TexturePackCount; ++i) {
            TexturePackFlags flag = (TexturePackFlags)(1 << i);
            ctx::Instance()->UnmapTexturePack(flag);
        }
    }

    bool IndexTexturePacks(void) {
        TexturePackField field = ctx::Instance()->GetField();
        for (uint8_t i = 0; i < TexturePackCount; ++i) {
            TexturePackFlags flag = (TexturePackFlags)(1 << i);
            if (field.HasFlag(flag)) {
                ctx::Instance()->IndexTexturePack(flag);
            }
        }
        return true;
    }

    bool MemoryMapAndIndexTexturePacks(void) {
        TexturePackField field = ctx::Instance()->GetField();
        for (uint8_t i = 0; i < TexturePackCount; ++i) {
            TexturePackFlags flag = (TexturePackFlags)(1 << i);
            if (field.HasFlag(flag)) {
                std::cout << "Mapping " << FlagToBasename.at(flag) << "..." << std::flush;
                ctx::Instance()->MemoryMapTexturePack(flag, converter().from_bytes(FlagToBasename.at(flag)));
                std::cout << "Indexing " << FlagToBasename.at(flag) << "..." << std::flush;
                ctx::Instance()->IndexTexturePack(flag);
            }
        }
        return true;
    }

    bool DumpTexturePacks(DumpFormatFlags dump_format, DumpCompressionFlags compression_flag) {
        TexturePackField field = ctx::Instance()->GetField();
        for (uint8_t i = 0; i < TexturePackCount; ++i) {
            TexturePackFlags flag = (TexturePackFlags)(1 << i);
            if (field.HasFlag(flag)) {
                ctx::Instance()->DumpTexturePack(flag, dump_format, compression_flag);
            }
        }
        return true;
    }

    void ClearTexturePackDumps(void) { 
        TexturePackField field = ctx::Instance()->GetField();
        for (uint8_t i = 0; i < TexturePackCount; ++i) {
            TexturePackFlags flag = (TexturePackFlags)(1 << i);
            ctx::Instance()->DeleteTexturePackDump(flag);
        }
    }

    GLuint RenderDDSToTexture(TexturePackResource res) { 
        auto *inst = ctx::Instance();
        return inst->RenderDDSToTexture(res);
    }

    bool ShareWithWGLContext(HGLRC hrc) {
        auto *inst = ctx::Instance();
        bool res = inst->ShareWithWGLContext(hrc);
        if (!res) {
            MessageBox(NULL, std::to_wstring(GetLastError()).c_str(), L"", MB_OK);
            return false;
        } else {
            return true;
        }
    }

    bool InjectResource(std::string src, std::string dst, TexturePackFlags flag) { 
        auto inst = ctx::Instance();
        return inst->InjectAndWritePack(src, dst, flag);
    }

    std::stringstream BuildDDSHeaderForResource(TexturePackResource res) {
        std::stringstream bytestream;
        unsigned char data1[] = {0x44, 0x44, 0x53, 0x20, 0x7C, 0x00,
                                 0x00, 0x00, 0x07, 0x10, 0x08, 0x00};
        size_t data1_size = sizeof(data1) / sizeof(data1[0]);

        bytestream.write((const char *)data1, data1_size);
        bytestream.write(reinterpret_cast<const char *>(&res.Height), sizeof(res.Height));
        bytestream.write(reinterpret_cast<const char *>(&res.Width), sizeof(res.Width));

        uint32_t dw = 0x0400;
        bytestream.write(reinterpret_cast<const char *>(&dw), sizeof(dw));

        unsigned char data2[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00,
                                 0x00, 0x04, 0x00, 0x00, 0x00, 0x44, 0x58, 0x54};
        size_t data2_size = sizeof(data2) / sizeof(data2[0]);
        bytestream.write((const char *)data2, data2_size);

        unsigned char b = 0x00;
        if (res.Format == ResourceFormat::DXT1) {
            b = 0x31;
        } else if (res.Format == ResourceFormat::DXT3) {
            b = 0x33;
        } else if (res.Format == ResourceFormat::DXT5) {
            b = 0x35;
        }
        bytestream.write(reinterpret_cast<const char *>(&b), sizeof(b));

        unsigned char data3[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        size_t data3_size = sizeof(data3) / sizeof(data3[0]);
        bytestream.write((const char *)data3, data3_size);
        return bytestream;
    }

    GLuint ctx::RenderDDSToTexture(TexturePackResource res) {
        HDC hdc = wglGetCurrentDC();
        HGLRC hrc = wglGetCurrentContext();

        wglMakeCurrent(m_hDC, m_hRC);

        char *start = res.Origin + res.Offset;

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        uint16_t decompress_from = 0;
        if (res.Format == ResourceFormat::DXT1) {
            decompress_from = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        } else if (res.Format == ResourceFormat::DXT3) {
            decompress_from = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        } else if (res.Format == ResourceFormat::DXT5) {
            decompress_from = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        } else {
            // Handle unsupported formats
        }

        glCompressedTexImage2D(GL_TEXTURE_2D, 0, decompress_from, res.Width, res.Height, 0,
                               res.Size, start);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D);
        // Create a new texture to store the decompressed image
        //GLuint rgbaTex;
        //glGenTextures(1, &rgbaTex);
        //glBindTexture(GL_TEXTURE_2D, rgbaTex);
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, res.Width, res.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
        //             NULL);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        //
        //// Create a framebuffer object (FBO) and attach the decompressed texture to it
        //GLuint fbo;
        //glGenFramebuffers(1, &fbo);
        //glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rgbaTex, 0);
        //
        //// Check FBO status
        //GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        //if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
        //    // Handle error
        //}
        //glViewport(0, 0, res.Width, res.Height);
        //glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        //glClear(GL_COLOR_BUFFER_BIT);
        //// Render a full-screen quad with the compressed texture bound here
        //// The decompressed image will be rendered into rgbTex
        //GLint texLocation = glGetUniformLocation(m_OutputProgram, "tex");
        //
        //// Bind your program
        //glUseProgram(m_OutputProgram);
        //
        //// Bind your texture to texture unit 0
        //glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_2D, tex);
        //
        //// Set the 'tex' uniform to texture unit 0
        //glUniform1i(texLocation, 0);
        //
        //// Render the fullscreen quad
        //glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        //
        //// Bind the FBO and read the pixels from the decompressed texture
        ////glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        ////std::vector<char> pixels(res.Width * res.Height * 4);
        ////glReadPixels(0, 0, res.Width, res.Height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        ////
        ////GLuint sharedtex;
        ////glGenTextures(1, &sharedtex);
        ////glBindTexture(GL_TEXTURE_2D, sharedtex);
        ////glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ////glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        ////glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, res.Width, res.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        ////glBindTexture(GL_TEXTURE_2D, 0);
        //
        //glBindFramebuffer(GL_FRAMEBUFFER, 0);
        //glBindTexture(GL_TEXTURE_2D, 0);
        ////glDeleteTextures(1, &rgbaTex);
        //glDeleteTextures(1, &tex);
        //glDeleteFramebuffers(1, &fbo);

        wglMakeCurrent(hdc, hrc);

        return tex;
    }

    bool ctx::ShareWithWGLContext(HGLRC hrc) { 
        wglMakeCurrent(NULL, NULL);
        bool res = wglShareLists(hrc, m_hRC);
        return res;
    }

    FileMapping::FileMapping(std::wstring path) {
        if (!TryCreateMapping(path)) {
            throw std::exception("File mapping could not be created!");
        }
    }

    FileMapping::FileMapping(FileMapping&& other) noexcept {
        // Transfer ownership of the handles from the other object
        hFile = other.hFile;
        hMap = other.hMap;
        lpMap = other.lpMap;

        // Reset the other object's members to safe defaults
        other.hFile = INVALID_HANDLE_VALUE;
        other.hMap = NULL;
        other.lpMap = NULL;
    }

    FileMapping& FileMapping::operator=(FileMapping&& other) noexcept {
        if (this != &other) {
            UnmapViewOfFile(lpMap);
            CloseHandle(hMap);
            CloseHandle(hFile);

            // Transfer ownership of the handles from the other object
            hFile = other.hFile;
            hMap = other.hMap;
            lpMap = other.lpMap;

            // Reset the other object's members to safe defaults
            other.hFile = INVALID_HANDLE_VALUE;
            other.hMap = NULL;
            other.lpMap = NULL;
        }
        return *this;
    }

    FileMapping::~FileMapping() {
        if (lpMap != NULL)
            UnmapViewOfFile(lpMap);
        if (hMap != NULL)
            CloseHandle(hMap);
        if (hFile != INVALID_HANDLE_VALUE)
            CloseHandle(hFile);
    }
    bool FileMapping::TryCreateMapping(std::wstring path) {
        hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "Could not open file: ";
            std::cerr << GetLastError() << std::endl;
            return false;
        }

        hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (hMap == NULL) {
            std::cerr << "Could not create file mapping ";
            std::cerr << GetLastError() << std::endl;
            CloseHandle(hFile);
            return false;
        }

        lpMap = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
        if (lpMap == NULL) {
            std::cerr << "Could not map view of file ";
            std::cerr << GetLastError() << std::endl;
            CloseHandle(hMap);
            CloseHandle(hFile);
            return false;
        }

        std::cout << "Success!" << std::endl;
        return true;
    }

    std::ostream& operator<<(std::ostream& os, const TexturePackBlock& block) {
        os << "Key: " << block.Key << ", "
            << "Size: " << block.Size << ", "
            << "Offset: " << block.Offset << ", ";
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const TexturePackResource& res) {
        os << "Name: " << res.Name << ", "
            << "Offset: " << res.Offset << ", "
            << "Size: " << res.Size << ", "
            << "Width: " << res.Width << ", "
            << "Height: " << res.Height << ", "
            << "Format: " << (int)res.Format;
        return os;
    }

    ctx *ctx::Instance() {
        static ctx *Context = new ctx;
        static bool init = true;
        if (init) {
            fpng::fpng_init();
            if (glewInit() == GLEW_OK) {
                Context->InitializeGLContext();
                Context->m_ClientHasWGLContext = true;
            } else {
                Context->m_ClientHasWGLContext = false;
            }
            init = false;
        }
        return Context;
    }

    void ctx::InitializeGLContext(void) {
        HDC hdc = wglGetCurrentDC();
        HGLRC hrc = wglGetCurrentContext();

        m_hDC = hdc;
        m_hRC = wglCreateContext(m_hDC);
        assert(wglShareLists(hrc, m_hRC) == true);
        bool result = wglMakeCurrent(m_hDC, m_hRC);

        m_OutputVertexShader = CompileShader(GL_VERTEX_SHADER, s_DefaultVertexShaderSrc);
        m_OutputFragmentShader = CompileShader(GL_FRAGMENT_SHADER, s_DefaultFragmentShaderSrc);
        m_OutputProgram =
            LinkProgram(m_OutputVertexShader, m_OutputFragmentShader);
        glDeleteShader(m_OutputVertexShader);
        glDeleteShader(m_OutputFragmentShader);

        result = wglMakeCurrent(hdc, hrc);
    }

    bool ctx::MemoryMapTexturePack(TexturePackFlags flag, std::wstring texture_pack_basename) {
        try {
            using namespace std::filesystem;
            path perm =
                m_PathPrefix.wstring() + L'\\' + texture_pack_basename + L".perm.bin";
            path temp =
                m_PathPrefix.wstring() + L'\\' + texture_pack_basename + L".temp.bin";
            path idx =
                m_PathPrefix.wstring() + L'\\' + texture_pack_basename + L".perm.idx";

            if (!(exists(perm) && (exists(temp) || exists(idx))))
                throw std::exception("Exception in MemoryMapTexturePack() \"Invalid TexturePack\"");

            if (std::filesystem::exists(temp)) {
                TexturePack tp = {
                    FileMapping(perm),
                    FileMapping(temp),
                    TexturePackType::TEMP
                };
                m_TexturePackFileMap.insert(std::make_pair(flag, std::move(tp)));
                return true;
            }
            else {
                TexturePack tp = {
                    FileMapping(perm),
                    FileMapping(idx),
                    TexturePackType::PERM
                };
                m_TexturePackFileMap.insert(std::make_pair(flag, std::move(tp)));
            }
        }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            return false;
        }
    }

    void ctx::UnmapTexturePack(TexturePackFlags flag) { 
        m_TexturePackFileMap.erase(flag);
        m_TexturePackIndexMap.erase(flag);
    }

    bool ctx::IsTexturePackMapped(TexturePackFlags flag) const { 
        return m_TexturePackFileMap.find(flag) != m_TexturePackFileMap.end();
    }

    bool ctx::IsTexturePackIndexed(TexturePackFlags flag) const {
        return m_TexturePackIndexMap.find(flag) != m_TexturePackIndexMap.end();
    }

    bool ctx::_IndexTexturePack(TexturePackFlags flag) {
        try {
            const auto& fm = m_TexturePackFileMap.at(flag);
            // impl
            std::cout << "Success!" << std::endl;
            return true;
        }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            return false;
        }
    }
    bool ctx::IndexTexturePack(TexturePackFlags flag) {
        const auto& fm = GetPerm(m_TexturePackFileMap.at(flag));
        const LPVOID lpMap = fm.GetMapView();
        const HANDLE hFile = fm.GetFile();
        char* fptr = static_cast<char*>(lpMap);
        char* fend = fptr + GetFileSize(hFile, NULL);
        std::cout << "TexturePackSize: (" << GetFileSize(hFile, NULL) << ")" << std::endl;

        std::vector<TexturePackBlock> blocks;
        std::vector<TexturePackResource> resources;
        int neither = 0;

        while (fptr < fend) {
            uint32_t pos = fptr - static_cast<char*>(lpMap);
            uint32_t block_key = *(uint32_t*)fptr; fptr += sizeof(uint32_t);
            int block_size = *(int*)fptr; fptr += sizeof(int);

            if (block_key == 0x5E73CDD7) {
                TexturePackBlock block = { block_key, block_size, pos };
                auto it = m_TexturePackIndexMap.find(flag);
                if (it != m_TexturePackIndexMap.end()) {
                    it->second.insert({ std::to_string(blocks.size()), (TexturePackEntry)block });
                    std::cout << "Added: " << block << std::endl;
                }
                else {
                    TexturePackIndex index;
                    index.insert({ std::to_string(blocks.size()), (TexturePackEntry)block });
                    m_TexturePackIndexMap.insert({ flag, index });
                    std::cout << "Added: " << block << std::endl;
                }
                blocks.push_back(block);
            }
            else if (block_key == 0xCDBFA090) {
                int start = (int)pos;
                fptr = static_cast<char*>(lpMap) + start + 0x44;

                char namebytes[0x20];
                std::memcpy(namebytes, fptr, sizeof(namebytes));
                size_t length = std::find(namebytes, namebytes + sizeof(namebytes), '\0') - namebytes;
                std::string name(namebytes, length);
                fptr += sizeof(namebytes);

                fptr = static_cast<char*>(lpMap) + start + 0x6c;
                char form = *(char*)fptr;
                fptr += sizeof(char);

                if (form >= static_cast<char>(ResourceFormat::INVALID)) {
                    std::cout << name << " has an unrecognized format? Skipping." << std::endl;
                    fptr = static_cast<char*>(lpMap) + pos + block_size + 0x10;
                    continue;
                }

                fptr = static_cast<char*>(lpMap) + start + 0x74;
                short wid = *(short*)fptr;
                fptr += sizeof(short);
                short hig = *(short*)fptr;
                fptr += sizeof(short);

                fptr = static_cast<char*>(lpMap) + start + 0x88;
                int size = *(int*)fptr;
                fptr += sizeof(int);

                fptr = static_cast<char*>(lpMap) + start + 0x90;
                uint32_t off = *(uint32_t*)fptr;
                fptr += sizeof(uint32_t);

                TexturePackType type = GetType(m_TexturePackFileMap.at(flag));
                char * origin =
                    (type == TexturePackType::PERM)
                    ? static_cast<char *> (GetPerm(m_TexturePackFileMap.at(flag)).GetMapView())
                        : static_cast<char *>(GetTemp(m_TexturePackFileMap.at(flag)).GetMapView());

                TexturePackResource res = {name,
                                           origin,
                                           0,
                                           size,
                                           wid,
                                           hig,
                                           static_cast<ResourceFormat>(form),
                                           type};

                res.Offset = blocks.empty() ? off : blocks.back().Offset + off;

                auto it = m_TexturePackIndexMap.find(flag);
                if (it != m_TexturePackIndexMap.end()) {
                    it->second.insert({ res.Name, (TexturePackEntry)res });
                    std::cout << "Added: " << res << std::endl;
                }
                else {
                    TexturePackIndex index;
                    index.insert({ res.Name, (TexturePackEntry)res });
                    m_TexturePackIndexMap.insert({ flag, index });
                    std::cout << "Added: " << res << std::endl;
                }
                resources.push_back(res);
            }
            else {
                neither += 1;
            }
            fptr = static_cast<char*>(lpMap) + pos + block_size + 0x10;
        }
        std::cout << "TexturePackStats: " << "Files: " << resources.size() << " Blocks: " << blocks.size() << " Neither: " << neither << std::endl;

        return true;
    }
    bool ctx::DumpTexturePack(TexturePackFlags flag, DumpFormatFlags format_flag,
                              DumpCompressionFlags compression_flag) {
        std::filesystem::path out_dir = m_PathPrefix / FlagToBasename.at(flag);
        if (std::filesystem::exists(out_dir)) {
            std::cout << "Error out dir (" << out_dir.generic_string() << ")" << " already exists!" << std::endl;
            return false;
        }
        std::filesystem::create_directory(out_dir); //can err
        std::cout << "Dumping to " << out_dir.generic_string() << "..." << std::endl;

        const TexturePackIndex& tpi = m_TexturePackIndexMap.at(flag);
        for (const auto& entry : tpi) {
            if (std::holds_alternative<TexturePackResource>(entry.second)) {
                const auto& res = std::get<TexturePackResource>(entry.second);

                std::filesystem::path file_path_png = out_dir / (res.Name + ".png");
                std::filesystem::path file_path_dds = out_dir / (res.Name + ".dds");

                if (res.Format != ResourceFormat::A8R8G8B8) {
                    auto bytestream = BuildDDSHeaderForResource(res);

                    TexturePackType type = GetType(m_TexturePackFileMap.at(flag));
                    LPVOID lpMap = NULL;
                    bool allgood = true;
                    if (type == TexturePackType::PERM) {
                        lpMap = GetPerm(m_TexturePackFileMap.at(flag)).GetMapView();
                    }
                    else if (type == TexturePackType::TEMP) {
                        lpMap = GetTemp(m_TexturePackFileMap.at(flag)).GetMapView();
                    }
                    else {
                        std::cerr << "Invalid TexturePackType" << std::endl;
                        allgood = false;
                    }
                    if (allgood) {
                        char* fptr = static_cast<char*>(lpMap);
                        char* start = fptr + res.Offset;

                        if ((format_flag == DumpFormatFlags::Native) || (!m_ClientHasWGLContext)) {
                          FILE *pFile = nullptr;
                          fopen_s(&pFile, file_path_dds.generic_string().c_str(), "wb");

                          if (!pFile)
                            continue;

                          fwrite((const char *)bytestream.str().c_str(), 1, 128, pFile);
                          fwrite((const char *)start, 1, res.Size, pFile);
                          fclose(pFile);
                        }
                        else {
                            HDC hdc = wglGetCurrentDC();
                            HGLRC hrc = wglGetCurrentContext();

                            wglMakeCurrent(m_hDC, m_hRC);

                            GLuint tex;
                            glGenTextures(1, &tex);
                            glBindTexture(GL_TEXTURE_2D, tex);
                            uint16_t decompress_from = 0;
                            if (res.Format == ResourceFormat::DXT1) {
                                decompress_from = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                            }
                            else if (res.Format == ResourceFormat::DXT3) {
                                decompress_from = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                            }
                            else if (res.Format == ResourceFormat::DXT5) {
                                decompress_from = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                            }
                            else {
                                // Handle unsupported formats
                            }

                            glCompressedTexImage2D(GL_TEXTURE_2D, 0, decompress_from,
                                res.Width, res.Height, 0, res.Size, start);

                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                            glGenerateMipmap(GL_TEXTURE_2D);

                            // Create a new texture to store the decompressed image
                            GLuint rgbTex;
                            glGenTextures(1, &rgbTex);
                            glBindTexture(GL_TEXTURE_2D, rgbTex);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, res.Width, res.Height, 0, GL_RGBA,
                                GL_UNSIGNED_BYTE, NULL);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                            // Create a framebuffer object (FBO) and attach the decompressed texture to it
                            GLuint fbo;
                            glGenFramebuffers(1, &fbo);
                            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                rgbTex, 0);

                            // Check FBO status
                            GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                            if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
                                // Handle error
                            }
                            glViewport(0, 0, res.Width, res.Height);
                            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                            glClear(GL_COLOR_BUFFER_BIT);
                            // Render a full-screen quad with the compressed texture bound here
                            // The decompressed image will be rendered into rgbTex
                            GLint texLocation = glGetUniformLocation(m_OutputProgram, "tex");

                            // Bind your program
                            glUseProgram(m_OutputProgram);

                            // Bind your texture to texture unit 0
                            glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, tex);

                            // Set the 'tex' uniform to texture unit 0
                            glUniform1i(texLocation, 0);

                            // Render the fullscreen quad
                            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                            // Bind the FBO and read the pixels from the decompressed texture
                            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                            std::vector<char> pixels(res.Width * res.Height * 4);
                            glReadPixels(0, 0, res.Width, res.Height, GL_RGBA, GL_UNSIGNED_BYTE,
                                pixels.data());

                            // Unbind and delete resources
                            glBindFramebuffer(GL_FRAMEBUFFER, 0);
                            glBindTexture(GL_TEXTURE_2D, 0);
                            glDeleteTextures(1, &rgbTex);
                            glDeleteTextures(1, &tex);
                            glDeleteFramebuffers(1, &fbo);

                            fpng::fpng_encode_image_to_file(file_path_png.generic_string().c_str(),
                                pixels.data(), res.Width, res.Height, 4,
                                fpng::FPNG_ENCODE_SLOWER);

                            wglMakeCurrent(hdc, hrc);
                        }
                    }

                }
                else {
                    TexturePackType type = GetType(m_TexturePackFileMap.at(flag));
                    LPVOID lpMap = NULL;
                    bool allgood = true;
                    if (type == TexturePackType::PERM) {
                        lpMap = GetPerm(m_TexturePackFileMap.at(flag)).GetMapView();
                    }
                    else if (type == TexturePackType::TEMP) {
                        lpMap = GetTemp(m_TexturePackFileMap.at(flag)).GetMapView();
                    }
                    else {
                        std::cerr << "Invalid TexturePackType" << std::endl;
                        allgood = false;
                    }
                    if (allgood) {
                        char* fptr = static_cast<char*>(lpMap);
                        char* start = fptr + res.Offset;
                        fpng::fpng_encode_image_to_file(file_path_png.generic_string().c_str(), start,
                            res.Width, res.Height, 4, static_cast<uint32_t>(compression_flag));
                    }
                }
            }
        }
        return true;
    }
    bool ctx::DeleteTexturePackDump(TexturePackFlags flag) {
        std::filesystem::path out_dir = m_PathPrefix / FlagToBasename.at(flag);
        if (!std::filesystem::exists(out_dir)) {
            std::cout << "Error out dir (" << out_dir.generic_string() << ")"
                      << " does not exist!" << std::endl;
            return false;
        } else {
            std::filesystem::remove_all(out_dir);
            std::cout << "Removed out dir (" << out_dir.generic_string() << ")"
                      << "!" << std::endl;
            return true;
        }
    }
    bool ctx::InjectAndWritePack(std::string src, std::string dst, TexturePackFlags flag) {
        TexturePackResource _src = GetMappedResource(src);
        TexturePackResource _dst = GetMappedResource(dst);
        
        if (!(_dst.Fits(_src))) {
            return false;
        } else {
            // Get src data
            char *start = _src.Origin + _src.Offset;
            std::vector<uint8_t> buffer(_src.Size);
            std::memcpy(buffer.data(), start, _src.Size);

            const auto &fm = m_TexturePackFileMap.at(flag);
            size_t size = 0;
            if (_dst.PackType == TexturePackType::PERM) {
                size = GetFileSize(GetPerm(fm).GetFile(), NULL);
            } else {
                size = GetFileSize(GetTemp(fm).GetFile(), NULL);
            }
            // copy dst data
            std::vector<uint8_t> new_bin_data(size);
            std::memcpy(new_bin_data.data(), _dst.Origin, size);

            //overwrite dst with src
            char *dst_start = reinterpret_cast<char*>(new_bin_data.data()) + _dst.Offset;
            std::memcpy(dst_start, buffer.data(), _src.Size);

            // TODO: wrangle the tp name PS: ugh yur lib suxxxx!!!!!!!!!
            // Write data to disk
            std::ofstream new_bin(GetTexturePackFilenameForResource(_dst), std::ios::binary);
            new_bin.write((const char *)new_bin_data.data(), size);
            new_bin.close();
        }
        return true;
    }
    TexturePackResource ctx::GetMappedResource(std::string res_name) {
        for (uint8_t i = 0; i < TexturePackCount; ++i) {
            TexturePackFlags flag = (TexturePackFlags)(1 << i);
            auto index = m_TexturePackIndexMap.find(flag);
            if (index != m_TexturePackIndexMap.end()) {
                auto it = index->second.find(res_name);
                if (it != index->second.end()) {
                    return std::get<TexturePackResource>(it->second);
                }
            }
        }
        return TexturePackResource();
    }
    std::string ctx::GetTexturePackFilenameForResource(TexturePackResource res) {
        for (uint8_t i = 0; i < TexturePackCount; ++i) {
            TexturePackFlags flag = (TexturePackFlags)(1 << i);
            auto index = m_TexturePackIndexMap.find(flag);
            if (index != m_TexturePackIndexMap.end()) {
                auto it = index->second.find(res.Name);
                if (it != index->second.end()) {
                    return FlagToBasename.at(flag) + PackTypeToExt.at(res.PackType);
                }
            }
        }
        return std::string();
    }
    std::vector<TexturePackResource> ctx::GetResourcesFromTexturePack(TexturePackFlags flag) const {
        if (!(IsTexturePackMapped(flag) && IsTexturePackIndexed(flag)))
            return std::vector<TexturePackResource>();
        else {
            std::vector<TexturePackResource> resources;
            const auto &index = m_TexturePackIndexMap.at(flag);
            for (const auto &entry : index) {
                if (std::holds_alternative<TexturePackResource>(entry.second)) {
                    resources.push_back(std::get<TexturePackResource>(entry.second));
                }
            }
            return resources;
        }
    }
#pragma warning(pop)
    bool TexturePackResource::Fits(const TexturePackResource &other) {
        return (Format == other.Format) && (Width == other.Width) &&
               (Height == other.Height) && (Size == other.Size);
    }
    } // namespace mcctp
