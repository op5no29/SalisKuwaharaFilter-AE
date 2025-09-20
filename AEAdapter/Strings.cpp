/*******************************************************************/
/*                                                                 */
/*                   Salis Kuwahara Filter                         */
/*                                                                 */
/*******************************************************************/

#include "SalisKuwaharaFilter.h"

typedef struct {
	A_u_long	index;
	A_char		str[256];
} TableString;

TableString	g_strs[StrID_NUMTYPES] = {
	StrID_NONE,						"",
	StrID_Name,						"Salis Kuwahara Filter",
	StrID_Description,				"Applies a Kuwahara filter for painterly effects.\nBy Salis.",
	StrID_Radius_Param_Name,		"Radius",
	StrID_Sectors_Param_Name,		"Sector Count",
	StrID_Anisotropy_Param_Name,	"Anisotropy",
	StrID_Softness_Param_Name,		"Softness",
	StrID_Mix_Param_Name,			"Mix",
};

char *GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}