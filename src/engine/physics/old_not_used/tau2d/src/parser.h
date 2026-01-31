//
//  parser.h
//  tau2d
//
//  Created by Carl Johan Gribel on 2013-10-14.
//  Copyright (c) 2013 __MyCompanyName__. All rights reserved.
//

#ifndef tau2d_parser_h
#define tau2d_parser_h

#include "t2World.h"
#include "config.h"

#include <sstream>
#include <string>

#define KEY_ENTER (char)13
#define KEY_SPACE (char)32
#define KEY_BACKSPACE (char)127
#define KEY_SEMIC (char)59
#define KEY_ESC (char)27
#define KEY_TAB (char)9

/**** parser, grammar implemented using AST & traversal *****/

/* production, terminal */
struct ASTnode
{
    //string lexeme;
    std::vector<ASTnode> children;
    //bool is_terminal;
};

struct ASTnode_Stmt {};

struct ASTnode_float_Exp
{
    float nbr;
    
    bool parse(std::vector<string> tokens, int token_nbr, void* res)
    {
        if (token_nbr >= tokens.size()) return false;
        
        string token = tokens[token_nbr];
        if (sscanf(token.c_str(), "%f", &nbr) == 1)
            return true;
        
        //print_options(tokens, token_nbr, float_cmd, nbr_float_cmd);
        return false;
    }
};

struct ASTnode_Exp {};

// traverse: check syntaxt (return true), tab, execute


/**** parser, grammar implemented using methods *****/

/* productions & terminals */
string stmt[] = {"get", "set", "list", "render", "[exp]"};
int nbr_stmt = 5;
string get_stmt[] = {"mass", "i"};
int nbr_get_stmt = 2;
string set_stmt[] = {"mass", "i", "g"};
int nbr_set_stmt = 3;
string render_stmt[] = {"fill", "aabb", "contacts", "forces", "joints", "local_space", "normals", "sleep"};
int nbr_render_stmt = 8;
string float_cmd[] = {"[float]"};
int nbr_float_cmd = 1;
string int_cmd[] = {"[int]"};
int nbr_int_cmd = 1;

/* current input string */
string s;

/* application specifics */
extern int nbrWorlds;
extern unsigned int render_mask;

/*
    find (and print) whole or partial matches of token wrt array of lexemes
    note: whole matches exist "should" have been parsed and not appear here
 */
static void print_options(std::vector<std::string> tokens, int token_nbr, string *lexemes, int nbr_lexemes, bool is_production = true)
{
    string token = token_nbr<tokens.size()? tokens[token_nbr] : "";
    int token_len = token.length();
    std::vector<string> options;
    
    // find matches (unless this is not a production, like a numeral)
    if (is_production) {
        for (int i=0; i<nbr_lexemes; i++) {
            if (token_len <= lexemes[i].length()){
                if (token == lexemes[i].substr(0, token_len))
                    options.push_back(lexemes[i]);
            }
        }
    }
    
    // one match?: add to input string (ensure that token matches ending of input string)
    if (options.size() == 1 && token == s.substr(s.length()-token.length(), std::string::npos)) {
        s += options[0].substr(token_len, std::string::npos);
    // no matches?: add all lexemes to options
    } else if (options.size() == 0) {
        for (int i=0; i<nbr_lexemes; i++)
            options.push_back(lexemes[i]);
    }
    
    // print parsed tokens
    printf("options: ");
    for (int i=0; i<token_nbr; i++)
        printf("%s ", tokens[i].c_str());
    printf("[");
    // print options
    for (int i=0; i<options.size(); i++)
        printf("%s%s", options[i].c_str(), i<options.size()-1?" ":"");
    printf("]\n");
}

static bool parse_int(std::vector<std::string> tokens, int token_nbr, bool tab, int &nbr)
{
    string token = token_nbr<tokens.size()? tokens[token_nbr] : "";
    
    if(sscanf(token.c_str(), "%d", &nbr) == 1)
        return true;
    
    print_options(tokens, token_nbr, int_cmd, nbr_int_cmd, false);
    return false;
}

static bool parse_float(std::vector<std::string> tokens, int token_nbr, bool tab, float &nbr)
{
    string token = token_nbr<tokens.size()? tokens[token_nbr] : "";
    
    if(sscanf(token.c_str(), "%f", &nbr) == 1)
        return true;
    
    print_options(tokens, token_nbr, float_cmd, nbr_float_cmd, false);
    return false;
}

