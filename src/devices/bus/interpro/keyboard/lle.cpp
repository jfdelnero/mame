// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * A low level emulation implementation of the Intergraph InterPro keyboard.
 *
 * These keyboards have two primary banks of keys. The lower bank consists of
 * a total of 67 regular keyboard keyswitches plus a numeric keypad with a
 * further 18 keys. The upper bank consists of 57 membrane-style programmable
 * function keys in groups of 9, 36 and 12 from left to right.
 *
 * The following describes the key labels and positions according to the
 * standard US English keyboard layout. At least a German keyboard variant is
 * known to have existed.
 *
 * Upper bank keys indicated here with asterisks are printed in white, as are
 * all the A*, B* and C* keys; all the others are printed in brown.
 *
 *    Setup*  Home*   2nd             A1      A2      A3      A4      A5      A6      A7      A8      A9      A10     A11     A12          A13     A14     A15     2nd
 *    Help    Clear   F*                                                                                                                                           F*
 *            Screen
 *
 *    Find    Insert  Print           B1      B2      B3      B4      B5      B6      B7      B8      B9      B10     B11     B12          B13     B14     B15     B16
 *            Here    Screen*
 *                    Remove
 *
 *    Select  Prev    Next            C1      C2      C3      C4      C5      C6      C7      C8      C9      C10     C11     C12          C13     C14     C15     C16
 *            Screen  Screen
 *
 *
 * In between the banks on the right hand side, there is a row of LEDs, the
 * first three are pictures, rather than the descriptions given here).
 *
 *                                                                                                                                         Disk Lock ----- L1 L2 L3 L4
 *
 * Lower bank keys have up to 3 labels, in shifted and unshifted positions, and
 * in red on the front face of the key-cap.
 *
 *    Esc     ~       !       @       #       $       %       ^       &       *       (       )       _       +       Back    Delete       PF1     PF2     PF3     PF4
 *            `       1       2       3       4       5       6       7       8       9       0       -       =       Space                ±       ÷       ×       +
 *                                                                                                                                         Esc     Num Lk  ScrlLk  Sys
 *
 *    Alt     Tab         Q       W       E       R       T       Y       U       I       O       P       {       }                        7       8       9       _
 *    Mode                                                                                                [       ]
 *                                                                                                                                         Home    ↑       Pg Up   Prt Sc
 *
 *    Ctrl    Caps          A       S       D       F       G       H       J       K       L       :       "       |         Return       4       5       6       ,
 *            Lock
 *                                                                                                                                         ←               →       −
 *
 *            Shift     >       Z       X       C       V       B       N       M       ,       .       ?       Shift         ▲            1       2       3       =
 *                      <                                                                                                     ■
 *                                                                                                                                         End     ↓       Pg Dn
 *
 *    Hold       Super-   Line                                                                          Repeat        ◄       ■       ►    0               ◦
 *    Screen     impose   Feed                                                                                                ▼                            .       Enter
 *                                                                                                                                         Ins             Del     +
 *
 * Alt Mode and Caps Lock keys have locking switches, capturing the key in the
 * depressed position, as well as physical leds visible on the keycaps
 * themselves. The keyboard also has two physical buttons on its rear face, a
 * circular button labelled Boot, and a square one labelled Reset.
 *
 * The keyboard uses a 1200bps serial protocol to communicate with the host,
 * with 1 start bit, 8 data bits, 1 stop bit, and even parity.
 *
 *   Ref   Part                     Function
 *     1   SN74LS244N               octal buffer and line driver with tri-state outputs
 *     2   M2732A-2F1               NMOS 32K (4Kx8) UV EPROM
 *     3   SN74LS374N               octal d-type flip-flop
 *     4   SN74LS373N               octal d-type latch
 *     5   Intel 8049AH             MCS-48 family microcontroller
 *     6   SN74LS368AN              hex bus driver with tri-state outputs
 *     7   SN74159N                 4-to-16 decoder/demultiplexer with open collector outputs
 *     8   SN74LS14N                hex Schmitt trigger inverter
 *     9   Motorola SC2 3181C       ?
 *    10   DS75452N                 dual peripheral NAND driver
 *    52   11.000MHz crystal
 *
 *    67                            upper matrix connector?
 *    68                            reset and boot button connector
 *    69                            computer interface cable connector
 *    70, 71, 72                    lower matrix connectors?
 *    5?-64                         status LEDs
 *
 * The keyboard software relies on a timer interrupt to handle all serial I/O
 * and the speaker, while running the main loop which processes commands, and
 * scans the upper and lower banks for key status. The lower bank is scanned
 * via some unknown external means, possibly the custom Motorola device? This
 * method involves clearing and incrementing a counter corresponding to each
 * key position, and then reading the selected key state on the T1 input. The
 * upper bank is handled via more conventional matrix scan where 6 bits of data
 * are read from the bus for each of 15 matrix rows in turn.
 *
 * The raw count or matrix information is then looked up in various tables (one
 * in the on-board rom, and others in the external EPROM), to translate into the
 * relevant make/break codes that are then output via the serial interface.
 *
 * TODO
 *   - upper matrix is not mapped
 *   - arrow keys, keypad and some other keys are not mapped
 */

/*
 * Work in progress notes
 *
 * 11MHz XTAL / 15 / 32 gives 22.916kHz clock (~43.6us/cycle)
 *   - prescaler loaded with 0xfa, gives ~262us/tick (~3.819kHz)
 *
 * P1
 *   0x0f - out: upper row select
 *   0x10 - out: speaker
 *   0x20 - out: enable upper row select
 *   0x40 - out: update led state
 *   0x80 - out: speaker enable?
 *
 * P2
 *   0x0f - out: bank select
 *   0x10 - ?
 *   0x20 - out: serial tx, inverted
 *   0x40 - out: increment lower count
 *   0x80 - out: reset lower count
 *
 * T0
 *   in: serial rx, inverted
 *
 * T1
 *   in: lower key state
 *
 * BUS
 *   in: upper scan state
 *   out: led state
 *
 * LEDs
 *   0x01 - L1?
 *   0x02 - L2?
 *   0x04 - L3?
 *   0x08 - L4?
 *   0x10 - bar?
 *   0x20 - lock?
 *   0x40 - caps
 *   0x80 - alt mode
 */

#include "emu.h"
#include "lle.h"

#include "speaker.h"
#include "machine/keyboard.ipp"

#define LOG_GENERAL (1U << 0)
#define LOG_RXTX    (1U << 1)
#define LOG_PORT    (1U << 2)

#define VERBOSE (0)
#include "logmacro.h"

DEFINE_DEVICE_TYPE_NS(INTERPRO_LLE_EN_US_KEYBOARD, bus::interpro::keyboard, lle_en_us_device, "kbd_lle_en_us", "InterPro Keyboard (LLE, US English)")

namespace bus { namespace interpro { namespace keyboard {

namespace {

INPUT_PORTS_START(interpro_en_us)

	PORT_START("upper.0")
	// nothing
	// fc 4c
	// fc 6c
	// nothing
	// nothing
	// nothing

	PORT_START("upper.1")
	// fc 28
	// fc 46
	// fc 66
	// fc 64
	// fc 44
	// fc 26

	PORT_START("upper.2")
	// fc 2a
	// fc 48
	// fc 68
	// fc 62
	// fc 42
	// fc 24

	PORT_START("upper.3")
	// fc 2c
	// fc 4a
	// fc 6a
	// fc 60
	// fc 40
	// fc 22

	PORT_START("upper.4")
	// fc 1c
	// fc 3a
	// fc 5a
	// fc 58
	// fc 38
	// fc 1a

	PORT_START("upper.5")
	// fc 1e
	// fc 3c
	// fc 5c
	// fc 56
	// fc 36
	// fc 18

	PORT_START("upper.6")
	// fc 20
	// fc 3e
	// fc 5e
	// fc 54
	// fc 34
	// fc 16

	PORT_START("upper.7")
	// fc 14
	// fc 32
	// fc 52
	// fc 01
	// fc 04
	// fc 07

	PORT_START("upper.8")
	// fc 12
	// fc 30
	// fc 50
	// fc 02
	// fc 05
	// fc 08

	PORT_START("upper.9")
	// fc 10
	// fc 2e
	// fc 4e
	// fc 03
	// fc 06
	// nothing?

	PORT_START("upper.a")
	// fc 6e
	// fc 76
	// fc 7e
	// nothing
	// nothing
	// nothing

	PORT_START("upper.b")
	// fc 70
	// fc 78
	// fc 80
	// nothing
	// nothing
	// nothing

	PORT_START("upper.c")
	// fc 72
	// fc 7a
	// fc 82
	// nothing
	// nothing
	// nothing

	PORT_START("upper.d")
	// fc 74
	// fc 7c
	// nothing
	// nothing
	// nothing
	// 1c fc

	PORT_START("upper.e")
	// nothing
	// nothing
	// nothing
	// nothing
	// nothing
	// nothing

	PORT_START("lower.0")
		// 0x00-0x07: 0x82 3 4 5 6 7 0 -
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0x82
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_3)                                 PORT_CHAR('3')  PORT_CHAR('#')
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_4)                                 PORT_CHAR('4')  PORT_CHAR('$')
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_5)                                 PORT_CHAR('5')  PORT_CHAR('%')
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_6)                                 PORT_CHAR('6')  PORT_CHAR('^')
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_7)                                 PORT_CHAR('7')  PORT_CHAR('&')
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_0)                                 PORT_CHAR('0')  PORT_CHAR(')')
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS)                             PORT_CHAR('-')  PORT_CHAR('_')

	PORT_START("lower.1")
		// 0x08-0x0f: del 0xf5 0xab 2 w e t u
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_DEL)        PORT_NAME("Delete")    PORT_CHAR(127)
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xf5
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xab
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_2)                                 PORT_CHAR('2')  PORT_CHAR('@')
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_W)                                 PORT_CHAR('w')  PORT_CHAR('W')
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_E)                                 PORT_CHAR('e')  PORT_CHAR('E')
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_T)                                 PORT_CHAR('t')  PORT_CHAR('T')
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_U)                                 PORT_CHAR('u')  PORT_CHAR('U')

	PORT_START("lower.2")
		// 0x10-0x17: 8 9 = bs 0xaf 0xaa 1 q
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_8)                                 PORT_CHAR('8')  PORT_CHAR('*')
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_9)                                 PORT_CHAR('9')  PORT_CHAR('(')
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_EQUALS)                            PORT_CHAR('=')  PORT_CHAR('+')
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSPACE)  PORT_NAME("Backspace") PORT_CHAR(8)
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xaf
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xaa
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_1)                                 PORT_CHAR('1')  PORT_CHAR('!')
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Q)                                 PORT_CHAR('q')  PORT_CHAR('Q')

	PORT_START("lower.3")
		// 0x18-0x1f: a r y i p ] none 0xb7
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_A)                                 PORT_CHAR('a')  PORT_CHAR('A')
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_R)                                 PORT_CHAR('r')  PORT_CHAR('R')
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Y)                                 PORT_CHAR('y')  PORT_CHAR('Y')
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_I)                                 PORT_CHAR('i')  PORT_CHAR('I')
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_P)                                 PORT_CHAR('p')  PORT_CHAR('P')
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE)                        PORT_CHAR(']')  PORT_CHAR('}')
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) // none - 0x1e never scanned?
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xb7

	PORT_START("lower.4")
		// 0x20-0x27: 0xad ` tab s f j o '
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xad
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_TILDE)                             PORT_CHAR('`')  PORT_CHAR('~')
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_TAB)        PORT_NAME("Tab")       PORT_CHAR(9)
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_S)                                 PORT_CHAR('s')  PORT_CHAR('S')
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F)                                 PORT_CHAR('f')  PORT_CHAR('F')
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_J)                                 PORT_CHAR('j')  PORT_CHAR('J')
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_O)                                 PORT_CHAR('o')  PORT_CHAR('O')
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE)                             PORT_CHAR('\'') PORT_CHAR('"')

	PORT_START("lower.5")
		// 0x28-0x2f: [ cr 0xb8 0xb9 none shift d g
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)                         PORT_CHAR('[')  PORT_CHAR('{')
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_ENTER)      PORT_NAME("Return")    PORT_CHAR(13)
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xb8
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xb9
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD)                               PORT_NAME("Alt Mode")  PORT_TOGGLE
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_CAPSLOCK)   PORT_NAME("Caps Lock") PORT_TOGGLE PORT_CHAR(UCHAR_MAMEKEY(CAPSLOCK))
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_D)                                 PORT_CHAR('d')  PORT_CHAR('D')
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_G)                                 PORT_CHAR('g')  PORT_CHAR('G')

	PORT_START("lower.6")
		// 0x30-0x37: h k ; \ 0x85 0xb4 0xac esc
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_H)                                 PORT_CHAR('h')  PORT_CHAR('H')
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_K)                                 PORT_CHAR('k')  PORT_CHAR('K')
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)                             PORT_CHAR(';')  PORT_CHAR(':')
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH)                         PORT_CHAR('\\') PORT_CHAR('|')
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0x85
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xb4
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xac - superimpose?
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_ESC)        PORT_NAME("Esc")       PORT_CHAR(27)

	PORT_START("lower.7")
		// 0x38-0x3f: shift < x b l / shift 0xb1
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_LSHIFT)     PORT_NAME("LShift")    PORT_CHAR(UCHAR_SHIFT_1)
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH2)                        PORT_CHAR('<')  PORT_CHAR('>')
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_X)                                 PORT_CHAR('x')  PORT_CHAR('X')
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_B)                                 PORT_CHAR('b')  PORT_CHAR('B')
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_L)                                 PORT_CHAR('l')  PORT_CHAR('L')
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH)                             PORT_CHAR('/')  PORT_CHAR('?')
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_RSHIFT)     PORT_NAME("RShift")    PORT_CHAR(UCHAR_MAMEKEY(RSHIFT))
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xb1

	PORT_START("lower.8")
		// 0x40-0x47: 0xb5 0xb6 control 0x81 z c n .
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xb5
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xb6
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_LCONTROL) PORT_CODE(KEYCODE_RCONTROL)
																				   PORT_NAME("Control")   PORT_CHAR(UCHAR_MAMEKEY(LCONTROL)) PORT_CHAR(UCHAR_MAMEKEY(RCONTROL))
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0x81
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z)                                 PORT_CHAR('z')  PORT_CHAR('Z')
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_C)                                 PORT_CHAR('c')  PORT_CHAR('C')
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_N)                                 PORT_CHAR('n')  PORT_CHAR('N')
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP)                              PORT_CHAR('.')

	PORT_START("lower.9")
		// 0x48-0x4f: none 0x83 0x84 0xb2 0xb3 0x00 0x82 lf
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD)                               PORT_NAME("Repeat")
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0x83
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0x84
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xb2
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xb3
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0x00
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0x82
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD)                               PORT_NAME("Line Feed") PORT_CHAR(10)

	PORT_START("lower.a")
		// 0x50-0x57: v m , space 0x80 0xb0 0xae 0x8d
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_V)                                 PORT_CHAR('v')  PORT_CHAR('V')
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_M)                                 PORT_CHAR('m')  PORT_CHAR('M')
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA)                             PORT_CHAR(',')
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_SPACE)      PORT_NAME("Space")     PORT_CHAR(' ')
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0x80
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xb0
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0xae
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) // 0x8d

