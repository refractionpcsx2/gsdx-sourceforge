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

#include "StdAfx.h"
#include "GPUDrawScanline.h"

GPUDrawScanline::GPUDrawScanline(GPUState* state, int filter, int dither)
	: m_state(state)
	, m_filter(filter)
	, m_dither(dither)
{
	Init();
}

GPUDrawScanline::~GPUDrawScanline()
{
}

void GPUDrawScanline::SetOptions(int filter, int dither)
{
	m_filter = filter;
	m_dither = dither;
}

// IDrawScanline

void GPUDrawScanline::SetupDraw(Vertex* vertices, int count, const void* texture)
{
	GPUDrawingEnvironment& env = m_state->m_env;

	// m_sel

	m_sel.dw = 0;
	m_sel.iip = env.PRIM.IIP;
	m_sel.me = env.STATUS.ME;
	m_sel.abe = env.PRIM.ABE;
	m_sel.abr = env.STATUS.ABR;
	m_sel.tge = env.PRIM.TGE;
	m_sel.tme = env.PRIM.TME;
	m_sel.tlu = env.STATUS.TP < 2;
	m_sel.twin = (env.TWIN.ai32 & 0xfffff) != 0;
	m_sel.dtd = m_dither ? env.STATUS.DTD : 0;
	m_sel.ltf = m_filter < 2 ? m_filter : env.PRIM.TYPE == GPU_POLYGON ? 1 : 0;

	m_dsf = m_ds[m_sel];

	// m_slenv

	m_slenv.mem = &m_state->m_mem;

	if(m_sel.tme)
	{
		m_slenv.tex = texture;
		m_slenv.clut = m_state->m_mem.GetCLUT(env.STATUS.TP, env.CLUT.X, env.CLUT.Y);

		if(m_sel.twin)
		{
			DWORD u, v;

			u = ~(env.TWIN.TWW << 3) & 0xff;
			v = ~(env.TWIN.TWH << 3) & 0xff;

			m_slenv.u[0] = GSVector4i((u << 16) | u);
			m_slenv.v[0] = GSVector4i((v << 16) | v);

			u = env.TWIN.TWX << 3;
			v = env.TWIN.TWY << 3;
			
			m_slenv.u[1] = GSVector4i((u << 16) | u);
			m_slenv.v[1] = GSVector4i((v << 16) | v);
		}
	}

	m_slenv.a = GSVector4i(env.PRIM.ABE ? 0xffffffff : 0);
	m_slenv.md = GSVector4i(env.STATUS.MD ? 0x80008000 : 0);
}

void GPUDrawScanline::SetupScanline(const Vertex& dv)
{
	GSVector4 ps0123 = GSVector4::ps0123();
	GSVector4 ps4567 = GSVector4::ps4567();

	GSVector4 dt = dv.t;

	m_slenv.ds[0] = dt.xxxx() * ps0123;
	m_slenv.dt[0] = dt.yyyy() * ps0123;
	m_slenv.ds[1] = dt.xxxx() * ps4567;
	m_slenv.dt[1] = dt.yyyy() * ps4567;
	m_slenv.ds[2] = dt.xxxx() * 8.0f;
	m_slenv.dt[2] = dt.yyyy() * 8.0f;

	GSVector4i dc;

	dc = GSVector4i(dv.c);
	dc = dc.ps32();
	dc = dc.upl16(dc);

	GSVector4i s_01234567(0x00010000, 0x00030002, 0x00050004, 0x00070006); // TODO

	m_slenv.dr = dc.xxxx().mul16l(s_01234567);
	m_slenv.dg = dc.yyyy().mul16l(s_01234567);
	m_slenv.db = dc.zzzz().mul16l(s_01234567);
	m_slenv.dc = dc.sll16(3);
}

void GPUDrawScanline::DrawScanline(int top, int left, int right, const Vertex& v)	
{
	(this->*m_dsf)(top, left, right, v);
}

void GPUDrawScanline::FillRect(const GSVector4i& r, const Vertex& v)
{
	// TODO

	ASSERT(0);
}

