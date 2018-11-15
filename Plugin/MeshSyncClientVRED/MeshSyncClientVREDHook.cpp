﻿#include "pch.h"
#include "msvrContext.h"


#pragma warning(push)
#pragma warning(disable:4229)
static void(*WINAPI _glActiveTexture)(GLenum texture);
static void(*WINAPI _glBindTexture)(GLenum target, GLuint texture);
static void(*WINAPI _glGenBuffers)(GLsizei n, GLuint* buffers);
static void(*WINAPI _glDeleteBuffers) (GLsizei n, const GLuint* buffers);
static void(*WINAPI _glBindBuffer) (GLenum target, GLuint buffer);
static void(*WINAPI _glBufferData) (GLenum target, GLsizeiptr size, const void* data, GLenum usage);
static void* (*WINAPI _glMapBuffer) (GLenum target, GLenum access);
static GLboolean(*WINAPI _glUnmapBuffer) (GLenum target);
static void(*WINAPI _glVertexAttribPointer) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
static void(*WINAPI _glUniform4fv) (GLint location, GLsizei count, const GLfloat* value);
static void(*WINAPI _glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
static void(*WINAPI _glDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid * indices);
static void(*WINAPI _glFlush)(void);
static void* (*WINAPI _wglGetProcAddress)(const char* name);
#pragma warning(pop)


static void WINAPI glActiveTexture_hook(GLenum texture)
{
    msvrGetContext()->onActiveTexture(texture);
    _glActiveTexture(texture);
}
static void WINAPI glBindTexture_hook(GLenum target, GLuint texture)
{
    msvrGetContext()->onBindTexture(target, texture);
    _glBindTexture(target, texture);
}

static void WINAPI glGenBuffers_hook(GLsizei n, GLuint* buffers)
{
    _glGenBuffers(n, buffers);
    msvrGetContext()->onGenBuffers(n, buffers);
}
static void WINAPI glDeleteBuffers_hook(GLsizei n, const GLuint* buffers)
{
    msvrGetContext()->onDeleteBuffers(n, buffers);
    _glDeleteBuffers(n, buffers);
}
static void WINAPI glBindBuffer_hook(GLenum target, GLuint buffer)
{
    msvrGetContext()->onBindBuffer(target, buffer);
    _glBindBuffer(target, buffer);
}
static void WINAPI glBufferData_hook(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
    msvrGetContext()->onBufferData(target, size, data, usage);
    _glBufferData(target, size, data, usage);
}
static void* WINAPI glMapBuffer_hook(GLenum target, GLenum access)
{
    auto ret = _glMapBuffer(target, access);
    msvrGetContext()->onMapBuffer(target, access, ret);
    return ret;
}
static GLboolean WINAPI glUnmapBuffer_hook(GLenum target)
{
    msvrGetContext()->onUnmapBuffer(target);
    return _glUnmapBuffer(target);
}

static void WINAPI glVertexAttribPointer_hook(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer)
{
    msvrGetContext()->onVertexAttribPointer(index, size, type, normalized, stride, pointer);
    _glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

static void WINAPI glUniform4fv_hook(GLint location, GLsizei count, const GLfloat* value)
{
    msvrGetContext()->onUniform4fv(location, count, value);
    _glUniform4fv(location, count, value);
}
static void WINAPI glUniformMatrix4fv_hook(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    msvrGetContext()->onUniformMatrix4fv(location, count, transpose, value);
    _glUniformMatrix4fv(location, count, transpose, value);
}

static void WINAPI glDrawElements_hook(GLenum mode, GLsizei count, GLenum type, const GLvoid * indices)
{
    msvrGetContext()->onDrawElements(mode, count, type, indices);
    _glDrawElements(mode, count, type, indices);
}
static void WINAPI glFlush_hook(void)
{
    msvrGetContext()->onFlush();
    _glFlush();
}

static void* WINAPI wglGetProcAddress_hook(const char* name)
{
    static struct HookData
    {
        const char *name;
        void **func_hook;
        void **func_orig;
    }
    s_hooks[] = {
#define Hook(Name) {#Name, (void**)&Name##_hook, (void**)&_##Name}
    Hook(glActiveTexture),
    Hook(glBindTexture),
    Hook(glGenBuffers),
    Hook(glDeleteBuffers),
    Hook(glBindBuffer),
    Hook(glBufferData),
    Hook(glMapBuffer),
    Hook(glUnmapBuffer),
    Hook(glVertexAttribPointer),
    Hook(glUniform4fv),
    Hook(glUniformMatrix4fv),
    Hook(glDrawElements),
    Hook(glFlush),
#undef Hook
    };

    for (auto& hook : s_hooks) {
        if (strcmp(hook.name, name) == 0) {
            auto sym = _wglGetProcAddress(name);
            if (sym) {
                *hook.func_orig = sym;
                return hook.func_hook;
            }
        }
    }
    return _wglGetProcAddress(name);
}



template<class T>
static inline void ForceWrite(T &dst, const T &src)
{
    DWORD old_flag;
    ::VirtualProtect(&dst, sizeof(T), PAGE_EXECUTE_READWRITE, &old_flag);
    dst = src;
    ::VirtualProtect(&dst, sizeof(T), old_flag, &old_flag);
}

static void* AllocateExecutableMemoryForward(size_t size, void *location)
{
    static size_t base = (size_t)location;

    void *ret = nullptr;
    const size_t step = 0x10000; // 64kb
    for (size_t i = 0; ret == nullptr; ++i) {
        // increment address until VirtualAlloc() succeed
        // (MSDN says "If the memory is already reserved and is being committed, the address is rounded down to the next page boundary".
        //  https://msdn.microsoft.com/en-us/library/windows/desktop/aa366887.aspx
        //  but it seems VirtualAlloc() return nullptr if memory is already reserved and is being committed)
        ret = ::VirtualAlloc((void*)((size_t)base + (step*i)), size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    return ret;
}

static void* EmitJmpInstruction(void* from_, void* to_)
{
    BYTE *from = (BYTE*)from_;
    BYTE *to = (BYTE*)to_;
    BYTE *jump_from = from + 5;
    size_t distance = jump_from > to ? jump_from - to : to - jump_from;
    if (distance <= 0x7fff0000) {
        // 0xe9 [RVA]
        *(from++) = 0xe9;
        *(((DWORD*&)from)++) = (DWORD)(to - jump_from);
    }
    else {
        // 0xff 0x25 [RVA] [TargetAddr]
        *(from++) = 0xff;
        *(from++) = 0x25;
#ifdef _M_IX86
        *(((DWORD*&)from)++) = (DWORD)(from + 4);
#elif defined(_M_X64)
        *(((DWORD*&)from)++) = (DWORD)0;
#endif
        *(((DWORD_PTR*&)from)++) = (DWORD_PTR)(to);
    }
    return from;
}

static void* OverrideDLLExport(HMODULE module, const char *funcname, void *replacement, void *&jump_table)
{
    if (!module) { return nullptr; }

    size_t ImageBase = (size_t)module;
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)ImageBase;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) { return nullptr; }

    PIMAGE_NT_HEADERS pNTHeader = (PIMAGE_NT_HEADERS)(ImageBase + pDosHeader->e_lfanew);
    DWORD RVAExports = pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (RVAExports == 0) { return nullptr; }

    IMAGE_EXPORT_DIRECTORY *pExportDirectory = (IMAGE_EXPORT_DIRECTORY *)(ImageBase + RVAExports);
    DWORD *RVANames = (DWORD*)(ImageBase + pExportDirectory->AddressOfNames);
    WORD *RVANameOrdinals = (WORD*)(ImageBase + pExportDirectory->AddressOfNameOrdinals);
    DWORD *RVAFunctions = (DWORD*)(ImageBase + pExportDirectory->AddressOfFunctions);
    for (DWORD i = 0; i < pExportDirectory->NumberOfFunctions; ++i) {
        char *pName = (char*)(ImageBase + RVANames[i]);
        if (strcmp(pName, funcname) == 0) {
            void *before = (void*)(ImageBase + RVAFunctions[RVANameOrdinals[i]]);
            DWORD RVAJumpTable = (DWORD)((size_t)jump_table - ImageBase);
            ForceWrite<DWORD>(RVAFunctions[RVANameOrdinals[i]], RVAJumpTable);
            jump_table = EmitJmpInstruction(jump_table, replacement);
            return before;
        }
    }
    return nullptr;
}



BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        // setup hooks
        auto opengl32 = ::LoadLibraryA("opengl32.dll");
        auto jumptable = AllocateExecutableMemoryForward(1024, opengl32);

#define Override(Name) (void*&)_##Name = OverrideDLLExport(opengl32, #Name, Name##_hook, jumptable);
        Override(wglGetProcAddress);
        Override(glDrawElements);
        Override(glFlush);
#undef Override
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
    }
    return TRUE;
}
