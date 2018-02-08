// license:BSD-3-Clause
// copyright-holders:R. Belmont
/***************************************************************************

 inteladv.cpp: VTech Intelligence Advance E/R Lerncomputer

 CPU is a Rockwell R65C02 (the dead-end bit-twiddling 65C02, as opposed to
 the WDC version that the 65816 is back-compatible with) with some customizations:

 JMP (ZP) accepts a 3rd ZP location after the 16-bit address to jump to which
 contains a bank value for the ROM window at 0x4000.  I believe it's in 0x4000
 byte segments, as the first long jump uses a value of 4 and that leads to
 the correct start of a subroutine at 0x10000 + the offset.

 JSR / RTS may also push and pop an additional byte for the bank offset but
 this is not proven yet.

***************************************************************************/

#include "emu.h"
#include "cpu/m6502/r65c02.h"
#include "machine/timer.h"
#include "screen.h"
#include "speaker.h"

class inteladv_state : public driver_device
{
public:
	inteladv_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_palette(*this, "palette")
	{ }

	uint32_t screen_update_inteladv(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);

	void inteladv(machine_config &config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;
	virtual void video_start() override;

private:
	required_device<cpu_device> m_maincpu;
	required_device<palette_device> m_palette;
};

void inteladv_state::video_start()
{
}

uint32_t inteladv_state::screen_update_inteladv(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	bitmap.fill(0, cliprect);
	screen.priority().fill(0, cliprect);

	return 0;
}

static ADDRESS_MAP_START( inteladv_main, AS_PROGRAM, 8, inteladv_state )
	AM_RANGE(0x0000, 0x01ff) AM_RAM // zero page and stack
	AM_RANGE(0x4000, 0x5fff) AM_ROM AM_REGION("maincpu", 0x0000)    // boot code at 4000
	AM_RANGE(0x8000, 0x8fff) AM_ROM AM_REGION("maincpu", 0x8000)    // fixed ROM region?
	AM_RANGE(0xf000, 0xffff) AM_ROM AM_REGION("maincpu", 0x3000)    // boot and other vectors at 3FFx
ADDRESS_MAP_END

static INPUT_PORTS_START( inteladv )
INPUT_PORTS_END

void inteladv_state::machine_start()
{
}

void inteladv_state::machine_reset()
{
}

MACHINE_CONFIG_START(inteladv_state::inteladv)
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", R65C02, XTAL(1'000'000) )
	MCFG_CPU_PROGRAM_MAP(inteladv_main)

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(59.62)  /* verified on pcb */
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(64*8, 32*8)
	MCFG_SCREEN_VISIBLE_AREA(40, 400-1, 16, 240-1)
	MCFG_SCREEN_UPDATE_DRIVER(inteladv_state, screen_update_inteladv)
	MCFG_SCREEN_PALETTE("palette")

	MCFG_PALETTE_ADD("palette", 256)
	MCFG_PALETTE_ENABLE_SHADOWS()
	MCFG_PALETTE_FORMAT(XBGR)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")
MACHINE_CONFIG_END

ROM_START( inteladv )
	ROM_REGION( 0x800000, "maincpu", 0 ) /* main program */
	ROM_LOAD( "vtechinteladv.bin", 0x000000, 0x800000, CRC(e24dbbcb) SHA1(7cb7f25f5eb123ae4c46cd4529aafd95508b2210) )
ROM_END

GAME( 1995, inteladv,    0, inteladv, inteladv,  inteladv_state, 0, ROT0, "VTech", "Intelligence Advance E/R Lerncomputer", MACHINE_NOT_WORKING )
