//////////////////////////////////////////////////////////////////////////
// Mapper020  Nintendo Disk System(FDS)                                 //
//////////////////////////////////////////////////////////////////////////
void	Mapper020::Reset()
{
	irq_type = 0;

	irq_enable = irq_repeat = 0;
	irq_counter = irq_latch = 0;
	irq_occur = 0;
	irq_transfer = 0;

	disk_enable = 0xFF;
	sound_enable = 0xFF;

	block_point = 0;
	block_mode = 0;
	RW_start = 0xFF;
	size_file_data = 0;
	file_amount = 0;
	point = 0;
	first_access = 0;

	disk_eject = 0xFF;
	drive_ready = 0;

	disk_side = 0xFF;	// Eject
	disk = disk_w = NULL;

	// Mechanical sound
	sound_startup_flag  = 0xFF;
	sound_startup_timer = -1;	// stop
	sound_seekend_timer = -1;	// stop
	MechanicalSound( MECHANICAL_SOUND_ALLSTOP );

//	::memset( DRAM, 0xFF, sizeof(DRAM) );
	SetPROM_Bank( 3, DRAM+0x0000, BANKTYPE_DRAM );
	SetPROM_Bank( 4, DRAM+0x2000, BANKTYPE_DRAM );
	SetPROM_Bank( 5, DRAM+0x4000, BANKTYPE_DRAM );
	SetPROM_Bank( 6, DRAM+0x6000, BANKTYPE_DRAM );
	SetPROM_Bank( 7, nes->rom->GetDISKBIOS(), BANKTYPE_ROM );
	SetCRAM_8K_Bank( 0 );

	// ƒfƒtƒHƒ‹ƒg
//	nes->SetIrqType( NES::IRQ_HSYNC );

	// ‹A‚Á‚Ä‚«‚½ƒ}ƒŠƒIƒuƒ‰ƒU[ƒY
	if( nes->rom->GetMakerID() == 0x01 && nes->rom->GetGameID() == 0x4b4d4152 ) {
		nes->SetRenderMethod( NES::PRE_ALL_RENDER );
	}

	// ƒKƒ‹ƒtƒH[ƒX
	if( nes->rom->GetMakerID() == 0xB6 && nes->rom->GetGameID() == 0x47414C20 ) {
		nes->SetRenderMethod( NES::PRE_ALL_RENDER );
	}
	// ƒtƒ@ƒCƒA[ƒoƒ€
	if( nes->rom->GetMakerID() == 0xB6 && nes->rom->GetGameID() == 0x46424D20 ) {
		nes->SetRenderMethod( NES::PRE_ALL_RENDER );
	}
	// ‚R‚cƒzƒbƒgƒ‰ƒŠ[
	if( nes->rom->GetMakerID() == 0x01 && nes->rom->GetGameID() == 0x54445245 ) {
		nes->SetRenderMethod( NES::PRE_ALL_RENDER );
		irq_type = 1;	// ƒCƒ“ƒ`ƒL
	}
	// ƒ^ƒCƒ€ƒcƒCƒXƒg
	if( nes->rom->GetMakerID() == 0x01 && nes->rom->GetGameID() == 0x54540120 ) {
		nes->SetRenderMethod( NES::PRE_ALL_RENDER );
	}
	// ƒoƒCƒIƒ~ƒ‰ƒNƒ‹‚Ú‚­‚Á‚ÄƒEƒp
	if( nes->rom->GetMakerID() == 0xA4 && nes->rom->GetGameID() == 0x424F4B20 ) {
		nes->SetRenderMethod( NES::PRE_ALL_RENDER );
	}
	// �p�b�g�p�b�g�S���t
	if( nes->rom->GetMakerID() == 0x99 && nes->rom->GetGameID() == 0x50504720 ) {
		irq_type = 99;
	}

	// �p�`����
	if( nes->rom->GetMakerID() == 0x00 && nes->rom->GetGameID() == 0x00000000 ) {
		::memset( RAM+0x100, 0xFF, 0x100 );
	}

DEBUGOUT( "MAKER ID=%02X\n", nes->rom->GetMakerID() );
DEBUGOUT( "GAME  ID=%08X\n", nes->rom->GetGameID() );

	nes->apu->SelectExSound( 4 );

//	ExCmdWrite( EXCMDWR_DISKINSERT, 0 );
	// Disk 0, Side A‚ðƒZƒbƒg
	disk = nes->rom->GetPROM()+16+65500*0;
	disk_w = nes->rom->GetDISK()+16+65500*0;

	disk_side = 0;
	disk_eject = 0xFF;
	drive_ready = 0;
	disk_mount_count = 119;

	nes->Command( NES::NESCMD_DISK_THROTTLE_OFF );

	bDiskThrottle = FALSE;
	DiskThrottleTime = 0;

	swap_started = 0;
	AutoSwapReset();
}

