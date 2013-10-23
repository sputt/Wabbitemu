#include "stdafx.h"

#include "spasm.h"
#include "storage.h"
#include "errors.h"
#include "parser.h"
#include "CTextStream.h"
#include "CIncludeDirectoryCollection.h"

class ATL_NO_VTABLE CZ80Assembler :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CZ80Assembler, &__uuidof(Z80Assembler)>,
	public IDispatchImpl<IZ80Assembler, &__uuidof(IZ80Assembler), &LIBID_SPASM, 1, 2>
{
public:
	DECLARE_REGISTRY_RESOURCEID(IDR_Z80ASSEMBLER)

	BEGIN_COM_MAP(CZ80Assembler)
		COM_INTERFACE_ENTRY(IZ80Assembler)
		COM_INTERFACE_ENTRY(IDispatch)
	END_COM_MAP()

	HRESULT FinalConstruct()
	{
		HRESULT hr = m_dict.CreateInstance(__uuidof(Dictionary));

		CComObject<CTextStream> *pObj = NULL;
		CComObject<CTextStream>::CreateInstance(&pObj);
		pObj->AddRef();
		m_pStdOut = pObj;
		pObj->Release();

		CComObject<CIncludeDirectoryCollection> *pDirObj = NULL;
		CComObject<CIncludeDirectoryCollection>::CreateInstance(&pDirObj);
		pDirObj->AddRef();
		m_pDirectories = pDirObj;
		pDirObj->Release();

		m_fFirstAssembly = TRUE;
		return hr;
	}

	STDMETHOD(get_Defines)(IDictionary **ppDictionary)
	{
		return m_dict->QueryInterface(ppDictionary);
	}

	STDMETHOD(get_StdOut)(ITextStream **ppStream)
	{
		return m_pStdOut->QueryInterface(ppStream);
	}

	STDMETHOD(get_InputFile)(LPBSTR lpbstrInputFile)
	{
		*lpbstrInputFile = SysAllocString(m_bstrInputFile);
		return S_OK;
	}

	STDMETHOD(put_InputFile)(BSTR bstrInputFile)
	{
		m_bstrInputFile = bstrInputFile;
		return S_OK;
	}

	STDMETHODIMP put_OutputFile(BSTR bstrOutputFile)
	{
		m_bstrOutputFile = bstrOutputFile;
		return S_OK;
	}

	STDMETHODIMP get_OutputFile(LPBSTR lpbstrOutputFile)
	{
		*lpbstrOutputFile = SysAllocString(m_bstrOutputFile);
		return S_OK;
	}

	STDMETHODIMP put_CurrentDirectory(BSTR bstrDirectory)
	{
		SetCurrentDirectory(_bstr_t(bstrDirectory));
		return S_OK;
	}

	STDMETHODIMP get_CurrentDirectory(LPBSTR lpbstrDirectory)
	{
		TCHAR szBuffer[MAX_PATH];
		GetCurrentDirectory(ARRAYSIZE(szBuffer), szBuffer);
		*lpbstrDirectory = SysAllocString(_bstr_t(szBuffer));
		return S_OK;
	}

	STDMETHOD(get_Options)(LPDWORD lpdwOptions)
	{
		*lpdwOptions = m_dwOptions;
		return S_OK;
	}

	STDMETHOD(put_Options)(DWORD dwOptions)
	{
		m_dwOptions = dwOptions;
		return S_OK;
	}

	STDMETHOD(get_CaseSensitive)(VARIANT_BOOL *lpCaseSensitive)
	{
		*lpCaseSensitive = get_case_sensitive() ? VARIANT_TRUE : VARIANT_FALSE;
		return S_OK;
	}

	STDMETHOD(put_CaseSensitive)(VARIANT_BOOL caseSensitive)
	{
		set_case_sensitive(caseSensitive == VARIANT_TRUE ? TRUE : FALSE);
		return S_OK;
	}

	STDMETHOD(get_IncludeDirectories)(IIncludeDirectoryCollection **ppDirectories)
	{
		return m_pDirectories->QueryInterface(ppDirectories);
	}

	STDMETHOD(Assemble)(VARIANT varInput, IStream **ppOutput)
	{
		if (!m_fFirstAssembly)
		{
			free_storage();
		}

		m_fFirstAssembly = FALSE;

		init_storage();
	
		HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, output_buf_size); 
		output_contents = (unsigned char *) GlobalLock(hGlobal);

		mode = m_dwOptions;

		if (V_VT(&varInput) == VT_BSTR)
		{
			mode |= MODE_NORMAL | MODE_COMMANDLINE;

			CW2CT szInput(V_BSTR(&varInput));
			input_contents = strdup(szInput);
		}
		else
		{
			mode &= ~MODE_COMMANDLINE;
			mode |= MODE_NORMAL;

			if (curr_input_file) {
				free(curr_input_file);
			}
			curr_input_file = strdup(m_bstrInputFile);
			if (output_filename) {
				free(output_filename);
			}
			output_filename = strdup(m_bstrOutputFile);
		}

		// Set up the include directories
		CComPtr<IUnknown> pEnumUnk;
		HRESULT hr = m_pDirectories->get__NewEnum(&pEnumUnk);

		CComQIPtr<IEnumVARIANT> pEnum = pEnumUnk;

		CComVariant varItem;
		ULONG ulFetched;

		while (pEnum->Next(1, &varItem, &ulFetched) == S_OK)
		{
			include_dirs = list_prepend(include_dirs, (char *) strdup(_bstr_t(V_BSTR(&varItem))));
		}

		int result = run_assembly();

		list_free(include_dirs, true, NULL);
		include_dirs = NULL;

		ClearSPASMErrorSessions();

		GlobalUnlock(hGlobal);

		CComPtr<IStream> pStream;
		hr = CreateStreamOnHGlobal(hGlobal, TRUE, &pStream);
		ULARGE_INTEGER ul;
		ul.QuadPart = out_ptr - output_contents;
		pStream->SetSize(ul);

		return pStream->QueryInterface(ppOutput);
	}

	STDMETHOD(Parse)(BSTR bstrInput, LPBSTR lpbstrOutput)
	{
		return E_NOTIMPL;
	}

private:
	BOOL m_fFirstAssembly;
	DWORD m_dwOptions;
	IDictionaryPtr m_dict;
	CComPtr<ITextStream> m_pStdOut;
	CComPtr<IIncludeDirectoryCollection> m_pDirectories;
	_bstr_t m_bstrInputFile;
	_bstr_t m_bstrOutputFile;
};

OBJECT_ENTRY_AUTO(__uuidof(Z80Assembler), CZ80Assembler)
