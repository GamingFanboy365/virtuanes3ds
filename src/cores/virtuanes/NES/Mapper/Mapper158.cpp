//////////////////////////////////////////////////////////////////////////
// Mapper158  Tengen 800037 (RAMBO-1 with CHR-controlled nametables)     //
//
// Like mapper 64, but the nametable at $2000+$400*i comes from bit 7 of
// the CHR register mapped at PPU $0000+$400*i, and $A000 does nothing.
//////////////////////////////////////////////////////////////////////////
void	Mapper158::Reset()
{
	Mapper064::Reset();
	for( INT i = 0; i < 8; i++ ) {
		ntsel[i] = 0;
	}
	SetNametables();
}

void	Mapper158::Write( WORD addr, BYTE data )
{
	if( (addr & 0xF003) == 0xA000 ) {
		return;
	}

	Mapper064::Write( addr, data );

	if( (addr & 0xF003) == 0x8001 ) {
		// The same pages Mapper064::Write just switched.
		BYTE	nt = data >> 7;
		switch( reg[0] ) {
			case	0x00:
				if( reg[2] ) { ntsel[4] = ntsel[5] = nt; }
				else	     { ntsel[0] = ntsel[1] = nt; }
				break;
			case	0x01:
				if( reg[2] ) { ntsel[6] = ntsel[7] = nt; }
				else	     { ntsel[2] = ntsel[3] = nt; }
				break;
			case	0x02: ntsel[reg[2] ? 0 : 4] = nt; break;
			case	0x03: ntsel[reg[2] ? 1 : 5] = nt; break;
			case	0x04: ntsel[reg[2] ? 2 : 6] = nt; break;
			case	0x05: ntsel[reg[2] ? 3 : 7] = nt; break;
			case	0x08: ntsel[1] = nt; break;
			case	0x09: ntsel[3] = nt; break;
		}
		SetNametables();
	}
}

void	Mapper158::SetNametables()
{
	for( INT i = 0; i < 4; i++ ) {
		SetVRAM_1K_Bank( 8+i, ntsel[i] );
	}
}

void	Mapper158::SaveState( LPBYTE p )
{
	Mapper064::SaveState( p );
	for( INT i = 0; i < 8; i++ ) {
		p[64+i] = ntsel[i];
	}
}

void	Mapper158::LoadState( LPBYTE p )
{
	Mapper064::LoadState( p );
	for( INT i = 0; i < 8; i++ ) {
		ntsel[i] = p[64+i];
	}
	SetNametables();
}