IDrawScanline::DrawScanlinePtr GPUDrawScanline::GetDrawScanlinePtr()
{
	return m_dsf;
}

void GPUDrawScanline::SampleTexture(int pixels, DWORD ltf, DWORD tlu, DWORD twin, GSVector4i& test, const GSVector4* s, const GSVector4* t, GSVector4i* c)
{
	const void* RESTRICT tex = m_slenv.tex;
	const WORD* RESTRICT clut = m_slenv.clut;

	if(ltf)
	{
		GSVector4i cc[8];

		for(int j = 0; j < 2; j++)
		{
			GSVector4 ss = s[j] - 0.25f;
			GSVector4 tt = t[j] - 0.25f;

			GSVector4 uf = ss.floor();
			GSVector4 vf = tt.floor();

			GSVector4 uff = ss - uf;
			GSVector4 vff = tt - vf;

			GSVector4i u = GSVector4i(uf);
			GSVector4i v = GSVector4i(vf);
		
			GSVector4i u01 = GSVector4i(u).ps32(u + GSVector4i::x00000001());
			GSVector4i v01 = GSVector4i(v).ps32(v + GSVector4i::x00000001());

			if(twin)
			{
				u01 = (u01 & m_slenv.u[0]).add16(m_slenv.u[1]);
				v01 = (v01 & m_slenv.v[0]).add16(m_slenv.v[1]);
			}

			GSVector4i uv01 = u01.pu16(v01);

			GSVector4i addr0011 = uv01.upl8(uv01.zwxy());
			GSVector4i addr0110 = uv01.upl8(uv01.wzyx());

			GSVector4i c0011, c0110;

			#if _M_SSE >= 0x401

			if(tlu)
			{
				c0011 = addr0011.gather16_16((const BYTE*)tex).gather16_16(clut);
				c0110 = addr0110.gather16_16((const BYTE*)tex).gather16_16(clut);
			}
			else
			{
				c0011 = addr0011.gather16_16((const WORD*)tex);
				c0110 = addr0110.gather16_16((const WORD*)tex);
			}

			#else

			int i = 0;

			if(tlu)
			{
				do
				{
					c0011.u16[i] = clut[((const BYTE*)tex)[addr0011.u16[i]]];
					c0110.u16[i] = clut[((const BYTE*)tex)[addr0110.u16[i]]];
				}
				while(++i < 8);
			}
			else
			{
				do
				{
					c0011.u16[i] = ((const WORD*)tex)[addr0011.u16[i]];
					c0110.u16[i] = ((const WORD*)tex)[addr0110.u16[i]];
				}
				while(++i < 8);
			}

			#endif

			GSVector4i r0011 = GSVector4i(c0011 & 0x001f001f) << 3;
			GSVector4i r0110 = GSVector4i(c0110 & 0x001f001f) << 3;

			GSVector4 r00 = GSVector4(r0011.upl16());
			GSVector4 r01 = GSVector4(r0110.upl16());
			GSVector4 r10 = GSVector4(r0110.uph16());
			GSVector4 r11 = GSVector4(r0011.uph16());

			r00 = r00.lerp(r01, vff);
			r10 = r10.lerp(r11, vff);
			r00 = r00.lerp(r10, uff);

			cc[j * 4 + 0] = GSVector4i(r00);

			GSVector4i g0011 = GSVector4i(c0011 & 0x03e003e0) >> 2;
			GSVector4i g0110 = GSVector4i(c0110 & 0x03e003e0) >> 2;

			GSVector4 g00 = GSVector4(g0011.upl16());
			GSVector4 g01 = GSVector4(g0110.upl16());
			GSVector4 g10 = GSVector4(g0110.uph16());
			GSVector4 g11 = GSVector4(g0011.uph16());

			g00 = g00.lerp(g01, vff);
			g10 = g10.lerp(g11, vff);
			g00 = g00.lerp(g10, uff);

			cc[j * 4 + 1] = GSVector4i(g00);

			GSVector4i b0011 = GSVector4i(c0011 & 0x7c007c00) >> 7;
			GSVector4i b0110 = GSVector4i(c0110 & 0x7c007c00) >> 7;

			GSVector4 b00 = GSVector4(b0011.upl16());
			GSVector4 b01 = GSVector4(b0110.upl16());
			GSVector4 b10 = GSVector4(b0110.uph16());
			GSVector4 b11 = GSVector4(b0011.uph16());

			b00 = b00.lerp(b01, vff);
			b10 = b10.lerp(b11, vff);
			b00 = b00.lerp(b10, uff);

			cc[j * 4 + 2] = GSVector4i(b00);

			GSVector4i a0011 = GSVector4i(c0011 & 0x80008000);
			GSVector4i a0110 = GSVector4i(c0110 & 0x80008000);

			GSVector4 a00 = GSVector4(a0011.upl16());
			GSVector4 a01 = GSVector4(a0110.upl16());
			GSVector4 a10 = GSVector4(a0110.uph16());
			GSVector4 a11 = GSVector4(a0011.uph16());

			a00 = a00.lerp(a01, vff);
			a10 = a10.lerp(a11, vff);
			a00 = a00.lerp(a10, uff);

			cc[j * 4 + 3] = GSVector4i(a00);
		}

		c[0] = cc[0].ps32(cc[4]);
		c[1] = cc[1].ps32(cc[5]);
		c[2] = cc[2].ps32(cc[6]);
		c[3] = cc[3].ps32(cc[7]).gt16(GSVector4i::zero());

		// mask out blank pixels (not perfect)

		test |= 
			c[0].eq16(GSVector4i::zero()) & 
			c[1].eq16(GSVector4i::zero()) &
			c[2].eq16(GSVector4i::zero()) & 
			c[3].eq16(GSVector4i::zero());
	}
	else
	{
		GSVector4i u, v;
		
		u = GSVector4i(s[0]).ps32(GSVector4i(s[1]));
		v = GSVector4i(t[0]).ps32(GSVector4i(t[1]));

		if(twin)
		{
			u = (u & m_slenv.u[0]).add16(m_slenv.u[1]);
			v = (v & m_slenv.v[0]).add16(m_slenv.v[1]);
		}

		GSVector4i uv = u.pu16(v);

		GSVector4i addr = uv.upl8(uv.zwxy());

		GSVector4i c00;

		#if _M_SSE >= 0x401

		if(tlu)
		{
			c00 = addr.gather16_16((const BYTE*)tex).gather16_16(clut);
		}
		else
		{
			c00 = addr.gather16_16((const WORD*)tex);
		}

		#else

		int i = 0;

		if(tlu)
		{
			do
			{
				if(test.u16[i]) // me && 
				{
					continue;
				}

				c00.u16[i] = clut[((const BYTE*)tex)[addr.u16[i]]];
			}
			while(++i < pixels);
		}
		else
		{
			do
			{
				if(test.u16[i]) // me && 
				{
					continue;
				}

				c00.u16[i] = ((const WORD*)tex)[addr.u16[i]];
			}
			while(++i < pixels);
		}

		#endif

		test |= c00.eq16(GSVector4i::zero()); // mask out blank pixels

		c[0] = (c00 & 0x001f001f) << 3;
		c[1] = (c00 & 0x03e003e0) >> 2;
		c[2] = (c00 & 0x7c007c00) >> 7;
		c[3] = c00.sra16(15);
	}
}

