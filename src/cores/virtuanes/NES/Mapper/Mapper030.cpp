//////////////////////////////////////////////////////////////////////////
// Mapper030  UNROM 512                                                  //
//
// Writes to $8000-$FFFF set the latch [MCCP PPPP]: 16 KiB PRG bank at
// $8000 (the last bank is fixed at $C000), 8 KiB CHR RAM bank, and the
// one-screen nametable on boards wired for it. The header picks the
// mirroring: horizontal or vertical as usual, one-screen from the latch
// with only the four-screen bit set, and four-screen from the last 8 KiB
// of CHR RAM with both bits set.
//
// With the battery bit set, the PRG ROM is an SST39SF0x0 flash chip the
// game can reprogram: writes to $8000-$BFFF go to the chip and only
// $C000-$FFFF reach the latch. Changed 4 KiB sectors are saved to
// <ROM name>.flash. Follows Nintendulator.
//////////////////////////////////////////////////////////////////////////
#define	FLASH_SECTOR	0x1000

Mapper030::~Mapper030()
{
	if( original ) {
		free( original );
	}
}

void	Mapper030::Reset()
{
	latch = 0;
	command = 0;
	idmode = 0;
	flash = nes->rom->IsSAVERAM() ? 1 : 0;

	for( INT i = 0; i < 0x4000; i++ ) {
		idpage[i] = (i & 1) ? 0xB7 : 0xBF;	// SST39SF040 device and manufacturer IDs
	}

	SetBank();
}

void	Mapper030::SetBank()
{
	if( idmode ) {
		SetPROM_Bank( 4, idpage+0x0000, BANKTYPE_ROM );
		SetPROM_Bank( 5, idpage+0x2000, BANKTYPE_ROM );
	} else {
		SetPROM_16K_Bank( 4, latch & 0x1F );
	}
	SetPROM_16K_Bank( 6, PROM_16K_SIZE-1 );

	for( INT i = 0; i < 8; i++ ) {
		SetCHR_1K_Bank( i, ((latch >> 5) & 0x03) * 8 + i );
	}

	BOOL	vertical = nes->rom->IsVMIRROR();
	if( nes->rom->Is4SCREEN() ) {
		if( vertical ) {
			// The last 8 KiB of CHR RAM holds four nametables.
			for( INT i = 0; i < 4; i++ ) {
				SetCHR_1K_Bank( 8+i, 24+i );
			}
		} else {
			if( latch & 0x80 ) SetVRAM_Mirror( VRAM_MIRROR4H );
			else		   SetVRAM_Mirror( VRAM_MIRROR4L );
		}
	} else {
		if( vertical ) SetVRAM_Mirror( VRAM_VMIRROR );
		else	       SetVRAM_Mirror( VRAM_HMIRROR );
	}
}

// SetCRAM_1K_Bank() limits CHR RAM to 8 KiB on boards without CHR ROM;
// these have 32 KiB.
void	Mapper030::SetCHR_1K_Bank( BYTE page, INT bank )
{
	bank &= 0x1F;
	CHANGE_PPU_MEM_BANK(CRAM+0x0400*bank);
	PPU_MEM_TYPE[page] = BANKTYPE_CRAM;
	PPU_MEM_PAGE[page] = bank;
	CRAM_USED[bank>>2] = 0xFF;
}

void	Mapper030::Write( WORD addr, BYTE data )
{
	if( flash && addr < 0xC000 ) {
		WriteFlash( addr, data );
	} else {
		latch = data;
		SetBank();
	}
}

void	Mapper030::WriteFlash( WORD addr, BYTE data )
{
	// The chip sees A0-A13 from the CPU and A14 up from the latch.
	INT	chip = ((latch & 0x01) << 14) | (addr & 0x3FFF);
	INT	target = (((latch & 0x1F) << 14) | (addr & 0x3FFF)) % PRGSize();
	LPBYTE	prg = nes->rom->GetPROM();

	if( data == 0xF0 && command != 3 ) {	// reset / leave software ID mode
		command = 0;
		if( idmode ) {
			idmode = 0;
			SetBank();
		}
		return;
	}

	switch( command ) {
		case	0:
		case	4:
			command = (chip == 0x5555 && data == 0xAA) ? command+1 : 0;
			break;
		case	1:
		case	5:
			command = (chip == 0x2AAA && data == 0x55) ? command+1 : 0;
			break;
		case	2:
			command = 0;
			if( chip == 0x5555 ) {
				if( data == 0xA0 ) command = 3;		// byte program
				if( data == 0x80 ) command = 4;		// erase
				if( data == 0x90 ) {			// software ID
					idmode = 1;
					SetBank();
				}
			}
			break;
		case	3:
			// Programming can only clear bits.
			prg[target] &= data;
			dirty = 1;
			command = 0;
			break;
		case	6:
			if( data == 0x30 ) {				// sector erase
				memset( prg + (target & ~(FLASH_SECTOR-1)), 0xFF, FLASH_SECTOR );
				dirty = 1;
			} else if( chip == 0x5555 && data == 0x10 ) {	// chip erase
				memset( prg, 0xFF, PRGSize() );
				dirty = 1;
			}
			command = 0;
			break;
	}
}

// <ROM name>.flash holds the sectors that differ from the ROM: a 4 byte
// little-endian offset followed by the sector's 4 KiB, repeated.
void	Mapper030::LoadBattery( LPCSTR path )
{
	if( !nes->rom->IsSAVERAM() || PRGSize() <= 0 ) {
		return;
	}

	LPBYTE	prg = nes->rom->GetPROM();
	if( !original ) {
		original = (LPBYTE)malloc( PRGSize() );
		if( !original ) {
			return;
		}
		memcpy( original, prg, PRGSize() );
	}
	dirty = 0;

	FILE*	fp = fopen( path, "rb" );
	if( !fp ) {
		return;
	}
	BYTE	header[4];
	while( fread( header, 4, 1, fp ) == 1 ) {
		INT	offset = header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
		if( offset < 0 || offset % FLASH_SECTOR || offset + FLASH_SECTOR > PRGSize() ) {
			break;
		}
		if( fread( prg + offset, FLASH_SECTOR, 1, fp ) != 1 ) {
			break;
		}
	}
	fclose( fp );
}

void	Mapper030::SaveBattery( LPCSTR path )
{
	if( !original || !dirty ) {
		return;
	}

	LPBYTE	prg = nes->rom->GetPROM();
	FILE*	fp = NULL;
	for( INT offset = 0; offset < PRGSize(); offset += FLASH_SECTOR ) {
		if( memcmp( prg + offset, original + offset, FLASH_SECTOR ) == 0 ) {
			continue;
		}
		if( !fp && !(fp = fopen( path, "wb" )) ) {
			return;
		}
		BYTE	header[4] = { (BYTE)offset, (BYTE)(offset >> 8), (BYTE)(offset >> 16), (BYTE)(offset >> 24) };
		fwrite( header, 4, 1, fp );
		fwrite( prg + offset, FLASH_SECTOR, 1, fp );
	}
	if( fp ) {
		fclose( fp );
	} else {
		remove( path );		// back to the original ROM
	}
	dirty = 0;
}

void	Mapper030::SaveState( LPBYTE p )
{
	p[0] = latch;
	p[1] = command;
	p[2] = idmode;
}

void	Mapper030::LoadState( LPBYTE p )
{
	latch = p[0];
	command = p[1];
	idmode = p[2];
	SetBank();
}