INPUT_PORTS_END

INPUT_PORTS_START(lle_en_us_device)
	PORT_INCLUDE(interpro_en_us)
INPUT_PORTS_END

ROM_START(lle_en_us)
	ROM_REGION(0x800, "mcu", 0)
	ROM_LOAD("i8049ah.5", 0x0, 0x800, CRC(7b74f43b) SHA1(c43d41ac52df4d1282edc06f891cf27ef9255faa))

	ROM_REGION(0x1000, "eprom", 0)
	ROM_LOAD("sd03595.37", 0x0, 0x1000, CRC(263ed215) SHA1(4de550ff1eec7996c7f26e92c5d268aa24024a7f))
ROM_END

} // anonymous namespace

lle_device_base::lle_device_base(machine_config const &mconfig, device_type type, char const *tag, device_t *owner, u32 clock)
	: device_t(mconfig, type, tag, owner, clock)
	, device_interpro_keyboard_port_interface(mconfig, *this)
	, m_mcu(*this, "mcu")
	, m_ext(*this, "ext")
	, m_upper(*this, "upper.%x", 0)
	, m_lower(*this, "lower.%x", 0)
	, m_speaker(*this, "speaker")
	, m_leds(*this, "led_%u", 0U)
{
}
MACHINE_CONFIG_START(lle_device_base::device_add_mconfig)
	MCFG_DEVICE_ADD("mcu", I8049, 11_MHz_XTAL)
	MCFG_DEVICE_ADDRESS_MAP(AS_IO, io_map)
	MCFG_MCS48_PORT_T0_IN_CB(READLINE(*this, lle_device_base, t0_r))
	MCFG_MCS48_PORT_T1_IN_CB(READLINE(*this, lle_device_base, t1_r))
	MCFG_MCS48_PORT_P1_OUT_CB(WRITE8(*this, lle_device_base, p1_w))
	MCFG_MCS48_PORT_P2_OUT_CB(WRITE8(*this, lle_device_base, p2_w))
	MCFG_MCS48_PORT_BUS_IN_CB(READ8(*this, lle_device_base, bus_r))
	MCFG_MCS48_PORT_BUS_OUT_CB(WRITE8(*this, lle_device_base, bus_w))

	MCFG_DEVICE_ADD(m_ext, ADDRESS_MAP_BANK, 0)
	MCFG_DEVICE_PROGRAM_MAP(ext_map)
	MCFG_ADDRESS_MAP_BANK_DATA_WIDTH(8)
	MCFG_ADDRESS_MAP_BANK_ADDR_WIDTH(12)
	MCFG_ADDRESS_MAP_BANK_STRIDE(0x100)

	SPEAKER(config, "keyboard").front_center();
	MCFG_DEVICE_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "keyboard", 0.25)