void GPUDrawScanline::ColorTFX(DWORD tfx, const GSVector4i& r, const GSVector4i& g, const GSVector4i& b, GSVector4i* c)
{
	switch(tfx)
	{
	case 0: // none (tfx = 0)
	case 1: // none (tfx = tge)
		c[0] = r.srl16(7);
		c[1] = g.srl16(7);
		c[2] = b.srl16(7);
		break;
	case 2: // modulate (tfx = tme | tge)
		c[0] = c[0].sll16(2).mul16hu(r).pu16().upl8();
		c[1] = c[1].sll16(2).mul16hu(g).pu16().upl8();
		c[2] = c[2].sll16(2).mul16hu(b).pu16().upl8();
		break;
	case 3: // decal (tfx = tme)
		break;
	default:
		__assume(0);
	}
}
void GPUDrawScanline::AlphaBlend(UINT32 abr, UINT32 tme, const GSVector4i& d, GSVector4i* c)
{
	GSVector4i r = (d & 0x001f001f) << 3;
	GSVector4i g = (d & 0x03e003e0) >> 2;
	GSVector4i b = (d & 0x7c007c00) >> 7;

	switch(abr)
	{
	case 0:
		r = r.avg8(c[0]);
		g = g.avg8(c[0]);
		b = b.avg8(c[0]);
		break;
	case 1:
		r = r.addus8(c[0]);
		g = g.addus8(c[1]);
		b = b.addus8(c[2]);
		break;
	case 2:
		r = r.subus8(c[0]);
		g = g.subus8(c[1]);
		b = b.subus8(c[2]);
		break;
	case 3:
		r = r.addus8(c[0].srl16(2));
		g = g.addus8(c[1].srl16(2));
		b = b.addus8(c[2].srl16(2));
		break;
	default:
		__assume(0);
	}

	if(tme) // per pixel
	{
		c[0] = c[0].blend8(r, c[3]);
		c[1] = c[1].blend8(g, c[3]);
		c[2] = c[2].blend8(b, c[3]);
	}
	else
	{
		c[0] = r;
		c[1] = g;
		c[2] = b;
		c[3] = GSVector4i::zero();
	}
}

