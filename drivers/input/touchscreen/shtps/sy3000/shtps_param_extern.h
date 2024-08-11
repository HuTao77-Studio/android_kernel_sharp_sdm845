/* drivers/input/touchscreen/shtps/sy3000/shtps_param_extern.h
 *
 * Copyright (c) 2017, Sharp. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SHTPS_PARAM_EXTERN_H__
#define __SHTPS_PARAM_EXTERN_H__
/* --------------------------------------------------------------------------- */
#include <linux/input/shtps_dev.h>	/* (1) */
#include "shtps_cfg.h"			/* (2) */
/* --------------------------------------------------------------------------- */
#undef SHTPS_PARAM_DEF			/* (3) */
#if defined( SHTPS_DEVELOP_MODE_ENABLE )	/* (4) */
	#define SHTPS_PARAM_DEF(name, val) 	extern int name;
#else
	#define SHTPS_PARAM_DEF(name, val)	extern const int name;
#endif /* SHTPS_DEVELOP_MODE_ENABLE */
/* --------------------------------------------------------------------------- */
#include "shtps_param_list.h"	/* (5) */
/* --------------------------------------------------------------------------- */
#undef SHTPS_PARAM_DEF			/* (6) */
#if defined( SHTPS_DEVELOP_MODE_ENABLE )	/* (7) */
	#define SHTPS_PARAM_DEF(name, val) \
		int name = val; \
		module_param(name, int, S_IRUGO | S_IWUSR);
#else
	#define SHTPS_PARAM_DEF(name, val) \
		const int name = val;
#endif /* SHTPS_DEVELOP_MODE_ENABLE */
/* --------------------------------------------------------------------------- */
#endif	/* __SHTPS_PARAM_EXTERN_H__ */

