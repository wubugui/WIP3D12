#pragma once
#include "Common.h"
#include "Framework.h"
//#include "d3dcompiler.h"
#include <dxcapi.h>
#include <comdef.h>
#include <map>
#include <initializer_list>
#include <string>
#include <iostream>

struct ISlangBlob;

namespace WIP3D
{
    /** Minimal smart pointer for working with COM objects.
    */
    template<typename T>
    struct ComPtr
    {
    public:
        /// Type of the smart pointer itself
        typedef ComPtr ThisType;

        /// Initialize to a null pointer.
        ComPtr() : mpObject(nullptr) {}
        ComPtr(nullptr_t) : mpObject(nullptr) {}

        /// Release reference to the pointed-to object.
        ~ComPtr() { if (mpObject) (mpObject)->Release(); }

        /// Add a new reference to an existing object.
        explicit ComPtr(T* pObject) : mpObject(pObject) { if (pObject) (pObject)->AddRef(); }

        /// Add a new reference to an existing object.
        ComPtr(const ThisType& rhs) : mpObject(rhs.mpObject) { if (mpObject) (mpObject)->AddRef(); }

        /// Add a new reference to an existing object
        T* operator=(T* in)
        {
            if (in) in->AddRef();
            if (mpObject) mpObject->Release();
            mpObject = in;
            return in;
        }

        /// Add a new reference to an existing object
        const ThisType& operator=(const ThisType& rhs)
        {
            if (rhs.mpObject) rhs.mpObject->AddRef();
            if (mpObject) mpObject->Release();
            mpObject = rhs.mpObject;
            return *this;
        }

        /// Transfer ownership of a reference.
        ComPtr(ThisType&& rhs) : mpObject(rhs.mpObject) { rhs.mpObject = nullptr; }

        /// Transfer ownership of a reference.
        ComPtr& operator=(ThisType&& rhs) { T* swap = mpObject; mpObject = rhs.mpObject; rhs.mpObject = swap; return *this; }

        /// Clear out object pointer.
        void setNull()
        {
            if (mpObject)
            {
                mpObject->Release();
                mpObject = nullptr;
            }
        }

        /// Swap pointers with another reference.
        void swap(ThisType& rhs)
        {
            T* tmp = mpObject;
            mpObject = rhs.mpObject;
            rhs.mpObject = tmp;
        }

        /// Get the underlying object pointer.
        T* get() const { return mpObject; }

        /// Cast to object pointer type.
        operator T* () const { return mpObject; }

        /// Access members of underlying object.
        T* operator->() const { return mpObject; }

        /// Dereference underlying pointer.
        T& operator*() { return *mpObject; }

        /// Transfer ownership of reference to the caller.
        T* detach() { T* ptr = mpObject; mpObject = nullptr; return ptr; }

        /// Transfer ownership of reference from the caller.
        void attach(T* in) { T* old = mpObject; mpObject = in; if (old) old->Release(); }

        /// Get a writable reference suitable for use as a function output argument.
        T** writeRef() { setNull(); return &mpObject; }

        /// Get a readable reference suitable for use as a function input argument.
        T* const* readRef() const { return &mpObject; }

    protected:
        // Disabled: do not take the address of a smart pointer.
        T** operator&();

        /// The underlying raw object pointer
        T* mpObject;
    };