BYTE	Mapper020::ExRead( WORD addr )
{
BYTE	data = addr>>8;

	switch( addr ) {
		case	0x4030:	// Disk I/O status
			data = 0x80;
			data |= (irq_occur && !irq_transfer)?0x01:0x00;
			data |= (irq_occur && irq_transfer)?0x02:0x00;
			irq_occur = 0;

			// Clear Timer IRQ
			nes->cpu->ClrIRQ( IRQ_MAPPER );
			// Clear Disk IRQ
			nes->cpu->ClrIRQ( IRQ_MAPPER2 );
			break;
		case	0x4031:	// Disk data read
			if( !RW_mode )
				return	0xFF;

			first_access = 0;

			if( disk ) {
				switch( block_mode ) {
					case BLOCK_VOLUME_LABEL:
						data = disk[block_point];
						if( block_point < SIZE_VOLUME_LABEL ) {
							block_point++;
						} else {
							data = 0;
						}
						return	data;
					case BLOCK_FILE_AMOUNT:
						data = disk[ block_point + point ];
						if( block_point < SIZE_FILE_AMOUNT ) {
							block_point++;
							file_amount = data;
						} else {
							data = 0;
						}
						return	data;
					case BLOCK_FILE_HEADER:
						data = disk[ block_point + point ];
						if( block_point == 13 )
							size_file_data = data;
						else if( block_point == 14 )
							size_file_data += ((INT)data<<8);

						if( block_point < SIZE_FILE_HEADER ) {
							block_point++;
						} else {
							data = 0;
						}
						return	data;
					case BLOCK_FILE_DATA:
						data = disk[ block_point + point ];
						if( block_point < size_file_data + 1 ) {
							block_point++;
						} else {
							data = 0;
						}
						return	data;
				}
			} else {
				return	0xFF;
			}
			break;
		case	0x4032:	// Disk status
			// The drive is idle: the game isn't waiting for a transfer.
			if( !irq_transfer && (drive_reset || !disk_motor_mode) )
				AutoSwapPoll();

			data = 0x40;
			data |= disk_eject?0x01:0x00;
			data |= disk_eject?0x04:0x00;
			data |= (!disk_eject && disk_motor_mode && !drive_reset)?0x00:0x02;
			break;
		case	0x4033:	// External connector data/Battery sense
			data = 0x80;
			break;
		default:
			if( addr >= 0x4040 )
				data = nes->apu->ExRead( addr );
			break;
	}

//if( addr >= 0x4030 && addr <= 0x4033 ){
//DEBUGOUT( "R %04X %02X\n", addr, data );
//}
	return	data;
}

