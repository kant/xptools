#include "WED_Airport.h"
#include "WED_EnumSystem.h"

DEFINE_PERSISTENT(WED_Airport)

WED_Airport::WED_Airport(WED_Archive * a, int i) : WED_GISComposite(a,i),
	airport_type	(this, "Type",				"WED_airport",	"kind",			Airport_Type, type_Airport),
	elevation		(this, "Field Elevation",	"WED_airport",	"elevation",	0),
	has_atc			(this, "Has ATC",			"WED_airport",	"has_atc",		1),
	icao			(this, "ICAO Identifier",	"WED_airport",	"icao",			"xxxx")
{
}

WED_Airport::~WED_Airport()
{
}

void	WED_Airport::GetICAO(string& i) const
{
	i = icao.value;
}

int		WED_Airport::GetAirportType(void) const
{
	return airport_type.value;
}
