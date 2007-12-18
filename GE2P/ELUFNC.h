#ifndef _ELUFNC_H_INCLUDED
#define _ELUFNC_H_INCLUDED
	
enum ELU28_FNC_INPUT
{
    FNC_DASL_LOOP_SYNC      = 0x51, // DASL LOOP SYNC status (1= SyncOK, 0= SyncLost)
    FNC_XMIT_FLOW_CONTROL   = 0x52, // DASL Transmit Queue flow control (1= Xon, 0= Xoff)
    FNC_RESET_REMOTE_DTSS   = 0x53, // Request Reset all DTSs connected to remote GW
    FNC_KILL_REMOTE_IDODS   = 0x54, // Request Kill of all remote IDOD servers
    FNC_VR_VAD              = 0x55, // Voice Recording Voice Activity Detection

//  Signal name               FNC      Description
//
    FNC_EQUACT              = 0x80, // Activate equipment, set LED- and tone ringer cadences.
    FNC_EQUSTAREQ           = 0x81, // DTS status request
    FNC_CLOCKCORRECT        = 0x82, // Adjustment of real time clock
    //                      = 0x83, // Not used
    FNC_TRANSMISSION        = 0x84, // Transmission order
    FNC_CLEARDISPLAY        = 0x85, // Clear display completely
    FNC_CLEARDISPLAYFIELD   = 0x86, // Clear display field (10 char)
    //                      = 0x87, // Not used
    FNC_INTERNRINGING       = 0x88, // Internal ringing
    FNC_INTERNRINGING1LOW   = 0x89, // Internal ringing, 1 period low
    FNC_EXTERNRINGING       = 0x8A, // External ringing
    FNC_EXTERNRINGING1LOW   = 0x8B, // External ringing, 1 period low
    FNC_CALLBACKRINGING     = 0x8C, // Call Back ringing
    FNC_CALLBACKRINGING1LOW = 0x8D, // Call Back ringing, 1 period low
    FNC_STOPRINGING         = 0x8E, // Stop ringing
    FNC_CLEARLEDS           = 0x8F, // Clear all LED indicators
    FNC_CLEARLED            = 0x90, // Clear one indicator
    FNC_SETLED              = 0x91, // Set indicator
    FNC_FLASHLEDCAD0        = 0x92, // Flash indicator with cadence 0
    FNC_FLASHLEDCAD1        = 0x93, // Flash indicator with cadence 1
    FNC_FLASHLEDCAD2        = 0x94, // Flash indicator with cadence 2
    FNC_WRITEDISPLAYFIELD   = 0x95, // Write in display field (10 char)
    FNC_RNGCHRUPDATE        = 0x96, // Update ringing tone character
    FNC_STOPWATCH           = 0x97, // Control local stopwatch
    FNC_DISSPECCHR          = 0x98, // Definition of special display character
    FNC_FLASHDISPLAYCHR     = 0x99, // Activate blinking character string in display
    FNC_ACTCLOCK            = 0x9A, // Activate clock function
    FNC_RELPRGFNCREQ2       = 0x9B, // Release programmable function key request
    FNC_RELFIXFNCREQ2       = 0x9C, // Release fixed function key request
    FNC_ACTCURSOR           = 0x9D, // Activate blinking cursor
    FNC_CLEARCURSOR         = 0x9E, // Clear cursor from display
    FNC_FIXFLASHCHR         = 0x9F, // Change blinking character string to fixed presentation
    FNC_A0                  = 0xA0, // Must not be sent to DBC600
    FNC_A1                  = 0xA1, // CTT10: Login successfull; Must not be sent to DBC600
    FNC_A2                  = 0xA2, // CTT10: Login error
    FNC_A3                  = 0xA3, // CTT10: Logout
    FNC_A4                  = 0xA4, // CTT10: Logout by system
    FNC_MMUMESDIVUPD        = 0xA2, // TAU-D: Message Diversion update  
    FNC_MMURES              = 0xA3, // TAU-D: Response to Query  
    FNC_MMURS               = 0xA4, // TAU-D: Response to Set Feature  
    FNC_MMUNRUPDAT          = 0xA5, // TAU-D: Number update  
    FNC_MMUCALSTA           = 0xA6, // TAU-D: Call state update  
    FNC_RNGLVLUPDATE        = 0xA7, // Ringing level update
    FNC_TRANSLVLUPDATE      = 0xA8, // Transmission level update
    FNC_IO_SETUP            = 0xA9, // Update I/O connection
    FNC_FLASHDISPLAYCHR2    = 0xAA, // Activate blinking character string in display 2
    FNC_TRANSCODEUPDATE     = 0xAB, // Update transmissions coding principle
    FNC_EXTERNUNIT1UPDATE   = 0xAC, // Update extern unit 1
    FNC_EXTERNUNIT2UPDATE   = 0xAD, // Update extern unit 2
    FNC_WRITEDISPLAYCHR     = 0xAE, // Write characters in display
    FNC_ACTDISPLAYLEVEL     = 0xAF, // Activate display level
    FNC_ANTICLIPPINGUPDATE  = 0xB0, // Update Anti clipping level
    FNC_EXTRARNGUPDATE      = 0xB1, // Extra ringing cadences update
    FNC_EXTRARINGING        = 0xB2, // Extra ringing
    FNC_EXTRARINGING1LOW    = 0xB3, // Extra ringing, 1 period low
    FNC_HEADSETUPDATE       = 0xB4, // Headset update
    FNC_HANDSFREEPARAMETER  = 0xB5, // Handsfree parameter update
    //   0xB6 - 0xCF                   Not Used
    FNC_EQUSTAD4REQ         = 0xD0, // Request for D4 equipment state
    FNC_CLEARDISPLAYGR      = 0xD1, // Clear graphic display
    FNC_WRITEDISPLAYGR      = 0xD2, // Write graphic display
    FNC_STOPWATCHGR         = 0xD3, // Stop watch control
    FNC_CLOCKORGR           = 0xD4, // Adjust local real time clock
    FNC_ACTCURSORGR         = 0xD5, // Active blinking cursor
    FNC_HWICONSGR           = 0xD6, // Active/delete HWICONs
    FNC_DRAWGR              = 0xD7, // Draw line and rectangle
    FNC_LISTDATA            = 0xD8, // Lista data
    FNC_SCROLLLIST          = 0xD9, // Scroll list options
    FNC_TOPMENUDATA         = 0xDA, // Top menu data
    FNC_SCROLLTOPMENU       = 0xDB, // Scroll top menu
    FNC_SCROLLSUBMENU       = 0xDC, // Scroll submenu
    //   0xDD - 0xDF                   Not used
    FNC_EQULOOP             = 0xE0, // Transceiver loop order
    FNC_EQUTESTREQ          = 0xE4, // Test function request, see Test code below
    FNC_NULLORDER           = 0xFF  // Null order
    };