void	Mapper020::ExWrite( WORD addr, BYTE data )
{
//DEBUGOUT( "W %04X %02X C:%d\n", addr, data, nes->cpu->GetTotalCycles() );
	switch( addr ) {
		case	0x4020:	// IRQ latch low
			irq_latch = (irq_latch&0xFF00)|data;
			break;
		case	0x4021:	// IRQ latch high
			irq_latch = (irq_latch&0x00FF)|((WORD)data<<8);
			break;
		case	0x4022:	// IRQ control
			irq_repeat = data & 0x01;
			irq_enable = (data & 0x02) && (disk_enable);
			irq_occur  = 0;
			if( irq_enable ) {
				irq_counter = irq_latch;
			} else {
				// Clear Timer IRQ
				nes->cpu->ClrIRQ( IRQ_MAPPER );
			}
			break;

		case	0x4023: // 2C33 control
			disk_enable = data & 0x01;
			if( !disk_enable ) {
				irq_enable = 0;
				irq_occur  = 0;
				// Clear Timer IRQ
				nes->cpu->ClrIRQ( IRQ_MAPPER );
				// Clear Disk IRQ
				nes->cpu->ClrIRQ( IRQ_MAPPER2 );
			}
			break;

		case	0x4024:	// Data write
			// Clear Disk IRQ
			nes->cpu->ClrIRQ( IRQ_MAPPER2 );

			if( RW_mode )
				break;

			if( first_access ) {
				first_access = 0;
				break;
			}

			if( disk ) {
				switch( block_mode ) {
					case BLOCK_VOLUME_LABEL:
						if( block_point < SIZE_VOLUME_LABEL-1 ) {
							disk[ block_point ] = data;
							disk_w[ block_point ] = 0xFF;
							block_point++;
						}
						break;
					case BLOCK_FILE_AMOUNT:
						if( block_point < SIZE_FILE_AMOUNT ) {
							disk[ block_point + point ] = data;
							disk_w[ block_point + point ] = 0xFF;
							block_point++;
						}
						break;
					case BLOCK_FILE_HEADER:
						if( block_point < SIZE_FILE_HEADER ) {
								disk[ block_point + point ] = data;
								disk_w[ block_point + point ] = 0xFF;
								if( block_point == 13 )
									size_file_data = data;
								else if( block_point == 14 )
									size_file_data |= data << 8;
								block_point++;
						}
						break;
					case BLOCK_FILE_DATA:
						if( block_point < size_file_data+1 ) {
							disk[ block_point + point ] = data;
							disk_w[ block_point + point ] = 0xFF;
							block_point++;
						}
						break;
				}
			}
			break;

		case	0x4025:	// Disk I/O control
			// Š„‚èž‚Ý“]‘—
			irq_transfer = data & 0x80;
			if( !irq_transfer ) {
				nes->cpu->ClrIRQ( IRQ_MAPPER2 );
			}

			if( !RW_start && (data & 0x40) ) {
				block_point = 0;
				switch( block_mode ) {
					case	BLOCK_READY:
						block_mode = BLOCK_VOLUME_LABEL;
						point = 0;
						AutoSwapFindSide();
						break;
					case	BLOCK_VOLUME_LABEL:
						block_mode = BLOCK_FILE_AMOUNT;
						point += SIZE_VOLUME_LABEL;
						break;
					case	BLOCK_FILE_AMOUNT:
						block_mode = BLOCK_FILE_HEADER;
						point += SIZE_FILE_AMOUNT;
						break;
					case	BLOCK_FILE_HEADER:
						block_mode = BLOCK_FILE_DATA;
						point += SIZE_FILE_HEADER;
						break;
					case	BLOCK_FILE_DATA:
						block_mode = BLOCK_FILE_HEADER;
						point += size_file_data+1;
						break;
				}

				// Å‰‚Ì‚P‰ñ–Ú‚Ì‘‚«ž‚Ý‚ð–³Ž‹‚·‚é‚½‚ß
				first_access = 0xFF;
			}

			// “Ç‚Ý‘‚«ƒXƒ^[ƒg
			RW_start = data & 0x40;

			// “Ç‚Ý‘‚«ƒ‚[ƒh
			RW_mode = data & 0x04;

			// “Ç‚Ý‘‚«‚ÌƒŠƒZƒbƒg
			if( data&0x02 ) {
				point = 0;
				block_point = 0;
				block_mode = BLOCK_READY;
				RW_start = 0xFF;
				drive_reset = 0xFF;

				sound_startup_flag  = 0;
				sound_startup_timer = -1;	// stop
			} else {
				drive_reset = 0;

				if( !sound_startup_flag ) {
					MechanicalSound( MECHANICAL_SOUND_MOTOR_ON );
					sound_startup_flag  = 0xFF;
					sound_startup_timer = 40;
					sound_seekend_timer = 60*7;
				}
			}

			// ƒfƒBƒXƒNƒ‚[ƒ^[‚ÌƒRƒ“ƒgƒ[ƒ‹
			disk_motor_mode = data & 0x01;

			// The game is about to use the disk.
			if( swap_eject_timer > 0 && disk_motor_mode && !drive_reset ) {
				AutoSwapInsert();
				disk_eject = 0;
			}
			if( !(data & 0x01) ) {
				if( sound_seekend_timer >= 0 ) {
					sound_seekend_timer = -1;
					MechanicalSound( MECHANICAL_SOUND_MOTOR_OFF );
				}
			}

			// Mirror
			if( data&0x08 ) SetVRAM_Mirror( VRAM_HMIRROR );
			else		SetVRAM_Mirror( VRAM_VMIRROR );
			break;

		case	0x4026:	// External connector output/Battery sense
			break;
		default:
			if( addr >= 0x4040 )
				nes->apu->ExWrite( addr, data );
			break;
	}
}

