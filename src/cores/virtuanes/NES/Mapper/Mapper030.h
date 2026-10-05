//////////////////////////////////////////////////////////////////////////
// Mapper030  UNROM 512                                                  //
//////////////////////////////////////////////////////////////////////////
class	Mapper030 : public Mapper
{
public:
	Mapper030( NES* parent ) : Mapper(parent) { original = NULL; dirty = 0; }
	~Mapper030();

	void	Reset();
	void	Write( WORD addr, BYTE data );

	void	LoadBattery( LPCSTR path );
	void	SaveBattery( LPCSTR path );

	// For state save
	BOOL	IsStateSave() { return TRUE; }
	void	SaveState( LPBYTE p );
	void	LoadState( LPBYTE p );

protected:
	BYTE	latch;
	BYTE	flash;		// PRG ROM is a self-programmable flash chip
	BYTE	command;	// position in a flash command sequence
	BYTE	idmode;		// flash software ID mode
	BYTE	dirty;		// flash changed since the last save
	LPBYTE	original;	// the PRG ROM as loaded, for saving changes only
	BYTE	idpage[0x4000];

private:
	void	SetBank();
	void	SetCHR_1K_Bank( BYTE page, INT bank );
	void	WriteFlash( WORD addr, BYTE data );
	INT	PRGSize() { return nes->rom->GetPROM_SIZE() * 0x4000; }
};
