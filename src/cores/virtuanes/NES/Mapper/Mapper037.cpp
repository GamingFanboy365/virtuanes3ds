//////////////////////////////////////////////////////////////////////////
// Mapper037  Nintendo PAL-ZZ (Super Mario Bros./Tetris/Nintendo World Cup)
//
// An MMC3 with an outer bank register at $6000-$7FFF (written while the
// MMC3's WRAM is enabled and writable): bits 0-1 pick the 64 KiB PRG block
// (0-2: the first, 3: the second) unless bit 2 selects the last 128 KiB,
// and bit 2 also picks the 128 KiB CHR half. Follows Nintendulator.
//////////////////////////////////////////////////////////////////////////
void	Mapper037::Reset()
{
	outer = 0;
	Mapper004::Reset();
}

INT	Mapper037::PRGBank( INT bank )
{
	INT	mask = (outer << 1) | 0x07;
	INT	base = ((outer | (outer & 2 & (outer << 1))) << 2) & ~0x07;
	return (bank & mask) | base;
}

INT	Mapper037::CHRBank( INT bank )
{
	return (bank & 0x7F) | ((outer << 5) & ~0x7F);
}

void	Mapper037::WriteLow( WORD addr, BYTE data )
{
	if( addr >= 0x6000 && (reg[3] & 0xC0) == 0x80 ) {
		outer = data & 0x07;
		SetBank_CPU();
		SetBank_PPU();
	} else {
		Mapper004::WriteLow( addr, data );
	}
}

void	Mapper037::SaveState( LPBYTE p )
{
	Mapper004::SaveState( p );
	p[64] = outer;
}

void	Mapper037::LoadState( LPBYTE p )
{
	Mapper004::LoadState( p );
	outer = p[64];
}