void	Mapper020::WriteLow( WORD addr, BYTE data )
{
	DRAM[addr-0x6000] = data;
}

void	Mapper020::Write( WORD addr, BYTE data )
{
	if( addr < 0xE000 ) {
		DRAM[addr-0x6000] = data;
	}
}

void	Mapper020::Clock( INT cycles )
{
	// Timer IRQ
	if( irq_enable ) {
		if( !irq_type ) {
			irq_counter -= cycles;
		}
		if( irq_counter <= 0 ) {
////			irq_counter &= 0xFFFF;
			irq_counter += irq_latch;

			if( !irq_occur ) {
				irq_occur = 0xFF;
				if( !irq_repeat ) {
					irq_enable = 0;
				}
				if( irq_type != 99 ) {
					nes->cpu->SetIRQ( IRQ_MAPPER );
				} else {
					nes->cpu->SetIRQ( IRQ_TRIGGER2 );
				}
			}
		}
		if( irq_type ) {
			irq_counter -= cycles;
		}
	}
}

void	Mapper020::HSync( INT scanline )
{
	// Disk IRQ
	if( irq_transfer ) {
		nes->cpu->SetIRQ( IRQ_MAPPER2 );
	}
}

void	Mapper020::VSync()
{
	AutoSwapVSync();

	if( disk && disk_eject ) {
		if( disk_mount_count > 120 ) {
			disk_eject = 0;
		} else {
			disk_mount_count++;
		}
	}

//	if( disk && (disk_mount_count > 120) ) {
	if( disk ) {
		if( sound_startup_timer > 0 ) {
			sound_startup_timer--;
		} else if( sound_startup_timer == 0 ) {
			sound_startup_timer--;
			MechanicalSound( MECHANICAL_SOUND_BOOT );
		}

		if( sound_seekend_timer > 0 ) {
			sound_seekend_timer--;
		} else if( sound_seekend_timer == 0 ) {
			sound_seekend_timer--;
			MechanicalSound( MECHANICAL_SOUND_MOTOR_OFF );
			MechanicalSound( MECHANICAL_SOUND_SEEKEND );
			sound_startup_flag  = 0;
		}
	}

	if( irq_transfer || (disk && disk_mount_count < 120) ) {
		if( DiskThrottleTime > 2 ) {
			bDiskThrottle = TRUE;
		} else {
			bDiskThrottle = FALSE;
			DiskThrottleTime++;
		}
	} else {
		DiskThrottleTime = 0;
		bDiskThrottle = FALSE;
	}
	if( !bDiskThrottle ) {
		nes->Command( NES::NESCMD_DISK_THROTTLE_OFF );
	} else {
		nes->Command( NES::NESCMD_DISK_THROTTLE_ON );
	}
}