void GPUDrawScanline::WriteFrame(WORD* RESTRICT fb, const GSVector4i& test, const GSVector4i* c, int pixels)
{
	GSVector4i r = (c[0] & 0x00f800f8) >> 3;
	GSVector4i g = (c[1] & 0x00f800f8) << 2;
	GSVector4i b = (c[2] & 0x00f800f8) << 7;
	GSVector4i a = (c[3] & 0x00800080) << 8;

	GSVector4i s = r | g | b | a | m_slenv.md;

	int i = 0;

	do
	{
		if(test.u16[i] == 0)
		{
			fb[i] = s.u16[i];
		}
	}
	while(++i < pixels);
}

//

void GPUDrawScanline::Init()
{
	for(int i = 0; i < countof(m_ds); i++)
	{
		m_ds[i] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineT;
	}

	m_ds[0x00] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x00>;
	m_ds[0x01] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x01>;
	m_ds[0x02] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x02>;
	m_ds[0x03] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x03>;
	m_ds[0x04] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x04>;
	m_ds[0x05] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x05>;
	m_ds[0x06] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x06>;
	m_ds[0x07] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x07>;
	m_ds[0x08] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x08>;
	m_ds[0x09] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x09>;
	m_ds[0x0a] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x0a>;
	m_ds[0x0b] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x0b>;
	m_ds[0x0c] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x0c>;
	m_ds[0x0d] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x0d>;
	m_ds[0x0e] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x0e>;
	m_ds[0x0f] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x0f>;
	m_ds[0x10] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x10>;
	m_ds[0x11] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x11>;
	m_ds[0x12] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x12>;
	m_ds[0x13] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x13>;
	m_ds[0x14] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x14>;
	m_ds[0x15] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x15>;
	m_ds[0x16] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x16>;
	m_ds[0x17] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x17>;
	m_ds[0x18] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x18>;
	m_ds[0x19] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x19>;
	m_ds[0x1a] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x1a>;
	m_ds[0x1b] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x1b>;
	m_ds[0x1c] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x1c>;
	m_ds[0x1d] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x1d>;
	m_ds[0x1e] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x1e>;
	m_ds[0x1f] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x1f>;
	m_ds[0x20] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x20>;
	m_ds[0x21] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x21>;
	m_ds[0x22] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x22>;
	m_ds[0x23] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x23>;
	m_ds[0x24] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x24>;
	m_ds[0x25] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x25>;
	m_ds[0x26] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x26>;
	m_ds[0x27] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x27>;
	m_ds[0x28] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x28>;
	m_ds[0x29] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x29>;
	m_ds[0x2a] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x2a>;
	m_ds[0x2b] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x2b>;
	m_ds[0x2c] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x2c>;
	m_ds[0x2d] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x2d>;
	m_ds[0x2e] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x2e>;
	m_ds[0x2f] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x2f>;
	m_ds[0x30] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x30>;
	m_ds[0x31] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x31>;
	m_ds[0x32] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x32>;
	m_ds[0x33] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x33>;
	m_ds[0x34] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x34>;
	m_ds[0x35] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x35>;
	m_ds[0x36] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x36>;
	m_ds[0x37] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x37>;
	m_ds[0x38] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x38>;
	m_ds[0x39] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x39>;
	m_ds[0x3a] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x3a>;
	m_ds[0x3b] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x3b>;
	m_ds[0x3c] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x3c>;
	m_ds[0x3d] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x3d>;
	m_ds[0x3e] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x3e>;
	m_ds[0x3f] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x3f>;
	m_ds[0x40] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x40>;
	m_ds[0x41] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x41>;
	m_ds[0x42] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x42>;
	m_ds[0x43] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x43>;
	m_ds[0x44] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x44>;
	m_ds[0x45] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x45>;
	m_ds[0x46] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x46>;
	m_ds[0x47] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x47>;
	m_ds[0x48] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x48>;
	m_ds[0x49] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x49>;
	m_ds[0x4a] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x4a>;
	m_ds[0x4b] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x4b>;
	m_ds[0x4c] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x4c>;
	m_ds[0x4d] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x4d>;
	m_ds[0x4e] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x4e>;
	m_ds[0x4f] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x4f>;
	m_ds[0x50] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x50>;
	m_ds[0x51] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x51>;
	m_ds[0x52] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x52>;
	m_ds[0x53] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x53>;
	m_ds[0x54] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x54>;
	m_ds[0x55] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x55>;
	m_ds[0x56] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x56>;
	m_ds[0x57] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x57>;
	m_ds[0x58] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x58>;
	m_ds[0x59] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x59>;
	m_ds[0x5a] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x5a>;
	m_ds[0x5b] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x5b>;
	m_ds[0x5c] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x5c>;
	m_ds[0x5d] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x5d>;
	m_ds[0x5e] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x5e>;
	m_ds[0x5f] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x5f>;
	m_ds[0x60] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x60>;
	m_ds[0x61] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x61>;
	m_ds[0x62] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x62>;
	m_ds[0x63] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x63>;
	m_ds[0x64] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x64>;
	m_ds[0x65] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x65>;
	m_ds[0x66] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x66>;
	m_ds[0x67] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x67>;
	m_ds[0x68] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x68>;
	m_ds[0x69] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x69>;
	m_ds[0x6a] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x6a>;
	m_ds[0x6b] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x6b>;
	m_ds[0x6c] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x6c>;
	m_ds[0x6d] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x6d>;
	m_ds[0x6e] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x6e>;
	m_ds[0x6f] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x6f>;
	m_ds[0x70] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x70>;
	m_ds[0x71] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x71>;
	m_ds[0x72] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x72>;
	m_ds[0x73] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x73>;
	m_ds[0x74] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x74>;
	m_ds[0x75] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x75>;
	m_ds[0x76] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x76>;
	m_ds[0x77] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x77>;
	m_ds[0x78] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x78>;
	m_ds[0x79] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x79>;
	m_ds[0x7a] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x7a>;
	m_ds[0x7b] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x7b>;
	m_ds[0x7c] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x7c>;
	m_ds[0x7d] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x7d>;
	m_ds[0x7e] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x7e>;
	m_ds[0x7f] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x7f>;
	m_ds[0x80] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x80>;
	m_ds[0x81] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x81>;
	m_ds[0x82] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x82>;
	m_ds[0x83] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x83>;
	m_ds[0x84] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x84>;
	m_ds[0x85] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x85>;
	m_ds[0x86] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x86>;
	m_ds[0x87] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x87>;
	m_ds[0x88] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x88>;
	m_ds[0x89] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x89>;
	m_ds[0x8a] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x8a>;
	m_ds[0x8b] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x8b>;
	m_ds[0x8c] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x8c>;
	m_ds[0x8d] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x8d>;
	m_ds[0x8e] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x8e>;
	m_ds[0x8f] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x8f>;
	m_ds[0x90] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x90>;
	m_ds[0x91] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x91>;
	m_ds[0x92] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x92>;
	m_ds[0x93] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x93>;
	m_ds[0x94] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x94>;
	m_ds[0x95] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x95>;
	m_ds[0x96] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x96>;
	m_ds[0x97] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x97>;
	m_ds[0x98] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x98>;
	m_ds[0x99] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x99>;
	m_ds[0x9a] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x9a>;
	m_ds[0x9b] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x9b>;
	m_ds[0x9c] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x9c>;
	m_ds[0x9d] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x9d>;
	m_ds[0x9e] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x9e>;
	m_ds[0x9f] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0x9f>;
	m_ds[0xa0] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xa0>;
	m_ds[0xa1] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xa1>;
	m_ds[0xa2] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xa2>;
	m_ds[0xa3] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xa3>;
	m_ds[0xa4] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xa4>;
	m_ds[0xa5] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xa5>;
	m_ds[0xa6] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xa6>;
	m_ds[0xa7] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xa7>;
	m_ds[0xa8] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xa8>;
	m_ds[0xa9] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xa9>;
	m_ds[0xaa] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xaa>;
	m_ds[0xab] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xab>;
	m_ds[0xac] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xac>;
	m_ds[0xad] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xad>;
	m_ds[0xae] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xae>;
	m_ds[0xaf] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xaf>;
	m_ds[0xb0] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xb0>;
	m_ds[0xb1] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xb1>;
	m_ds[0xb2] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xb2>;
	m_ds[0xb3] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xb3>;
	m_ds[0xb4] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xb4>;
	m_ds[0xb5] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xb5>;
	m_ds[0xb6] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xb6>;
	m_ds[0xb7] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xb7>;
	m_ds[0xb8] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xb8>;
	m_ds[0xb9] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xb9>;
	m_ds[0xba] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xba>;
	m_ds[0xbb] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xbb>;
	m_ds[0xbc] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xbc>;
	m_ds[0xbd] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xbd>;
	m_ds[0xbe] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xbe>;
	m_ds[0xbf] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xbf>;
	m_ds[0xc0] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xc0>;
	m_ds[0xc1] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xc1>;
	m_ds[0xc2] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xc2>;
	m_ds[0xc3] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xc3>;
	m_ds[0xc4] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xc4>;
	m_ds[0xc5] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xc5>;
	m_ds[0xc6] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xc6>;
	m_ds[0xc7] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xc7>;
	m_ds[0xc8] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xc8>;
	m_ds[0xc9] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xc9>;
	m_ds[0xca] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xca>;
	m_ds[0xcb] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xcb>;
	m_ds[0xcc] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xcc>;
	m_ds[0xcd] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xcd>;
	m_ds[0xce] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xce>;
	m_ds[0xcf] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xcf>;
	m_ds[0xd0] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xd0>;
	m_ds[0xd1] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xd1>;
	m_ds[0xd2] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xd2>;
	m_ds[0xd3] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xd3>;
	m_ds[0xd4] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xd4>;
	m_ds[0xd5] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xd5>;
	m_ds[0xd6] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xd6>;
	m_ds[0xd7] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xd7>;
	m_ds[0xd8] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xd8>;
	m_ds[0xd9] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xd9>;
	m_ds[0xda] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xda>;
	m_ds[0xdb] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xdb>;
	m_ds[0xdc] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xdc>;
	m_ds[0xdd] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xdd>;
	m_ds[0xde] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xde>;
	m_ds[0xdf] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xdf>;
	m_ds[0xe0] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xe0>;
	m_ds[0xe1] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xe1>;
	m_ds[0xe2] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xe2>;
	m_ds[0xe3] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xe3>;
	m_ds[0xe4] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xe4>;
	m_ds[0xe5] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xe5>;
	m_ds[0xe6] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xe6>;
	m_ds[0xe7] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xe7>;
	m_ds[0xe8] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xe8>;
	m_ds[0xe9] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xe9>;
	m_ds[0xea] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xea>;
	m_ds[0xeb] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xeb>;
	m_ds[0xec] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xec>;
	m_ds[0xed] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xed>;
	m_ds[0xee] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xee>;
	m_ds[0xef] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xef>;
	m_ds[0xf0] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xf0>;
	m_ds[0xf1] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xf1>;
	m_ds[0xf2] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xf2>;
	m_ds[0xf3] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xf3>;
	m_ds[0xf4] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xf4>;
	m_ds[0xf5] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xf5>;
	m_ds[0xf6] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xf6>;
	m_ds[0xf7] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xf7>;
	m_ds[0xf8] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xf8>;
	m_ds[0xf9] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xf9>;
	m_ds[0xfa] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xfa>;
	m_ds[0xfb] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xfb>;
	m_ds[0xfc] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xfc>;
	m_ds[0xfd] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xfd>;
	m_ds[0xfe] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xfe>;
	m_ds[0xff] = (DrawScanlinePtr)&GPUDrawScanline::DrawScanlineExT<0xff>;
}

