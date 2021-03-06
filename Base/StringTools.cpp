/*
 * StringTools.cpp
 *
 *  Created on: Jun 16, 2010
 *      Author: TF
 * \copyright
 * Copyright (c) 2015, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 */

#include "StringTools.h"

std::list<std::string> splitString(const std::string& str, char delim)
{
	std::list<std::string> strList;
	std::stringstream ss(str);
	std::string item;
	while (getline(ss, item, delim))
		strList.push_back(item);
	return strList;
}

std::string replaceString(const std::string& searchString,
                          const std::string& replaceString,
                          std::string stringToReplace)
{
	std::string::size_type pos = stringToReplace.find(searchString, 0);
	int intLengthSearch = searchString.length();

	while (std::string::npos != pos)
	{
		stringToReplace.replace(pos, intLengthSearch, replaceString);
		pos = stringToReplace.find(searchString, 0);
	}
	return stringToReplace;
}

void trim(std::string& str, char ch)
{
	std::string::size_type pos = str.find_last_not_of(ch);
	if (pos != std::string::npos)
	{
		str.erase(pos + 1);
		pos = str.find_first_not_of(ch);
		if (pos != std::string::npos) str.erase(0, pos);
	}
	else
		str.erase(str.begin(), str.end());
}

std::string ltrim(const std::string& str, const char c)
{
	const size_t idx = str.find_first_not_of(c);
	if (idx == std::string::npos)
	{
		// string consists only of c
		return "";
	}
	else
	{
		return str.substr(idx);
	}
}

std::string rtrim(const std::string& str, const char c)
{
	const size_t idx = str.find_last_not_of(c);
	if (idx == std::string::npos)
	{
		// string consists only of c
		return "";
	}
	else
	{
		return str.substr(0, idx + 1);
	}
}

namespace BaseLib
{
std::string getFileNameFromPath(const std::string& str, bool with_extension)
{
	std::string::size_type beg1 = str.find_last_of('/');
	std::string::size_type beg2 = str.find_last_of('\\');
	std::string::size_type beg;
	if (beg1 == std::string::npos && beg2 == std::string::npos)
		beg = -1;
	else if (beg1 == std::string::npos)
		beg = beg2;
	else if (beg2 == std::string::npos)
		beg = beg1;
	else
		beg = (beg1 < beg2) ? beg2 : beg1;
	std::string file(str.substr(beg + 1));
	if (with_extension) return file;
	// cut extension
	std::string::size_type end = file.find_last_of('.');
	return file.substr(0, end);
}

std::string copyPathToFileName(const std::string& file_name,
                               const std::string& source)
{
	// check if file_name already contains a full path
	size_t pos(file_name.rfind("/"));  // linux, mac delimiter
	if (pos == std::string::npos)
	{
		pos = file_name.rfind("\\");  // windows delimiter
		if (pos == std::string::npos)
		{
			std::string path;
			BaseLib::extractPath(source, path);
			return path.append(file_name);
		}
		else
			return std::string(file_name);
	}
	else
		return std::string(file_name);
}

void extractPath(std::string const& fname, std::string& path)
{
	// extract path for reading external files
	size_t pos(fname.rfind("/"));  // linux, mac delimiter
	if (pos == std::string::npos)
	{
		pos = fname.rfind("\\");  // windows delimiter
		if (pos == std::string::npos) pos = 0;
	}
	path = fname.substr(0, pos == 0 ? pos : pos + 1);
}

}  // end namespace BaseLib
