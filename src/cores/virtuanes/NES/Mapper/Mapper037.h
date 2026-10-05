//////////////////////////////////////////////////////////////////////////
// Mapper037  Nintendo PAL-ZZ (Super Mario Bros./Tetris/Nintendo World Cup)
//////////////////////////////////////////////////////////////////////////
class	Mapper037 : public Mapper004
{
public:
	Mapper037( NES* parent ) : Mapper004(parent) {}

	void	Reset();
	void	WriteLow( WORD addr, BYTE data );

	void	SaveState( LPBYTE p );
	void	LoadState( LPBYTE p );

protected:
	BYTE	outer;

	INT	PRGBank( INT bank );
	INT	CHRBank( INT bank );
private:
};
