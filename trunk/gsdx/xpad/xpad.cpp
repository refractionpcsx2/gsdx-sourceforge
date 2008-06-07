/* 
 *	Copyright (C) 2007 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "xpad.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//
//	Note!
//
//		If this DLL is dynamically linked against the MFC
//		DLLs, any functions exported from this DLL which
//		call into MFC must have the AFX_MANAGE_STATE macro
//		added at the very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the 
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//

BEGIN_MESSAGE_MAP(xpadApp, CWinApp)
END_MESSAGE_MAP()

xpadApp::xpadApp()
{
}

xpadApp theApp;

BOOL xpadApp::InitInstance()
{
	__super::InitInstance();

	SetRegistryKey(_T("Gabest"));

	return TRUE;
}

//

#define PS2E_LT_PAD 0x02
#define PS2E_PAD_VERSION 0x0002

EXPORT_C_(UINT32) PS2EgetLibType()
{
	return PS2E_LT_PAD;
}

EXPORT_C_(char*) PS2EgetLibName()
{
	return "XPad";
}

EXPORT_C_(UINT32) PS2EgetLibVersion2(UINT32 type)
{
	const UINT32 revision = 0;
	const UINT32 build = 1;
	const UINT32 minor = 0;

	return (build << 0) | (revision << 8) | (PS2E_PAD_VERSION << 16) | (minor << 24);
}

//

struct XPadButton
{
	enum 
	{
		Select = 0x0001,
		L3 = 0x0002,
		R3 = 0x0004,
		Start = 0x0008,
		Up = 0x0010,
		Down = 0x0020,
		Right = 0x0040,
		Left = 0x0080,
		L2 = 0x0100,
		R2 = 0x0200,
		L1 = 0x0400,
		R1 = 0x0800,
		Triangle = 0x1000,
		Circle = 0x2000,
		Cross = 0x4000,
		Square = 0x8000,
	};
};

class XPad
{
public:
	int m_pad;
	bool m_connected;
	bool m_ds2native;
	bool m_analog;
	bool m_locked;
	bool m_vibration;
	WORD m_buttons;
	struct {BYTE x, y;} m_left;
	struct {BYTE x, y;} m_right;
	XINPUT_STATE m_state;

	void SetButton(WORD buttons, WORD mask, int flag)
	{
		if(buttons & mask)
		{
			m_buttons &= ~flag;
		}
		else 
		{
			m_buttons |= flag;
		}
	}

	void SetAnalog(short src, BYTE& dst, short deadzone)
	{
		if(abs(src) < deadzone) src = 0;

		dst = (src >> 8) + 128;
	}

public:
	XPad(int pad) 
		: m_pad(pad)
		, m_connected(false)
		, m_ds2native(true)
		, m_analog(true)
		, m_locked(false)
		, m_vibration(true)
		, m_buttons(0xffff)
	{
	}

	virtual ~XPad()
	{
	}

	BYTE GetId()
	{
		return m_analog ? (m_ds2native ? 0x79 : 0x71) : 0x41;
	}

	BYTE ReadData(int index)
	{
		if(index == 0)
		{
			memset(&m_state, 0, sizeof(m_state));

			m_connected = SUCCEEDED(XInputGetState(m_pad, &m_state));

			if(m_connected)
			{
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_BACK, XPadButton::Select);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_LEFT_THUMB, XPadButton::L3);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_RIGHT_THUMB, XPadButton::R3);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_START, XPadButton::Start);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_UP, XPadButton::Up);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_DOWN, XPadButton::Down);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_RIGHT, XPadButton::Right);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_LEFT, XPadButton::Left);
				SetButton(m_state.Gamepad.bLeftTrigger, 0xe0, XPadButton::L2);
				SetButton(m_state.Gamepad.bRightTrigger, 0xe0, XPadButton::R2);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER, XPadButton::L1);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER, XPadButton::R1);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_Y, XPadButton::Triangle);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_B, XPadButton::Circle);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_A, XPadButton::Cross);
				SetButton(m_state.Gamepad.wButtons, XINPUT_GAMEPAD_X, XPadButton::Square);

				SetAnalog(m_state.Gamepad.sThumbLX, m_left.x, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
				SetAnalog(m_state.Gamepad.sThumbLY, m_left.y, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
				SetAnalog(m_state.Gamepad.sThumbRX, m_right.x, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
				SetAnalog(m_state.Gamepad.sThumbRY, m_right.y, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
			}
			else
			{
				m_buttons = 0xffff;
				m_left.x = 128;
				m_left.y = 128;
				m_right.x = 128;
				m_right.y = 128;
			}
		}

		switch(index)
		{
		case 0:
			return (BYTE)(m_buttons & 0xff);
		case 1:
			return (BYTE)(m_buttons >> 8);
		case 2:
			return m_right.x;
		case 3:
			return 0xff - m_right.y;
		case 4:
			return m_left.x;
		case 5:
			return 0xff - m_left.y;
		}

		return 0xff;
	}
};

static class XPadPlugin
{
	CAtlArray<XPad*> m_pads;
	XPad* m_pad;
	int m_index;
	bool m_cfgreaddata;

	typedef BYTE (XPadPlugin::*CommandHandler)(int, BYTE);

	CommandHandler m_handlers[256];
	CommandHandler m_handler;

	BYTE QueryDS2AnalogMode(int index, BYTE value)
	{
		if(m_pad->m_ds2native)
		{
			ASSERT(value == 'Z');

			switch(index)
			{
			case 0: 
				return 0xff;
			case 1: 
				return 0xff;
			case 2: 
				return 3;
			case 3: 
				return 0;
			case 4: 
				return 0;
			case 5: 
				return 'Z';
			}
		}

		return 0;
	}

	BYTE ReadDataAndVibrate(int index, BYTE value)
	{
		switch(index)
		{
		case 0: 
			// TODO: value = 0/1 => small motor on/off
			break;
		case 1: 
			// TODO: value = 0->255 => large motor speed
			break;
		}

		return m_pad->ReadData(index);
	}

	BYTE ConfigMode(int index, BYTE value)
	{
		switch(index)
		{
		case 0: 
			switch(value)
			{
			case 0:
				m_cfgreaddata = false;
				break;
			case 1:
				m_cfgreaddata = true;
				break;
			}
			break;
		}

		if(m_cfgreaddata)
		{
			return m_pad->ReadData(index);
		}

		return 0;
	}

	BYTE SetModeAndLock(int index, BYTE value)
	{
		switch(index)
		{
		case 0: 
			// if(!m_pad->m_locked) ? 
			m_pad->m_analog = value == 1;
			break;
		case 1: 
			m_pad->m_locked = value == 3;
			break;
		}

		return 0;
	}

	BYTE QueryModelAndMode(int index, BYTE value)
	{
		switch(index)
		{
		case 0: 
			return 1;
		case 1: 
			return 2;
		case 2:
			return m_pad->m_analog ? 1 : 0;
		case 3:
			return m_pad->m_ds2native ? 1 : 2;
		case 4:
			return 1;
		}

		return 0;
	}

	BYTE ConfigVibration(int index, BYTE value)
	{
		switch(index)
		{
		case 0: 
			return value;
		case 1: 
			m_pad->m_vibration = value == 1;
			return value;
		}

		return 0xff;
	}

	BYTE SetDS2NativeMode(int index, BYTE value)
	{
		switch(index)
		{
		case 5: 
			m_pad->m_ds2native = true;
			return 'Z';
		}

		return 0;
	}

	BYTE UnknownCommand(int index, BYTE value)
	{
		TRACE(_T("Unknown command (%08x)\n"), value);

		return 0;
	}

public:

	XPadPlugin()
		: m_pad(NULL)
		, m_index(-1)
		, m_cfgreaddata(false)
		, m_handler(NULL)
	{
		m_pads.Add(new XPad(0));
		m_pads.Add(new XPad(1));

		for(int i = 0; i < countof(m_handlers); i++)
		{
			m_handlers[i] = &XPadPlugin::UnknownCommand;
		}

		m_handlers['A'] = &XPadPlugin::QueryDS2AnalogMode;
		m_handlers['B'] = &XPadPlugin::ReadDataAndVibrate;
		m_handlers['C'] = &XPadPlugin::ConfigMode;
		m_handlers['D'] = &XPadPlugin::SetModeAndLock;
		m_handlers['E'] = &XPadPlugin::QueryModelAndMode;
		m_handlers['M'] = &XPadPlugin::ConfigVibration;
		m_handlers['O'] = &XPadPlugin::SetDS2NativeMode;
	}

	void StartPoll(int pad)
	{
		m_pad = m_pads[pad & 1];
		m_index = 0;
	}

	BYTE Poll(BYTE value)
	{
		m_index++;

		switch(m_index)
		{
		case 1:
			m_handler = m_handlers[value];
			return m_pad->GetId();
		case 2:
			ASSERT(value == 0);
			return 'Z';
		default:
			return (this->*m_handler)(m_index - 3, value);
		}
	}

} s_padplugin;

//

EXPORT_C_(UINT32) PADinit(UINT32 flags)
{
	return 0;
}

EXPORT_C PADshutdown()
{
}

EXPORT_C_(UINT32) PADopen(HWND hWnd)
{
	XInputEnable(TRUE);

	return 0;
}

EXPORT_C PADclose()
{
	XInputEnable(FALSE);
}

EXPORT_C_(UINT32) CALLBACK PADquery()
{
	return 3;
}

EXPORT_C_(BYTE) PADstartPoll(int pad)
{
	s_padplugin.StartPoll(pad - 1);

	return 0xff;
}

EXPORT_C_(BYTE) PADpoll(BYTE value)
{
	return s_padplugin.Poll(value);
}

EXPORT_C_(KeyEvent*) PADkeyEvent()
{
	return NULL;
}

EXPORT_C PADconfigure()
{
}

EXPORT_C PADabout()
{
}

EXPORT_C_(UINT32) PADtest()
{
	return 0;
}