enum ELU28_FNC_OUTPUT
{
//  Signal name               FNC      Description
//
    FNC_EQUSTA              = 0x01, // Status and identity response
    FNC_PRGFNCACT           = 0x02, // Programmable function key pushed
    //                      = 0x03, // Not used
    FNC_FIXFNCACT           = 0x04, // Fixed function key pushed
    FNC_PRGFNCREL2          = 0x05, // Programmable function key released
    FNC_FIXFNCREL2          = 0x06, // Fixed function key released
    FNC_EQULOCALTST         = 0x07, // Change to/from local test mode
    FNC_EXTERNUNIT          = 0x08, // Extern unit response
    FNC_STOPWATCHREADY      = 0x09, // Stopwatch ready counting down
    FNC_MMEFNCACT           = 0x0A, // TAU-D: MMU Request of information  
    FNC_HFPARAMETERRESP     = 0x0B, // with trailing 0x01: Handsfree parameter response
    //                      = 0x0C-0x11 H Not used
    FNC_EQUTESTRES          = 0x12  // Test status response, see Test code below
    };

enum ELU28_FNC_TEST_CODE
{
    FNC_TEST_RESET          = 0x00,
    FNC_TEST_ERROR_STATUS   = 0x7F
    };

/* EQUTESTREQ test codes:

 -> Test code = 0x00: Reset and local test request
    Test code = 0x01: Read RAM/ROM request
    Test code = 0x03: Write RAM request
    Test code = 0x06: ROM article number, rev state request
    Test code = 0x0C: Request hardware revision info
    Test code = 0x0D: Request firmware version & state info
    Test code = 0x0E: Initiate teminal firmware download
    Test code = 0x0F: Firmware data transfer
    Test code = 0x10: Firmware download terminate
 -> Test code = 0x7F: Local error status request
    Test code = 0x80: Send hardware revision request
    Test code = 0x81: Start keyboard test request
    Test code = 0x82: Send keyboard test result request
    Test code = 0x83: Start LED test request
    Test code = 0x84: Starts bit error test request
    Test code = 0x85: Send data from the instrument request
    Test code = 0x86: Turn on/off the acknowledge from the instrument request.
    Test code = 0x87: Local mode request
    Test code = 0x88: Set equipment version request
    Test code = 0x89: Set hardware revision status request
    Test code = 0x8A: Write in the EEPROM request
    Test code = 0x8B: Read in the EEPROM request
    Test code = 0x8C: Set Installation date request
    Test code = 0x8D: Read Installation date request
    Test code = 0x8E: Turn on/off poll and acknowledge to Extern unit
    Test code = 0x8F: Send data to Extern unit
    Test code = 0x90: Synchronise LED cadences with DSS.
    Test code = 0x91: Read peripheral register
    Test code = 0x92: Write peripheral register
    Test code = 0x93: OPU programming (local)
    Test code = 0x94: Melody Programming (local)
    Test code = 0x95: Headset Priority (local)
    Test code = 0x96: LCD contrast adjust(local)
    Test code = 0x97: FW indication(local)
    Test code = 0x98: Read out font version (local)
    Test code = 0x99: Hearing Level amplification(local)
    Test code = 0xBA: Write HW test Pass/Fail
    Test code = 0xBB: Read out HW test Pass/Fail
    Test code = 0xBC: Write HW serial number
    Test code = 0xBD: Read out HW serial number
    Test code = 0xBE: Read out font version in the phone
    Test code = 0xBF: Go back to latest R version
*/
/* EQUTESTRES test codes:

 -> Test code = 0x01: RAM/ROM data read response
    Test code = 0x06: OM article number and rev state
    Test code = 0x0C: Response hardware info(ROA num and rev)
    Test code = 0x0D: Response firmware version & state info
    Test code = 0x0E: Response Initiate Download
    Test code = 0x0F: Response Data transfer
    Test code = 0x10: Response Download terminate
 -> Test code = 0x7F: Error status response
    Test code = 0x80: Hardware revision
    Test code = 0x82: Keyboard test result
    Test code = 0x84: Bit error test result
    Test code = 0x8B: EEPROM data read response
    Test code = 0x8D: Read Installation date response
    Test code = 0x91: Read peripheral register response
    Test code = 0xBB: Read factory HW test result
*/