//
// Automatic disk swap
//
// Games ask for a disk side in two ways, and often both:
//
// - They read the disk through the BIOS, whose CheckDiskHeader ($E445)
//   compares 10 bytes of the disk's header (maker, game, version, side
//   and disk number; $FF matches anything) with the ones ($00) points
//   to. AutoSwapFindSide puts in the side that matches as the BIOS
//   starts reading the header.
// - They ask the player to swap the disk, and poll the drive status
//   ($4032) until the disk is out and then in again. AutoSwapVSync does
//   that. It puts in the next side; AutoSwapFindSide corrects it, and if
//   it can't tell, the next try gets to the right one.
//
// Some games (Famicom Mukashibanashi) also poll the drive all the time, to
// note that the disk was out. So polling is counted by the address of the
// code that does it. If the game doesn't read the disk after a swap, that
// code isn't waiting for one, and is ignored until it stops polling. If the
// game uses the disk while it is out, it goes back in at once.
//
BOOL	Mapper020::IsAutoSwap()
{
	return	Config.emulator.bAutoDiskSwap && nes->rom->GetDiskNo() > 1;
}

void	Mapper020::AutoSwapReset()
{
	::memset( swap_pollers, 0, sizeof(swap_pollers) );
	swap_next_side = 0;
	swap_eject_timer = 0;
	swap_read_timer = 0;
}

void	Mapper020::AutoSwapInsert()
{
	DEBUGOUT( "FDS: auto insert side %d\n", swap_next_side );
	AutoSwapSetSide( swap_next_side );
	disk_eject = 0xFF;
	drive_ready = 0;
	disk_mount_count = 119;
	swap_eject_timer = 0;
	swap_read_timer = AUTOSWAP_READ_FRAMES;
}

void	Mapper020::AutoSwapPoll()
{
	if( !IsAutoSwap() )
		return;

	R6502	reg;
	nes->cpu->GetContext( reg );

	POLLER*	free = NULL;
	for( INT i = 0; i < AUTOSWAP_POLLERS; i++ ) {
		if( swap_pollers[i].pc == reg.PC ) {
			swap_pollers[i].polled = 0xFF;
			return;
		}
		if( !swap_pollers[i].pc )
			free = &swap_pollers[i];
	}
	if( free ) {
		::memset( free, 0, sizeof(POLLER) );
		free->pc = reg.PC;
		free->polled = 0xFF;
	}
}

void	Mapper020::AutoSwapSetSide( BYTE side )
{
	disk = nes->rom->GetPROM()+sizeof(NESHEADER)+65500*side;
	disk_w = nes->rom->GetDISK()+sizeof(NESHEADER)+65500*side;
	disk_side = side;
}

void	Mapper020::AutoSwapFindSide()
{
	if( disk )
		swap_started = 0xFF;
	if( !IsAutoSwap() || !disk )
		return;

	// The game read the disk after a swap: that's what it was waiting
	// for, and it may be for another if this isn't the right side.
	for( INT i = 0; i < AUTOSWAP_POLLERS; i++ ) {
		if( swap_pollers[i].state == POLLER_TRIED )
			swap_pollers[i].state = POLLER_COUNTING;
		swap_pollers[i].frames = 0;
	}
	swap_read_timer = 0;

	// Only for CheckDiskHeader, as ($00) is something else in the other
	// BIOS calls. It begins with a JSR $E6E3, which is where we are now:
	// look for its return address on the stack.
	LPBYTE	bios = nes->rom->GetDISKBIOS();
	if( bios[0x0445] != 0x20 || bios[0x0446] != 0xE3 || bios[0x0447] != 0xE6 )
		return;

	R6502	reg;
	nes->cpu->GetContext( reg );
	BOOL	found = FALSE;
	for( INT i = 1; i <= 16 && reg.S+i+1 <= 0xFF; i++ ) {
		if( RAM[0x100+reg.S+i] == 0x47 && RAM[0x100+reg.S+i+1] == 0xE4 ) {
			found = TRUE;
			break;
		}
	}
	if( !found )
		return;

	// The header the game wants
	WORD	addr = RAM[0]|((WORD)RAM[1]<<8);
	BYTE	want[10];
	for( INT i = 0; i < 10; i++, addr++ ) {
		if( addr < 0x2000 )
			want[i] = RAM[addr&0x07FF];
		else if( addr >= 0x6000 )
			want[i] = CPU_MEM_BANK[addr>>13][addr&0x1FFF];
		else
			return;
	}

	// The side that is in comes first, so that it stays if it matches.
	INT	sides = nes->rom->GetDiskNo();
	for( INT n = 0; n < sides; n++ ) {
		INT	side = (disk_side+n)%sides;
		LPBYTE	header = nes->rom->GetPROM()+sizeof(NESHEADER)+65500*side+15;
		INT	i;
		for( i = 0; i < 10; i++ ) {
			if( want[i] != 0xFF && want[i] != header[i] )
				break;
		}
		if( i == 10 ) {
			if( side != disk_side ) {
				DEBUGOUT( "FDS: auto swap to side %d\n", side );
				AutoSwapSetSide( side );
			}
			return;
		}
	}
}