__declspec(align(16)) static WORD s_dither[4][16] = 
{
	{7, 0, 6, 1, 7, 0, 6, 1, 7, 0, 6, 1, 7, 0, 6, 1},
	{2, 5, 3, 4, 2, 5, 3, 4, 2, 5, 3, 4, 2, 5, 3, 4}, 
	{1, 6, 0, 7, 1, 6, 0, 7, 1, 6, 0, 7, 1, 6, 0, 7}, 
	{4, 3, 5, 2, 4, 3, 5, 2, 4, 3, 5, 2, 4, 3, 5, 2}, 
};

void GPUDrawScanline::DrawScanlineT(int top, int left, int right, const Vertex& v)	
{
	GSVector4 s[2], t[2]; 
	
	GSVector4 vt = v.t;

	s[0] = vt.xxxx(); s[1] = s[0];
	t[0] = vt.yyyy(); t[1] = t[0];

	if(m_sel.tme)
	{
		s[0] += m_slenv.ds[0];
		t[0] += m_slenv.dt[0];
		s[1] += m_slenv.ds[1];
		t[1] += m_slenv.dt[1];
	}

	GSVector4i vc = GSVector4i(v.c);

	vc = vc.ps32(vc);
	vc = vc.upl16(vc);

	GSVector4i r, g, b;
	
	r = vc.xxxx();
	g = vc.yyyy();
	b = vc.zzzz();

	if(m_sel.iip)
	{
		r = r.add16(m_slenv.dr);
		g = g.add16(m_slenv.dg);
		b = b.add16(m_slenv.db);
	}

	GSVector4i dither;

	if(m_sel.dtd)
	{
		dither = GSVector4i::load<false>(&s_dither[top & 3][left & 3]);
	}

	int steps = right - left;

	WORD* fb = m_slenv.mem->GetPixelAddress(left, top);

	while(1)
	{
		do
		{
			int pixels = GSVector4i::store(GSVector4i::load(steps).min_i16(GSVector4i::load(8)));

			GSVector4i test = GSVector4i::zero();

			GSVector4i d = GSVector4i::zero();

			if(m_sel.rfb) // me | abe
			{
				d = GSVector4i::load<false>(fb);

				if(m_sel.me)
				{
					test = d.sra16(15);

					if(test.alltrue())
					{
						continue;
					}
				}
			}

			GSVector4i c[4];

			if(m_sel.tme)
			{
				SampleTexture(pixels, m_sel.ltf, m_sel.tlu, m_sel.twin, test, s, t, c);
			}

			ColorTFX(m_sel.tfx, r, g, b, c);

			if(m_sel.abe)
			{
				AlphaBlend(m_sel.abr, m_sel.tme, d, c);
			}

			if(m_sel.dtd)
			{
				c[0] = c[0].addus8(dither);
				c[1] = c[1].addus8(dither);
				c[2] = c[2].addus8(dither);
			}

			WriteFrame(fb, test, c, pixels);
		}
		while(0);

		if(steps <= 8) break;

		steps -= 8;

		fb += 8;

		if(m_sel.tme)
		{
			GSVector4 ds = m_slenv.ds[2];
			GSVector4 dt = m_slenv.dt[2];

			s[0] += ds;
			t[0] += dt;
			s[1] += ds;
			t[1] += dt;
		}

		if(m_sel.iip)
		{
			GSVector4i dc = m_slenv.dc;

			r = r.add16(dc.xxxx());
			g = g.add16(dc.yyyy());
			b = b.add16(dc.zzzz());
		}
	}
}

