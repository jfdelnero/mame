$input v_texcoord0, v_color0

/*
 * Copyright 2011-2017 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "common.sh"

SAMPLERCUBE(s_texColor, 0);

void main()
{
	vec4 color = textureCubeLod(s_texColor, v_texcoord0, u_textureLod);
	gl_FragColor = toEv(color * v_color0);
}