void	Mapper020::AutoSwapVSync()
{
	// The disk was taken out: put the next side in. (Also if the setting
	// was turned off meanwhile, or the drive would stay empty.)
	if( swap_eject_timer > 0 ) {
		if( --swap_eject_timer == 0 )
			AutoSwapInsert();
		return;
	}

	if( swap_read_timer > 0 && --swap_read_timer == 0 ) {
		for( INT i = 0; i < AUTOSWAP_POLLERS; i++ ) {
			if( swap_pollers[i].state == POLLER_TRIED ) {
				DEBUGOUT( "FDS: ignoring polls at %04X\n", swap_pollers[i].pc );
				swap_pollers[i].state = POLLER_IGNORED;
			}
		}
	}

	// (The BIOS's boot screen polls as well, before it reads the disk.)
	if( !IsAutoSwap() || !swap_started || !disk || disk_eject || irq_transfer )
		return;

	for( INT i = 0; i < AUTOSWAP_POLLERS; i++ ) {
		POLLER&	poller = swap_pollers[i];
		if( !poller.pc )
			continue;

		if( !poller.polled ) {
			if( ++poller.gap > AUTOSWAP_POLL_GAP )
				poller.pc = 0;
			continue;
		}
		poller.polled = 0;
		poller.gap = 0;
		poller.frames++;

		if( poller.state == POLLER_COUNTING && poller.frames >= AUTOSWAP_POLL_FRAMES
		 && swap_read_timer == 0 && swap_eject_timer == 0 ) {
			DEBUGOUT( "FDS: auto eject side %d (polls at %04X)\n", disk_side, poller.pc );
			poller.state = POLLER_TRIED;
			swap_next_side = (disk_side+1)%nes->rom->GetDiskNo();
			swap_eject_timer = AUTOSWAP_EJECT_FRAMES;

			disk = NULL;
			disk_w = NULL;
			disk_side = 0xFF;
			disk_eject = 0xFF;
			drive_ready = 0;
			disk_mount_count = 0;
		}
	}
}

BYTE	Mapper020::ExCmdRead( EXCMDRD cmd )
{
BYTE	data = 0x00;

	if( cmd == EXCMDRD_DISKACCESS ) {
		if( irq_transfer )
			return	0xFF;
	}

	return	data;
}

void	Mapper020::ExCmdWrite( EXCMDWR cmd, BYTE data )
{
	switch( cmd ) {
		case	EXCMDWR_NONE:
			break;
		case	EXCMDWR_DISKINSERT:
			AutoSwapReset();
			disk = nes->rom->GetPROM()+16+65500*data;
			disk_w = nes->rom->GetDISK()+16+65500*data;
			disk_side = data;
			disk_eject = 0xFF;
			drive_ready = 0;
			disk_mount_count = 0;
			break;
		case	EXCMDWR_DISKEJECT:
			AutoSwapReset();
			disk = NULL;	// ‚Æ‚è‚ ‚¦‚¸
			disk_w = NULL;
			disk_side = 0xFF;
			disk_eject = 0xFF;
			drive_ready = 0;
			disk_mount_count = 0;
			break;
	}
}