    void test()
    {
        IDxcLibrary* library;
        HRESULT hr = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
        //if(FAILED(hr)) Handle error...

        IDxcCompiler* compiler;
        hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
        //if(FAILED(hr)) Handle error...

        uint32_t codePage = CP_UTF8;
        IDxcBlobEncoding* sourceBlob;
        hr = library->CreateBlobFromFile(L"PS.hlsl", &codePage, &sourceBlob);
        //if(FAILED(hr)) Handle file loading error...

        IDxcOperationResult* result;
        hr = compiler->Compile(
            sourceBlob, // pSource
            L"PS.hlsl", // pSourceName
            L"main", // pEntryPoint
            L"PS_6_0", // pTargetProfile
            NULL, 0, // pArguments, argCount
            NULL, 0, // pDefines, defineCount
            NULL, // pIncludeHandler
            &result); // ppResult
        if (SUCCEEDED(hr))
            result->GetStatus(&hr);
        if (FAILED(hr))
        {
            if (result)
            {
                IDxcBlobEncoding* errorsBlob;
                hr = result->GetErrorBuffer(&errorsBlob);
                if (SUCCEEDED(hr) && errorsBlob)
                {
                    wprintf(L"Compilation failed with errors:\n%hs\n",
                        (const char*)errorsBlob->GetBufferPointer());
                    errorsBlob->Release();
                }
            }
            // Handle compilation error...
        }
        IDxcBlob* code;
        result->GetResult(&code);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        // (...)
        psoDesc.PS.BytecodeLength = code->GetBufferSize();
        psoDesc.PS.pShaderBytecode = code->GetBufferPointer();
        ComPtr<ID3D12PipelineState> pso;
        //hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));

        code->Release();
        code = nullptr;
        result->Release();
        result = nullptr;
        library->Release();
        compiler->Release();
    }

    class D3DShaderCompiler
    {
    private:
        static void show_error_message(ID3D10Blob* error, const char* filename)
        {
            char* e = (char*)(error->GetBufferPointer());
            unsigned long bs = error->GetBufferSize();
            std::ofstream fout;

            std::cout << e;

            fout.open("error.txt");


            fout << filename << " compile error : " << std::endl;
            for (unsigned long i = 0; i < bs; ++i)
            {
                fout << e[i];
            }

            fout << std::endl << "==================" << std::endl;

            error->Release();
            error = 0;

        }

    public:

        static bool compile(const wchar_t* name, const wchar_t* entry_point, const wchar_t* target_profile)
        {
            uint32_t codePage = CP_UTF8;
            IDxcBlobEncoding* sourceBlob = nullptr;
            HRESULT hr = library->CreateBlobFromFile(name, &codePage, &sourceBlob);
            if (FAILED(hr))
            {
                if (sourceBlob)
                {
                    sourceBlob->Release();
                    sourceBlob = nullptr;
                }
                _bstr_t nam_(name);
                g_logger->debug_print(WIP_ERROR, "cannt open shader file %s .", nam_);
                return false;
            }

            IDxcOperationResult* result;
            hr = compiler->Compile(
                sourceBlob, // pSource
                name, // pSourceName
                entry_point, // pEntryPoint
                target_profile, // pTargetProfile
                NULL, 0, // pArguments, argCount
                NULL, 0, // pDefines, defineCount
                NULL, // pIncludeHandler
                &result); // ppResult
            if (SUCCEEDED(hr))
                result->GetStatus(&hr);
            else
            {
                return false;
            }
            if (FAILED(hr))
            {
                if (result)
                {
                    IDxcBlobEncoding* errorsBlob;
                    hr = result->GetErrorBuffer(&errorsBlob);
                    if (SUCCEEDED(hr) && errorsBlob)
                    {
                        wprintf(L"Compilation failed with errors:\n%hs\n",
                            (const char*)errorsBlob->GetBufferPointer());
                        errorsBlob->Release();
                        return false;
                    }
                }
                // Handle compilation error...
            }
            IDxcBlob* code;
            result->GetResult(&code);

            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            // (...)
            psoDesc.PS.BytecodeLength = code->GetBufferSize();
            psoDesc.PS.pShaderBytecode = code->GetBufferPointer();
            ComPtr<ID3D12PipelineState> pso;
            //hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));

            code->Release();
            code = nullptr;
            result->Release();
            result = nullptr;
        }

        static bool load_compiler()
        {
            library = nullptr;
            compiler = nullptr;
            HRESULT hr = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
            if (FAILED(hr))
            {
                g_logger->debug_print(WIP_ERROR, "Can't create dxc library instance.");
                return false;
            }
            HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
            if (FAILED(hr))
            {
                g_logger->debug_print(WIP_ERROR, "Can't create dxc compiler instance.");
                library->Release();
                return false;
            }
            return true;
        }

        static void unload()
        {
            if (library)
            {
                library->Release();
                library = nullptr;
            }
            if (compiler)
            {
                compiler->Release();
                compiler = nullptr;
            }
        }

        static IDxcLibrary* library;
        static IDxcCompiler* compiler;
    };
}
