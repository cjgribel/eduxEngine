// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef parseutil_h
#define parseutil_h

// std
//#include <stdarg.h>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>

//if (filter_group_prefix.size()) {
//    if (dc.group_name.size() && !dc.group_name.compare(0, filter_group_prefix.length(), filter_group_prefix))
//        continue;
//}

inline std::string& rtrim(std::string& str)
{
    str.erase(str.find_last_not_of(" \n\r\t")+1);
    return str;
}

inline std::string& ltrim(std::string& str)
{
    str.erase(0, str.find_first_not_of(" \n\r\t"));
    return str;
}

inline std::string& lrtrim(std::string& str)
{
    return ltrim(rtrim(str));
}

inline std::string& lowercase(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

inline std::string lowercase_of(const std::string& str)
{
    std::string strlc = str;
    lowercase(strlc);
    return strlc;
}

inline std::string& uppercase(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

inline std::string uppercase_of(const std::string& str)
{
    std::string strlc = str;
    lowercase(strlc);
    return strlc;
}


inline void lowercase(char *s)
{
    while (*s) { *s = tolower(*s); s++; }
    
    // Alt.
//    for(int i = 0; s[i]; i++) s[i] = tolower(s[i]);
    
    // Alt.
//    for ( ; *s; ++s) *s = tolower(*s);
}

inline void uppercase(char *s)
{
    while (*s) { *s = toupper(*s); s++; }
}

struct PathSeparator_WIN { bool operator()( char ch ) const { return ch == '\\' || ch == '/'; } };
struct PathSeparator_UNIX { bool operator()( char ch ) const { return ch == '/'; } };
struct PathSeparator_ANY { bool operator()( char ch ) const { return ch == '\\' || ch == '/'; } };
struct ExtSeparator { bool operator()( char ch ) const { return ch == '.'; } };

// Parent- & file-path extraction code inspired by:
// https://stackoverflow.com/questions/8520560/get-a-file-name-from-a-path

inline std::string get_parentdir(const std::string& path)
{
    return std::string( path.begin(), std::find_if(path.rbegin(), path.rend(), PathSeparator_ANY() ).base() );
}

inline std::string get_filename(const std::string& path, std::function<bool(char)> PathSeparator = PathSeparator_ANY())
{
    return std::string( std::find_if( path.rbegin(), path.rend(), PathSeparator).base(), path.end() );
}

inline std::string get_fileext(const std::string& path)
{
    return std::string( std::find_if( path.rbegin(), path.rend(), ExtSeparator() ).base(), path.end() );
}

//! Decompose string into parts, e.g. "c:/dir/file.ext" -> { "c:/dir/", "file", "ext" }
inline void decompose_path(const std::string& fullpath,
                           std::string& path,
                           std::string& filename,
                           std::string& ext)
{
    auto pathsep = std::find_if( fullpath.rbegin(), fullpath.rend(), PathSeparator_ANY() );
    auto extsep = std::find_if( fullpath.rbegin(), pathsep, ExtSeparator() );
    
    path = std::string(fullpath.begin(), pathsep.base());
    
    if (std::distance(extsep, pathsep) > 0)
    {
        filename = std::string(pathsep.base(), extsep.base()-1);
        ext = std::string(extsep.base(), fullpath.end());
    }
    else
    {
        filename = std::string(extsep.base(), fullpath.end());
        ext = "";
    }
}

#if 0
static std::string get_parentdir(std::string path)
{
#ifdef _WIN32
    throw std::runtime_error("get_parentdir not defined for _WIN32\n");
#else
    std::string parent;
    size_t pos = path.rfind("/");
    
    if (pos != std::string::npos)
        parent = path.substr(0, pos+1);
    
    return parent;
#endif
}
#endif

//static std::string get_file_from_path(std::string path)
//{
//#ifdef _WIN32
//    throw std::runtime_error("get_parentdir not defined for _WIN32\n");
//#else
//    std::string file;
//    size_t pos = path.rfind("/");
//
//    if (pos != std::string::npos)
//        file = path.substr(pos, );
//
//    return file;
//#endif
//}

//
// Look for an option in the provided token list and parse the provided number of subsequent arguments
//
//template<typename...Args>
static bool find_option_args(const std::vector<std::string>& tokens,
                              const std::string& option_name,
                              int nbr_args,
                              ...)
                             //Args&...argvalues)
{
    auto opt_it = std::find(tokens.begin(), tokens.end(), option_name);
    if (opt_it > tokens.end() - nbr_args) return false;
    
    va_list retargs;
    va_start(retargs, nbr_args);
    
    for (int i=0; i<nbr_args; i++)
    {
        
        double argvalue = stod( *(opt_it+1+i) );    // Obtain and cast option argument value
        double* retval = va_arg(retargs, double*);  // Obtain return variable
        *retval = argvalue;                         // Assign argument value to return variable
    }
    
    va_end(retargs);
    return true;
}

//
// Look for a sequence of '-option_name val1 val2' in a vector of tokens
//
static bool find_option_args(const std::vector<std::string>& tokens,
                        const std::string& option_name,
                        float& val1,
                        float& val2)
{
    auto opt_it = std::find(tokens.begin(), tokens.end(), option_name);
    
    int nbr_values = 2;
    if (opt_it > tokens.end() - nbr_values) return false;

    val1 = stof(*(opt_it+1));
    val2 = stof(*(opt_it+2));
    return true;
}

//
// find and extract first occurance of *.[suffix] in a string
//
static bool find_filename_from_suffix(const std::string &str, const std::string &suffix, std::string& res)
{
    std::string dotsuffix = std::string(".")+suffix;
    size_t end = str.find(dotsuffix);
    
    if (end == std::string::npos)
        return false;
    
    size_t start = str.rfind(" ", end);
    
    if (start == end)
        return false;
    if (start == std::string::npos)
        start = -1;
    
    res = str.substr(start+1, end-start-1 + dotsuffix.length());
    return true;
}

//
// find and extract first occurance of *.[{suffix-list}] in a string
//
static bool find_filename_from_suffixes(const std::string &str, const std::vector<std::string>& suffixes, std::string& res)
{
    for (auto& suffix : suffixes)
        if (find_filename_from_suffix(str, suffix, res))
            return true;
    return false;
}

// tokenization wrt to a delimiter character
//
// note: use find_first_of to use a SET of valid delimiter characters
//
static std::vector<std::string> tokenize(const std::string& str, const char delim)
{
    std::vector<std::string> tokens;
    size_t str_len = str.length();
    
    size_t si = 0, sj = str.find(delim, 0);
    while (sj != std::string::npos)
    {
        if (si < sj)
            tokens.push_back(str.substr(si, sj-si));
        
        si = sj + 1;
        sj = str.find(delim, si);
    }
    if (si < str_len)
        tokens.push_back(str.substr(si, str_len-si));
    
    return tokens;
}

#endif /* parseutil_h */
