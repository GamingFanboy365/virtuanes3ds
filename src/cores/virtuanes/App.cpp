//
// �A�v���P�[�V�����T�|�[�g�N���X
//
#include "VirtuaNESres.h"
#include "App.h"

/*
// _tWinMain�����̈������̂܂�
HINSTANCE CApp::m_hInstance = NULL;
HINSTANCE CApp::m_hPrevInstance = NULL;
LPTSTR	CApp::m_lpCmdLine = NULL;
INT	CApp::m_nCmdShow = 0;

// ���ꃊ�\�[�X�v���O�C���̃C���X�^���X�n���h��
HINSTANCE CApp::m_hPlugin = NULL;
// ���C���E�C���h�E�̃E�C���h�E�n���h��
HWND	CApp::m_hWnd = NULL;
// ���C���E�C���h�E�̃��j���[�n���h��
HMENU	CApp::m_hMenu = NULL;

// �v���O�����̃p�X(�N�����̃p�X)
CHAR	CApp::m_szModulePath[_MAX_PATH];

*/
// �G���[�X�g�����O(�e���|����)
CHAR	szErrorString[256];

// �G���[�����p
INT	CApp::m_ErrorStringTableID[] = {
	IDS_ERROR,
	IDS_ERROR_STARTUP,
	IDS_ERROR_UNKNOWN,
	IDS_ERROR_OPEN,
	IDS_ERROR_READ,
	IDS_ERROR_WRITE,
	IDS_ERROR_OUTOFMEMORY,

	IDS_ERROR_ILLEGALOPCODE,
	IDS_ERROR_UNSUPPORTFORMAT,
	IDS_ERROR_INVALIDNESHEADER,
	IDS_ERROR_SMALLFILE,
	IDS_ERROR_UNSUPPORTMAPPER,
	IDS_ERROR_NODISKBIOS,
	IDS_ERROR_UNSUPPORTDISK,
	IDS_ERROR_ILLEGALDISKSIZE,

	IDS_ERROR_ILLEGALMAPPERNO,
	IDS_ERROR_ILLEGALHEADER,

	IDS_ERROR_ILLEGALSTATECRC,
	IDS_ERROR_ILLEGALMOVIEOLD,
	IDS_ERROR_ILLEGALMOVIEVER,
	IDS_ERROR_ILLEGALMOVIECRC,
	IDS_ERROR_ILLEGALMOVIEOLD_A,
	IDS_ERROR_ILLEGALMOVIEVER_A,
	IDS_ERROR_ILLEGALMOVIECRC_A,

	IDS_ERROR_NETWORKDISCONNECT,
	IDS_ERROR_NETWORKERROR,
	0,
};

CHAR	CApp::m_ErrorString[ERRORSTRING_MAX][256];

void	CApp::LoadErrorString()
{
	/*if( !m_hPlugin )
		return;

	for( INT i = 0; m_ErrorStringTableID[i]; i++ ) {
		LoadString( m_ErrorStringTableID[i], m_ErrorString[i], sizeof(m_ErrorString[i]) );
	}*/
}

// The Windows version loads these from its resources. Some are used as
// format strings: IDS_ERROR_OPEN takes the file name (%s), and
// IDS_ERROR_UNSUPPORTMAPPER the mapper number (%d).
CHAR*	CApp::GetErrorString( INT nID )
{
	switch( nID ) {
		case IDS_ERROR_OPEN:			return	"Can't open %s";
		case IDS_ERROR_READ:			return	"Can't read the file.";
		case IDS_ERROR_WRITE:			return	"Can't write the file.";
		case IDS_ERROR_OUTOFMEMORY:		return	"Out of memory.";
		case IDS_ERROR_UNSUPPORTFORMAT:	return	"Unknown file format.";
		case IDS_ERROR_INVALIDNESHEADER:	return	"Invalid NES header: the ROM is smaller than its header says.";
		case IDS_ERROR_SMALLFILE:		return	"The file is too small.";
		case IDS_ERROR_UNSUPPORTMAPPER:	return	"Mapper %d isn't supported.";
		case IDS_ERROR_NODISKBIOS:		return	"FDS BIOS not found. Put it in /3ds/virtuanes_3ds/bios/disksys.rom";
		case IDS_ERROR_UNSUPPORTDISK:	return	"Disks with more than 8 sides aren't supported.";
		case IDS_ERROR_ILLEGALDISKSIZE:	return	"The FDS image is smaller than its number of disk sides needs (65500 bytes each).";
		case IDS_ERROR_ILLEGALHEADER:	return	"Invalid header.";
		case IDS_ERROR_UNKNOWN:			return	"Unknown error.";
	}
	return	"Error.";	// NULL �X�g�����O�Ƃ��ēn��
}