void	Mapper020::MechanicalSound( INT type )
{
	switch( type ) {
		case	MECHANICAL_SOUND_BOOT:
			// Head start point CAM sound.
			//if( Config.sound.bExtraSoundEnable )
			//	DirectSound.EsfPlay( ESF_DISKSYSTEM_BOOT );
			break;
		case	MECHANICAL_SOUND_SEEKEND:
			//if( Config.sound.bExtraSoundEnable )
			//	DirectSound.EsfPlay( ESF_DISKSYSTEM_SEEKEND );
			// Reset or Seekend sound.
			break;
		case	MECHANICAL_SOUND_MOTOR_ON:
			//if( Config.sound.bExtraSoundEnable )
			//	DirectSound.EsfPlayLoop( ESF_DISKSYSTEM_MOTOR );
			// Start Motor sound.(loop)
			break;
		case	MECHANICAL_SOUND_MOTOR_OFF:
			//DirectSound.EsfStop( ESF_DISKSYSTEM_MOTOR );
			// Stop Motor sound.(loop)
			break;
		case	MECHANICAL_SOUND_ALLSTOP:
			// Stop sound.
			//DirectSound.EsfAllStop();
			break;
		default:
			break;
	}
}

void	Mapper020::SaveState( LPBYTE p )
{
	p[0] = irq_enable;
	p[1] = irq_repeat;
	p[2] = irq_occur;
	p[3] = irq_transfer;

	*(INT*)&p[4] = irq_counter;
	*(INT*)&p[8] = irq_latch;

	p[12] = disk_enable;
	p[13] = sound_enable;
	p[14] = RW_start;
	p[15] = RW_mode;
	p[16] = disk_motor_mode;
	p[17] = disk_eject;
	p[18] = drive_ready;
	p[19] = drive_reset;

	*(INT*)&p[20] = block_point;
	*(INT*)&p[24] = block_mode;
	*(INT*)&p[28] = size_file_data;
	*(INT*)&p[32] = file_amount;
	*(INT*)&p[36] = point;

	p[40] = first_access;
	p[41] = disk_side;
	p[42] = disk_mount_count;

	p[44] = sound_startup_flag;
	*(INT*)&p[48] = sound_startup_timer;
	*(INT*)&p[52] = sound_seekend_timer;
}

void	Mapper020::LoadState( LPBYTE p )
{
	irq_enable	= p[0];
	irq_repeat	= p[1];
	irq_occur	= p[2];
	irq_transfer	= p[3];

	irq_counter	= *(INT*)&p[4];
	irq_latch	= *(INT*)&p[8];

	disk_enable	= p[12];
	sound_enable	= p[13];
	RW_start	= p[14];
	RW_mode		= p[15];
	disk_motor_mode	= p[16];
	disk_eject	= p[17];
	drive_ready	= p[18];
	drive_reset	= p[19];

	block_point	= *(INT*)&p[20];
	block_mode	= *(INT*)&p[24];
	size_file_data	= *(INT*)&p[28];
	file_amount	= *(INT*)&p[32];
	point		= *(INT*)&p[36];

	first_access	= p[40];
	disk_side	= p[41];
	disk_mount_count= p[42];

	sound_startup_flag = p[44];
	sound_startup_timer = *(INT*)&p[48];
	sound_seekend_timer = *(INT*)&p[52];

	swap_started = 0xFF;
	AutoSwapReset();
	if( disk_side == 0xFF && IsAutoSwap() ) {
		// The state may be from the middle of an automatic swap
		swap_eject_timer = AUTOSWAP_EJECT_FRAMES;
	}

	if( disk_side != 0xFF ) {
		disk = nes->rom->GetPROM()+sizeof(NESHEADER)+65500*disk_side;
		disk_w = nes->rom->GetDISK()+sizeof(NESHEADER)+65500*disk_side;
	} else {
		disk = NULL;
		disk_w = NULL;
	}

	// DiskBios Setup(ƒXƒe[ƒg‚Åã‘‚«‚³‚ê‚Ä‚¢‚éˆ×)
	SetPROM_Bank( 7, nes->rom->GetDISKBIOS(), BANKTYPE_ROM );
}