MACHINE_CONFIG_END

void lle_device_base::device_start()
{
	m_leds.resolve();

	save_item(NAME(m_txd));
	save_item(NAME(m_p1));
	save_item(NAME(m_p2));
	save_item(NAME(m_bus));

	save_item(NAME(m_row));
	save_item(NAME(m_count));
}

void lle_device_base::device_reset()
{
	m_txd = 0;
}

void lle_device_base::io_map(address_map &map)
{
	map(0, 0xff).m(m_ext, FUNC(address_map_bank_device::amap8));
}

void lle_device_base::ext_map(address_map &map)
{
	map(0, 0xfff).rom().region("eprom", 0);

	// not clear what these addresses correspond to, possibly
	// used in manufacturer testing?
	if (VERBOSE & LOG_GENERAL)
		map(0x7fe, 0x7ff).lw8("write",
			[this](address_space &space, offs_t offset, u8 data, u8 mem_mask)
			{
				LOG("write offset 0x%03f data 0x%02x (%s)\n", offset, data, machine().describe_context());
			});
}

READ_LINE_MEMBER(lle_device_base::t0_r)
{
	if ((VERBOSE & LOG_RXTX) && (m_mcu->pc() == 0x8e) && m_txd == 0)
	{
		auto const suppressor(machine().disable_side_effects());

		address_space &mcu_ram(m_mcu->space(AS_DATA));
		const u8 input(mcu_ram.read_byte(0x42));

		LOGMASKED(LOG_RXTX, "received byte 0x%02x\n", input);
	}

	return m_txd;
}

