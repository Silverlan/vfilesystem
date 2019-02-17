/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "impl_fsys_util.hpp"

bool fsys::impl::has_value(std::vector<std::string> *values,size_t start,size_t end,std::string val,bool bKeepCase)
{
	if(bKeepCase == false)
		ustring::to_lower(val);
	for(auto i=start;i!=end;i++)
	{
		std::string valCmp = (*values)[i];
		if(bKeepCase == false)
			ustring::to_lower(valCmp);
		if(val == valCmp)
			return true;
	}
	return false;
}