enum DTS_KeyCode
{
    // Fixed function keys
    //
    KEY_FIXFN_0          = 0x000,
    KEY_FIXFN_1          = 0x001,
    KEY_FIXFN_2          = 0x002,
    KEY_FIXFN_3          = 0x003,
    KEY_FIXFN_4          = 0x004,
    KEY_FIXFN_5          = 0x005,
    KEY_FIXFN_6          = 0x006,
    KEY_FIXFN_7          = 0x007,
    KEY_FIXFN_8          = 0x008,
    KEY_FIXFN_9          = 0x009,
    KEY_FIXFN_STAR       = 0x00A,
    KEY_FIXFN_HASH       = 0x00B,
    KEY_FIXFN_TRANSFER   = 0x00C,
    KEY_FIXFN_CLEAR      = 0x00D,
    KEY_FIXFN_LINE       = 0x00E,
    KEY_FIXFN_ON_HOOK    = 0x00F,
    KEY_FIXFN_OFF_HOOK   = 0x010,
    KEY_FIXFN_SPK_PLUS   = 0x011,
    KEY_FIXFN_SPK_MINUS  = 0x012,
	//
    // Program function keys
    //
    KEY_PRGFN_PROGRAM    = 0x100,
    KEY_PRGFN_MENU       = 0x108,
    KEY_PRGFN_ACCESS1    = 0x10B,
    KEY_PRGFN_ACCESS2    = 0x10A,
    KEY_PRGFN_INQUIRY    = 0x109,

    KEY_PRGFN_MENU1      = 0x104,
    KEY_PRGFN_MENU2      = 0x105,
    KEY_PRGFN_MENU3      = 0x106,
    KEY_PRGFN_MENU4      = 0x107,

    KEY_PRGFN_KEY1       = 0x101,
    KEY_PRGFN_KEY2       = 0x102,
    KEY_PRGFN_KEY3       = 0x103,
    KEY_PRGFN_KEY4       = 0x10E,
    KEY_PRGFN_KEY5       = 0x10D,
    KEY_PRGFN_KEY11      = 0x10F,
    KEY_PRGFN_KEY12      = 0x110,
    KEY_PRGFN_KEY13      = 0x111,
    KEY_PRGFN_KEY14      = 0x112,
    KEY_PRGFN_KEY15      = 0x113,
    KEY_PRGFN_KEY16      = 0x114,
    KEY_PRGFN_KEY17      = 0x115,
    KEY_PRGFN_KEY18      = 0x116,
    KEY_PRGFN_KEY19      = 0x117
    };

enum DTS_TransmissionCode
{
    DTSTRANS_OFF                = 0x00,
    DTSTRANS_HANDSET_SPEAKING   = 0x01,
    DTSTRANS_HANDSFREE_SPEAKING = 0x02,
    DTSTRANS_NO_CHANGE          = 0x03,
    DTSTRANS_PUBLIC_ADDRESS     = 0x04,
    DTSTRANS_PUBLIC_SPEAKING    = 0x05,
    DTSTRANS_HEADSET_SPEAKING   = 0x06
    };

#endif // _ELUFNC_H_INCLUDED