READ_LINE_MEMBER(lle_device_base::t1_r)
{
	return BIT(m_lower[m_count >> 3]->read(), m_count & 0x7) ? ASSERT_LINE : CLEAR_LINE;
}

WRITE8_MEMBER(lle_device_base::p1_w)
{
	LOGMASKED(LOG_PORT, "p1_w 0x%02x (%s)\n", data, machine().describe_context());

	// speaker enable?
	if (!BIT(m_p1, 7) && BIT(data, 7))
		LOGMASKED(LOG_PORT, "p1.7 set (%s)\n", machine().describe_context());

	// update led state
	if (!BIT(m_p1, 6) && BIT(data, 6))
		for (int i = 0; i < 8; i++)
			m_leds[i] = !BIT(m_bus, i);

	// select upper row
	if (BIT(m_p1, 5) && !BIT(data, 5))
	{
		LOGMASKED(LOG_PORT, "select upper row 0x%01x\n", data & 0xf);

		m_row = data & 0xf;
	}

	// speaker output
	m_speaker->level_w(BIT(data, 4));

	m_p1 = data;
}

WRITE8_MEMBER(lle_device_base::p2_w)
{
	LOGMASKED(LOG_PORT, "p2_w 0x%02x (%s)\n", data, machine().describe_context());

	if ((VERBOSE & LOG_RXTX) && (m_mcu->pc() == 0x1d || m_mcu->pc() == 0x21))
	{
		auto const suppressor(machine().disable_side_effects());

		address_space &mcu_ram(m_mcu->space(AS_DATA));
		const u8 txd_state(mcu_ram.read_byte(0x1d));
		const u8 output(mcu_ram.read_byte(0x2d));

		if (txd_state == 0x20)
			LOGMASKED(LOG_RXTX, "transmitting byte 0x%02x\n", output);
	}

	// clear counter
	// FIXME: 0xfe is a strange init value, but makes the double-increment
	// that follows line up precisely with the make/break codes
	if (BIT(m_p2, 7) && !BIT(data, 7))
		m_count = 0xfe;

	// increment counter
	if (BIT(m_p2, 6) && !BIT(data, 6))
		m_count++;

	// serial transmit
	output_rxd(BIT(data, 5) ? CLEAR_LINE : ASSERT_LINE);

	// BIT(data, 4)?
	if (!BIT(m_p2, 4) && BIT(data, 4))
		LOG("p2.4 set (%s)\n", machine().describe_context());

	// bank select
	m_ext->set_bank(data & 0x0f);

	m_p2 = data;
}

READ8_MEMBER(lle_device_base::bus_r)
{
	if (!BIT(m_p1, 5))
	{
		LOGMASKED(LOG_PORT, "read upper row 0x%01x\n", m_row);

		return m_upper[m_row]->read();
	}
	else
		LOGMASKED(LOG_PORT, "bus_r (%s)\n", machine().describe_context());

	return 0xff;
}

WRITE8_MEMBER(lle_device_base::bus_w)
{
	if (data != 0xff)
		LOGMASKED(LOG_PORT, "bus_w 0x%02x (%s)\n", data, machine().describe_context());

	m_bus = data;
}

lle_en_us_device::lle_en_us_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: lle_device_base(mconfig, INTERPRO_LLE_EN_US_KEYBOARD, tag, owner, clock)
{
}

tiny_rom_entry const *lle_en_us_device::device_rom_region() const
{
	return ROM_NAME(lle_en_us);
}

ioport_constructor lle_en_us_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(lle_en_us_device);
}

} } } // namespace bus::interpro::keyboard