static bool parse_set_stmt(std::vector<std::string> tokens, int token_nbr, bool tab)
{
    string token = token_nbr<tokens.size()? tokens[token_nbr] : "";
    float nbr0, nbr1;
    
    if (token == "mass")
    {
        if (!parse_float(tokens, token_nbr+1, tab, nbr0)) return false;

        printf("-> set mass %f\n", nbr0);
        
    }
    else if (token == "i")
    {
        if (!parse_float(tokens, token_nbr+1, tab, nbr0)) return false;
        
        printf("-> set i %f\n", nbr0);    
    }
    else if (token == "g")
    {
        if (!parse_float(tokens, token_nbr+1, tab, nbr0)) return false;
        if (!parse_float(tokens, token_nbr+2, tab, nbr1)) return false;
        
        printf("-> set g %f %f\n", nbr0, nbr1);
    }
    else
    {
        print_options(tokens, token_nbr, set_stmt, nbr_set_stmt);   
        return false;
    }
    return true;
}

static bool parse_get_stmt(std::vector<std::string> tokens, int token_nbr, bool tab)
{
    string token = token_nbr<tokens.size()? tokens[token_nbr] : "";
    
    if (token == "mass")
    {
    }
    else if (token == "i")
    {   
    }
    else
    {
        print_options(tokens, token_nbr, get_stmt, nbr_get_stmt);
        return false;
    }
    return true;
}

static bool parse_render_stmt(std::vector<std::string> tokens, int token_nbr, bool tab)
{
    string token = token_nbr<tokens.size()? tokens[token_nbr] : "";
    int nbr0;
    
    if (token == "fill")
    {
        if (!parse_int(tokens, token_nbr+1, tab, nbr0)) return false;
        render_mask = nbr0 ? render_mask | T2_RENDER_FILL : render_mask & !T2_RENDER_FILL;
    }
    else if (token == "aabb")
    {
        if (!parse_int(tokens, token_nbr+1, tab, nbr0)) return false;
        render_mask = nbr0 ? render_mask | T2_RENDER_AABB : render_mask & !T2_RENDER_AABB;
    }
    else if (token == "contacts")
    {
        if (!parse_int(tokens, token_nbr+1, tab, nbr0)) return false;
        render_mask = nbr0 ? render_mask | T2_RENDER_CONTACTS : render_mask & !T2_RENDER_CONTACTS;
    }
    else if (token == "forces")
    {
        if (!parse_int(tokens, token_nbr+1, tab, nbr0)) return false;
        render_mask = nbr0 ? render_mask | T2_RENDER_FORCES : render_mask & !T2_RENDER_FORCES;
    }
    else if (token == "joints")
    {
        if (!parse_int(tokens, token_nbr+1, tab, nbr0)) return false;
        render_mask = nbr0 ? render_mask | T2_RENDER_JOINTS : render_mask & !T2_RENDER_JOINTS;
    }
    else if (token == "local_space")
    {
        if (!parse_int(tokens, token_nbr+1, tab, nbr0)) return false;
        render_mask = nbr0 ? render_mask | T2_RENDER_LOCAL_SPACE : render_mask & !T2_RENDER_LOCAL_SPACE;
    }
    else if (token == "normals")
    {
        if (!parse_int(tokens, token_nbr+1, tab, nbr0)) return false;
        render_mask = nbr0 ? render_mask | T2_RENDER_NORMALS : render_mask & !T2_RENDER_NORMALS;
    }
    else if (token == "sleep")
    {
        if (!parse_int(tokens, token_nbr+1, tab, nbr0)) return false;
        render_mask = nbr0 ? render_mask | T2_RENDER_SLEEP_STATUS : render_mask & !T2_RENDER_SLEEP_STATUS;
    }
    else
    {
        print_options(tokens, token_nbr, render_stmt, nbr_render_stmt);
        return false;
    }
    return true;
}

static bool parse_exp_stmt(std::vector<std::string> tokens, int token_nbr, bool tab)
{
    string token = token_nbr<tokens.size()? tokens[token_nbr] : "";
    float nbr0;
    /*
     5
     -5
     (exp)
     (add/sub/mult/div-exp)
     
     need global token index to parse '(' exp ')'
     all methods within one class?
     - may make it easier to check the last token during autocomplete
     */
}