template<DWORD sel>
void GPUDrawScanline::DrawScanlineExT(int top, int left, int right, const Vertex& v)
{
	DWORD iip = (sel >> 0) & 1;
	DWORD me = (sel >> 1) & 1;
	DWORD abe = (sel >> 2) & 1;
	DWORD abr = (sel >> 3) & 3;
	// DWORD tge = (sel >> 5) & 1;
	DWORD tme = (sel >> 6) & 1;
	DWORD twin = (sel >> 7) & 1;
	DWORD rfb = (sel >> 1) & 3;
	DWORD tfx = (sel >> 5) & 3;

	GSVector4 s[2], t[2]; 
	
	GSVector4 vt = v.t;

	s[0] = vt.xxxx(); s[1] = s[0];
	t[0] = vt.yyyy(); t[1] = t[0];
	
	if(tme)
	{
		s[0] += m_slenv.ds[0];
		t[0] += m_slenv.dt[0];
		s[1] += m_slenv.ds[1];
		t[1] += m_slenv.dt[1];
	}

	GSVector4i vc = GSVector4i(v.c);

	vc = vc.ps32(vc);
	vc = vc.upl16(vc);

	GSVector4i r, g, b;
	
	r = vc.xxxx();
	g = vc.yyyy();
	b = vc.zzzz();

	if(m_sel.iip)
	{
		r = r.add16(m_slenv.dr);
		g = g.add16(m_slenv.dg);
		b = b.add16(m_slenv.db);
	}

	GSVector4i dither;

	if(m_sel.dtd)
	{
		dither = GSVector4i::load<false>(&s_dither[top & 3][left & 3]);
	}

	int steps = right - left;

	WORD* fb = m_slenv.mem->GetPixelAddress(left, top);

	while(1)
	{
		do
		{
			int pixels = GSVector4i::store(GSVector4i::load(steps).min_i16(GSVector4i::load(8)));

			GSVector4i test = GSVector4i::zero();

			GSVector4i d = GSVector4i::zero();

			if(rfb) // me | abe
			{
				d = GSVector4i::load<false>(fb);

				if(me)
				{
					test = d.sra16(15);

					if(test.alltrue())
					{
						continue;
					}
				}
			}

			GSVector4i c[4];

			if(tme)
			{
				SampleTexture(pixels, m_sel.ltf, m_sel.tlu, twin, test, s, t, c);
			}

			ColorTFX(tfx, r, g, b, c);

			if(abe)
			{
				AlphaBlend(abr, tme, d, c);
			}

			if(m_sel.dtd)
			{
				c[0] = c[0].addus8(dither);
				c[1] = c[1].addus8(dither);
				c[2] = c[2].addus8(dither);
			}

			WriteFrame(fb, test, c, pixels);
		}
		while(0);

		if(steps <= 8) break;

		steps -= 8;

		fb += 8;

		if(tme)
		{
			GSVector4 ds = m_slenv.ds[2];
			GSVector4 dt = m_slenv.dt[2];

			s[0] += ds;
			t[0] += dt;
			s[1] += ds;
			t[1] += dt;
		}

		if(iip)
		{
			GSVector4i dc = m_slenv.dc;

			r = r.add16(dc.xxxx());
			g = g.add16(dc.yyyy());
			b = b.add16(dc.zzzz());
		}
	}
}