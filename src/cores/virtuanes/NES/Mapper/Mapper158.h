//////////////////////////////////////////////////////////////////////////
// Mapper158  Tengen 800037 (RAMBO-1 with CHR-controlled nametables)     //
//////////////////////////////////////////////////////////////////////////
class	Mapper158 : public Mapper064
{
public:
	Mapper158( NES* parent ) : Mapper064(parent) {}

	void	Reset();
	void	Write(WORD addr, BYTE data);

	void	SaveState( LPBYTE p );
	void	LoadState( LPBYTE p );

protected:
	BYTE	ntsel[8];	// bit 7 of the CHR register mapped at each 1 KiB page
private:
	void	SetNametables();
};