static bool parse_stmt(std::vector<std::string> tokens, int token_nbr, bool tab = 0)
{
//    printf("parse_cmd received: ");
//    for (int i=0; i<tokens.size(); i++)
//        printf("%s, ", tokens[i].c_str());
//    printf("\n");
    
    string token = token_nbr<tokens.size()? tokens[token_nbr] : "";
    
    
    if (token == "get")
    {
        return parse_get_stmt(tokens, token_nbr+1, tab);
    }
    else if (token == "set")
    {
        // "eat" token before returning (or keep global current index)
        // + investigate wildcards (*) for scanf; may allow for more flexible scanning
        
        return parse_set_stmt(tokens, token_nbr+1, tab);
    }
    else if (token == "list")
    {
        printf("parsed: list\n");
        return true;
    }
    else if (token == "render")
    {
        return parse_render_stmt(tokens, token_nbr+1, tab);
    }
    else if (parse_exp_stmt(tokens, token_nbr, tab))
    {
        return true;
    }
    else
    {
        print_options(tokens, token_nbr, stmt, nbr_stmt);
        return false;
    }
}

/*
 tokenize string wrt an arbitrary delimiter
 
 http://stackoverflow.com/a/10051869
 */
std::vector<std::string> inline StringSplit(const std::string &source, const char *delimiter = " ", bool keepEmpty = false)
{
    std::vector<std::string> results;
    
    size_t prev = 0;
    size_t next = 0;
    
    while ((next = source.find_first_of(delimiter, prev)) != std::string::npos)
    {
        if (keepEmpty || (next - prev != 0))
        {
            results.push_back(source.substr(prev, next - prev));
        }
        prev = next + 1;
    }
    
    if (prev < source.size())
    {
        results.push_back(source.substr(prev));
    }
    
    return results;
}

struct glut_input_parser
{
    //string s;
    istringstream output_stream;
    
    glut_input_parser() { }
    
    void put_char(unsigned char c)
    {
        if (c == KEY_ESC)
        {
            s.clear();
        }
        else if (c == KEY_BACKSPACE)
        {
            if (s.length() > 0)
                s = s.substr(0, s.length()-1);
        }   
        else if (c == KEY_TAB)
        {
            std::vector<std::string> tokens = StringSplit(s);
            parse_stmt(tokens, 0, true);
        }
        else if (c == KEY_ENTER)
        {
            std::vector<std::string> tokens = StringSplit(s);
            
            if (parse_stmt(tokens, 0))
                s.clear();
        }
        else
        {   
            s += c;
        }
        printf(">%s\n", s.c_str());
    }
    
    string get_current_input() { return s; }
    
//    std::vector<std::string> get_current_output() { return output; }
    
};

static void parse_stdinput(t2World *world, t2Body *body)
{
    char in[200];
    string s;
    
    while (getline(cin, s))
    {
        char c0[200], c1[200];
        int i0;
        float f0, f1, f2;
        
        /*
         exit
         */
        if (s == ";")
        {
            break;
        }
        /*
         delete
         */
        else if (s == "delete")
        {
//            world->removeBody(body);
            printf("[TODO]\n");
        }
        /*
         get mass
         */
        else if (s == "get mass")
        {
            printf("mass = %f\n", body->mass);
        }
        /*
         set mass
         */
        else if (sscanf(s.c_str(), "set mass %f", &f0) == 1)
        {
            body->mass = f0;
            printf("%f\n", body->mass);
        }
        /*
         get angular inertia
         */
        else if (s == "get i")
        {
            printf("i = %f\n", body->I);
        }
        /*
         set angular inertia
         */
        else if (sscanf(s.c_str(), "set i %f", &f0) == 1)
        {
            body->I = f0;
            printf("%f\n", body->I);
        }
        /*
         set gravity
         */
        else if (sscanf(s.c_str(), "set g %f %f", &f0, &f1) == 2)
        {
            body->gravity = vec2f(f0, f1);
            printf("g = (%f, %f)\n", body->gravity.x, body->gravity.y);
        }
        /*
         set color
         */
        else if (sscanf(s.c_str(), "set color %f %f %f", &f0, &f1, &f2) == 3)
        {
            body->colorFillR = f0;
            body->colorFillG = f1;
            body->colorFillB = f2;
            printf("color = (%f, %f, %f)\n", body->colorFillR, body->colorFillG, body->colorFillB);
        }
        /*
         default
         */
        else
        {
            sscanf(s.c_str(), "%s", c0);
            printf("unknown: %s\n", c0);
        }
    }
}

#endif
