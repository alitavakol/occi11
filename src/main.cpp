#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "occi11.h"
#include <iostream>

int main(void) {
	occi11 o("USERNAME", "PASSWORD", "CONNECTION_STRING");
	o.connect();

	o.ensureExecuteQuery("select count(*) from atable;", [](occi11*, oracle::occi::ResultSet*) {
		return false;
	}, [](const oracle::occi::SQLException &e) {
		std::cerr << occi11::make_exception_string(e) << std::endl;
	});

	return 0;
}
