/*
 * TinyJS
 *
 * A single-file Javascript-alike engine
 *
 * Authored By Gordon Williams <gw@pur3.co.uk>
 * Additional Coding By Marco Lizza <marco.lizza@gmail.com>
 *
 * Copyright (C) 2009 Pur3 Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Version 0.1  :  (gw) First published on Google Code
   Version 0.11 :  Making sure the 'root' variable never changes
                   'symbol_base' added for the current base of the sybmbol table
   Version 0.12 :  Added findChildOrCreate, changed string passing to use references
                   Fixed broken string encoding in getJSString()
                   Removed getInitCode and added getJSON instead
                   Added nil
                   Added rough JSON parsing
                   Improved example app
   Version 0.13 :  Added tokenEnd/tokenLastEnd to lexer to avoid parsing whitespace
                   Ability to define functions without names
                   Can now do "var mine = function(a,b) { ... };"
                   Slightly better 'trace' function
                   Added findChildOrCreateByPath function
                   Added simple test suite
                   Added skipping of blocks when not executing
   Version 0.14 :  Added parsing of more number types
                   Added parsing of string defined with '
                   Changed nil to null as per spec, added 'undefined'
                   Now set variables with the correct scope, and treat unknown
                              as 'undefined' rather than failing
                   Added proper (I hope) handling of null and undefined
                   Added === check
   Version 0.15 :  Fix for possible memory leaks
   Version 0.16 :  Removal of un-needed findRecursive calls
                   symbol_base removed and replaced with 'scopes' stack
                   Added reference counting a proper tree structure
                       (Allowing pass by reference)
                   Allowed JSON output to output IDs, not strings
                   Added get/set for array indices
                   Changed Callbacks to include user data pointer
                   Added some support for objects
                   Added more Java-esque builtin functions
   Version 0.17 :  Now we don't deepCopy the parent object of the class
                   Added JSON.stringify and eval()
                   Nicer JSON indenting
                   Fixed function output in JSON
                   Added evaluateComplex
                   Fixed some reentrancy issues with evaluate/execute
   Version 0.18 :  Fixed some issues with code being executed when it shouldn't
   Version 0.19 :  Added array.length
                   Changed '__parent' to 'prototype' to bring it more in line with javascript
   Version 0.20 :  Added '%' operator
   Version 0.21 :  Added array type
                   String.length() no more - now String.length
                   Added extra constructors to reduce confusion
                   Fixed checks against undefined
   Version 0.22 :  First part of ardi's changes:
                       sprintf -> sprintf_s
                       extra tokens parsed
                       array memory leak fixed
                   Fixed memory leak in evaluateComplex
                   Fixed memory leak in FOR loops
                   Fixed memory leak for unary minus
   Version 0.23 :  Allowed evaluate[Complex] to take in semi-colon separated
                     statements and then only return the value from the last one.
                     Also checks to make sure *everything* was parsed.
                   Ints + doubles are now stored in binary form (faster + more precise)
   Version 0.24 :  More useful error for maths ops
                   Don't dump everything on a match error.
   Version 0.25 :  Better string escaping
   Version 0.26 :  Add CScriptVar::equals
                   Add built-in array functions
   Version 0.27 :  Added OZLB's TinyJS.setVariable (with some tweaks)
                   Added OZLB's Maths Functions
   Version 0.28 :  Ternary operator
                   Rudimentary call stack on error
                   Added String Character functions
                   Added shift operators
   Version 0.29 :  Added new object via functions
                   Fixed getString() for double on some platforms
   Version 0.30 :  Rlyeh Mario's patch for Math Functions on VC++
   Version 0.31 :  Add exec() to TinyJS functions
                   Now print quoted JSON that can be read by PHP/Python parsers
                   Fixed postfix increment operator
   Version 0.32 :  Fixed Math.randInt on 32 bit PCs, where it was broken
   Version 0.33 :  Fixed Memory leak + brokenness on === comparison

    NOTE:
          Constructing an array with an initial length 'Array(5)' doesn't work
          Recursive loops of data such as a.foo = a; fail to be garbage collected
          length variable cannot be set
          The postfix increment operator returns the current value, not the previous as it should.
          There is no prefix increment operator
          Arrays are implemented as a linked list - hence a lookup time is O(n)

    TODO:
          Utility va-args style function in TinyJS for executing a function directly
          Merge the parsing of expressions/statements so eval("statement") works like we'd expect.
          Move 'shift' implementation into mathsOp

 */

#include "TinyJS.h"
#include <assert.h>

#ifndef ASSERT
  #define ASSERT(X) assert(X)
#endif
/* Frees the given link IF it isn't owned by anything else */
#define CLEAN(x) { VariableLink *__v = x; if (__v && !__v->owned) { delete __v; } }
/* Create a LINK to point to VAR and free the old link.
 * BUT this is more clever - it tries to keep the old link if it's not owned to save allocations */
#define CREATE_LINK(LINK, VAR) { if (!LINK || LINK->owned) LINK = new VariableLink(VAR); else LINK->replaceWith(VAR); }

#include <string>
#include <string.h>
#include <sstream>
#include <cstdlib>
#include <stdio.h>

#if defined(_WIN32) && !defined(_WIN32_WCE)
#ifdef _DEBUG
   #ifndef DBG_NEW
      #define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
      #define new DBG_NEW
   #endif
#endif
#endif

#ifdef __GNUC__
  #define vsprintf_s vsnprintf
  #define sprintf_s snprintf
  #define _strdup strdup
#endif

#ifdef _WIN32_WCE
  #include <strsafe.h>
  #define sprintf_s StringCbPrintfA
#endif

// ----------------------------------------------------------------------------------- Namespace

namespace TinyJS {

// ----------------------------------------------------------------------------------- Memory Debug

#ifdef DEBUG
  #define DEBUG_MEMORY 1
#endif

#if DEBUG_MEMORY

std::vector<Variable*> allocatedVars;
std::vector<VariableLink*> allocatedLinks;

void mark_allocated(Variable *v) {
    allocatedVars.push_back(v);
}

void mark_deallocated(Variable *v) {
    for (size_t i=0;i<allocatedVars.size();i++) {
      if (allocatedVars[i] == v) {
        allocatedVars.erase(allocatedVars.begin()+i);
        break;
      }
    }
}

void mark_allocated(VariableLink *v) {
    allocatedLinks.push_back(v);
}

void mark_deallocated(VariableLink *v) {
    for (size_t i=0;i<allocatedLinks.size();i++) {
      if (allocatedLinks[i] == v) {
        allocatedLinks.erase(allocatedLinks.begin()+i);
        break;
      }
    }
}

void show_allocated() {
    for (size_t i=0;i<allocatedVars.size();i++) {
      printf("ALLOCATED, %d refs\n", allocatedVars[i]->getRefs());
      allocatedVars[i]->trace("  ");
    }
    for (size_t i=0;i<allocatedLinks.size();i++) {
      printf("ALLOCATED LINK %s, allocated[%d] to \n", allocatedLinks[i]->name.c_str(), allocatedLinks[i]->var->getRefs());
      allocatedLinks[i]->var->trace("  ");
    }
    allocatedVars.clear();
    allocatedLinks.clear();
}
#endif

// ----------------------------------------------------------------------------------- Utils
bool isWhitespace(char ch) {
    return (ch==' ') || (ch=='\t') || (ch=='\n') || (ch=='\r');
}

bool isNumeric(char ch) {
    return (ch>='0') && (ch<='9');
}
bool isNumber(const std::string &str) {
    for (size_t i=0;i<str.size();i++)
      if (!isNumeric(str[i])) return false;
    return true;
}
bool isHexadecimal(char ch) {
    return ((ch>='0') && (ch<='9')) ||
           ((ch>='a') && (ch<='f')) ||
           ((ch>='A') && (ch<='F'));
}
bool isAlpha(char ch) {
    return ((ch>='a') && (ch<='z')) || ((ch>='A') && (ch<='Z')) || ch=='_';
}

void replace(std::string &str, char textFrom, const char *textTo) {
    int sLen = strlen(textTo);
    size_t p = str.find(textFrom);
    while (p != std::string::npos) {
        str = str.substr(0, p) + textTo + str.substr(p+1);
        p = str.find(textFrom, p+sLen);
    }
}

/// convert the given string into a quoted string suitable for javascript
std::string getJSString(const std::string &str) {
    std::string nStr = str;
    for (size_t i=0;i<nStr.size();i++) {
      const char *replaceWith = "";
      bool replace = true;

      switch (nStr[i]) {
        case '\\': replaceWith = "\\\\"; break;
        case '\n': replaceWith = "\\n"; break;
        case '\r': replaceWith = "\\r"; break;
        case '\a': replaceWith = "\\a"; break;
        case '"': replaceWith = "\\\""; break;
        default: {
          int nCh = ((int)nStr[i]) &0xFF;
          if (nCh<32 || nCh>127) {
            char buffer[5];
            sprintf_s(buffer, 5, "\\x%02X", nCh);
            replaceWith = buffer;
          } else replace=false;
        }
      }

      if (replace) {
        nStr = nStr.substr(0, i) + replaceWith + nStr.substr(i+1);
        i += strlen(replaceWith)-1;
      }
    }
    return "\"" + nStr + "\"";
}

/** Is the string alphanumeric */
bool isAlphaNum(const std::string &str) {
    if (str.size()==0) return true;
    if (!isAlpha(str[0])) return false;
    for (size_t i=0;i<str.size();i++)
      if (!(isAlpha(str[i]) || isNumeric(str[i])))
        return false;
    return true;
}

// ----------------------------------------------------------------------------------- EXCEPTION

Exception::Exception(const std::string &exceptionText)
    : text(exceptionText) {
}

// ----------------------------------------------------------------------------------- LEXER

Lexer::Lexer(const std::string &input) {
    data = _strdup(input.c_str());
    dataOwned = true;
    dataStart = 0;
    dataEnd = strlen(data);
    reset();
}

Lexer::Lexer(Lexer *owner, int startChar, int endChar) {
    data = owner->data;
    dataOwned = false;
    dataStart = startChar;
    dataEnd = endChar;
    reset();
}

Lexer::~Lexer(void)
{
    if (dataOwned)
        free((void*)data);
}

void Lexer::reset() {
    dataPos = dataStart;
    tokenStart = 0;
    tokenEnd = 0;
    tokenLastEnd = 0;
    tk = 0;
    tkStr = "";
    getNextCh();
    getNextCh();
    getNextToken();
}

void Lexer::match(int expected_tk) {
    if (tk!=expected_tk) {
        std::ostringstream errorString;
        errorString << "Got " << getTokenStr(tk) << " expected " << getTokenStr(expected_tk)
         << " at " << getPosition(tokenStart);
        throw new Exception(errorString.str());
    }
    getNextToken();
}

std::string Lexer::getTokenStr(int token) {
    if (token>32 && token<128) {
        char buf[4] = "' '";
        buf[1] = (char)token;
        return buf;
    }
    switch (token) {
        case LEXER_EOF : return "EOF";
        case LEXER_ID : return "ID";
        case LEXER_INT : return "INT";
        case LEXER_FLOAT : return "FLOAT";
        case LEXER_STR : return "STRING";
        case LEXER_EQUAL : return "==";
        case LEXER_TYPEEQUAL : return "===";
        case LEXER_NEQUAL : return "!=";
        case LEXER_NTYPEEQUAL : return "!==";
        case LEXER_LEQUAL : return "<=";
        case LEXER_LSHIFT : return "<<";
        case LEXER_LSHIFTEQUAL : return "<<=";
        case LEXER_GEQUAL : return ">=";
        case LEXER_RSHIFT : return ">>";
        case LEXER_RSHIFTUNSIGNED : return ">>";
        case LEXER_RSHIFTEQUAL : return ">>=";
        case LEXER_PLUSEQUAL : return "+=";
        case LEXER_MINUSEQUAL : return "-=";
        case LEXER_PLUSPLUS : return "++";
        case LEXER_MINUSMINUS : return "--";
        case LEXER_ANDEQUAL : return "&=";
        case LEXER_ANDAND : return "&&";
        case LEXER_OREQUAL : return "|=";
        case LEXER_OROR : return "||";
        case LEXER_XOREQUAL : return "^=";
                // reserved words
        case LEXER_RESERVED_IF : return "if";
        case LEXER_RESERVED_ELSE : return "else";
        case LEXER_RESERVED_DO : return "do";
        case LEXER_RESERVED_WHILE : return "while";
        case LEXER_RESERVED_FOR : return "for";
        case LEXER_RESERVED_BREAK : return "break";
        case LEXER_RESERVED_CONTINUE : return "continue";
        case LEXER_RESERVED_FUNCTION : return "function";
        case LEXER_RESERVED_RETURN : return "return";
        case LEXER_RESERVED_VAR : return "var";
        case LEXER_RESERVED_TRUE : return "true";
        case LEXER_RESERVED_FALSE : return "false";
        case LEXER_RESERVED_NULL : return "null";
        case LEXER_RESERVED_UNDEFINED : return "undefined";
        case LEXER_RESERVED_NEW : return "new";
    }

    std::ostringstream msg;
    msg << "?[" << token << "]";
    return msg.str();
}

void Lexer::getNextCh() {
    currCh = nextCh;
    if (dataPos < dataEnd)
        nextCh = data[dataPos];
    else
        nextCh = 0;
    dataPos++;
}

void Lexer::getNextToken() {
    tk = LEXER_EOF;
    tkStr.clear();
    while (currCh && isWhitespace(currCh)) getNextCh();
    // newline comments
    if (currCh=='/' && nextCh=='/') {
        while (currCh && currCh!='\n') getNextCh();
        getNextCh();
        getNextToken();
        return;
    }
    // block comments
    if (currCh=='/' && nextCh=='*') {
        while (currCh && (currCh!='*' || nextCh!='/')) getNextCh();
        getNextCh();
        getNextCh();
        getNextToken();
        return;
    }
    // record beginning of this token
    tokenStart = dataPos-2;
    // tokens
    if (isAlpha(currCh)) { //  IDs
        while (isAlpha(currCh) || isNumeric(currCh)) {
            tkStr += currCh;
            getNextCh();
        }
        tk = LEXER_ID;
             if (tkStr=="if") tk = LEXER_RESERVED_IF;
        else if (tkStr=="else") tk = LEXER_RESERVED_ELSE;
        else if (tkStr=="do") tk = LEXER_RESERVED_DO;
        else if (tkStr=="while") tk = LEXER_RESERVED_WHILE;
        else if (tkStr=="for") tk = LEXER_RESERVED_FOR;
        else if (tkStr=="break") tk = LEXER_RESERVED_BREAK;
        else if (tkStr=="continue") tk = LEXER_RESERVED_CONTINUE;
        else if (tkStr=="function") tk = LEXER_RESERVED_FUNCTION;
        else if (tkStr=="return") tk = LEXER_RESERVED_RETURN;
        else if (tkStr=="var") tk = LEXER_RESERVED_VAR;
        else if (tkStr=="true") tk = LEXER_RESERVED_TRUE;
        else if (tkStr=="false") tk = LEXER_RESERVED_FALSE;
        else if (tkStr=="null") tk = LEXER_RESERVED_NULL;
        else if (tkStr=="undefined") tk = LEXER_RESERVED_UNDEFINED;
        else if (tkStr=="new") tk = LEXER_RESERVED_NEW;
    } else if (isNumeric(currCh)) { // Numbers
        bool isHex = false;
        if (currCh=='0') { tkStr += currCh; getNextCh(); }
        if (currCh=='x') {
          isHex = true;
          tkStr += currCh; getNextCh();
        }
        tk = LEXER_INT;
        while (isNumeric(currCh) || (isHex && isHexadecimal(currCh))) {
            tkStr += currCh;
            getNextCh();
        }
        if (!isHex && currCh=='.') {
            tk = LEXER_FLOAT;
            tkStr += '.';
            getNextCh();
            while (isNumeric(currCh)) {
                tkStr += currCh;
                getNextCh();
            }
        }
        // do fancy e-style floating point
        if (!isHex && (currCh=='e'||currCh=='E')) {
          tk = LEXER_FLOAT;
          tkStr += currCh; getNextCh();
          if (currCh=='-') { tkStr += currCh; getNextCh(); }
          while (isNumeric(currCh)) {
             tkStr += currCh; getNextCh();
          }
        }
    } else if (currCh=='"') {
        // strings...
        getNextCh();
        while (currCh && currCh!='"') {
            if (currCh == '\\') {
                getNextCh();
                switch (currCh) {
                case 'n' : tkStr += '\n'; break;
                case '"' : tkStr += '"'; break;
                case '\\' : tkStr += '\\'; break;
                default: tkStr += currCh;
                }
            } else {
                tkStr += currCh;
            }
            getNextCh();
        }
        getNextCh();
        tk = LEXER_STR;
    } else if (currCh=='\'') {
        // strings again...
        getNextCh();
        while (currCh && currCh!='\'') {
            if (currCh == '\\') {
                getNextCh();
                switch (currCh) {
                case 'n' : tkStr += '\n'; break;
                case 'a' : tkStr += '\a'; break;
                case 'r' : tkStr += '\r'; break;
                case 't' : tkStr += '\t'; break;
                case '\'' : tkStr += '\''; break;
                case '\\' : tkStr += '\\'; break;
                case 'x' : { // hex digits
                              char buf[3] = "??";
                              getNextCh(); buf[0] = currCh;
                              getNextCh(); buf[1] = currCh;
                              tkStr += (char)strtol(buf,0,16);
                           } break;
                default: if (currCh>='0' && currCh<='7') {
                           // octal digits
                           char buf[4] = "???";
                           buf[0] = currCh;
                           getNextCh(); buf[1] = currCh;
                           getNextCh(); buf[2] = currCh;
                           tkStr += (char)strtol(buf,0,8);
                         } else
                           tkStr += currCh;
                }
            } else {
                tkStr += currCh;
            }
            getNextCh();
        }
        getNextCh();
        tk = LEXER_STR;
    } else {
        // single chars
        tk = currCh;
        if (currCh) getNextCh();
        if (tk=='=' && currCh=='=') { // ==
            tk = LEXER_EQUAL;
            getNextCh();
            if (currCh=='=') { // ===
              tk = LEXER_TYPEEQUAL;
              getNextCh();
            }
        } else if (tk=='!' && currCh=='=') { // !=
            tk = LEXER_NEQUAL;
            getNextCh();
            if (currCh=='=') { // !==
              tk = LEXER_NTYPEEQUAL;
              getNextCh();
            }
        } else if (tk=='<' && currCh=='=') {
            tk = LEXER_LEQUAL;
            getNextCh();
        } else if (tk=='<' && currCh=='<') {
            tk = LEXER_LSHIFT;
            getNextCh();
            if (currCh=='=') { // <<=
              tk = LEXER_LSHIFTEQUAL;
              getNextCh();
            }
        } else if (tk=='>' && currCh=='=') {
            tk = LEXER_GEQUAL;
            getNextCh();
        } else if (tk=='>' && currCh=='>') {
            tk = LEXER_RSHIFT;
            getNextCh();
            if (currCh=='=') { // >>=
              tk = LEXER_RSHIFTEQUAL;
              getNextCh();
            } else if (currCh=='>') { // >>>
              tk = LEXER_RSHIFTUNSIGNED;
              getNextCh();
            }
        }  else if (tk=='+' && currCh=='=') {
            tk = LEXER_PLUSEQUAL;
            getNextCh();
        }  else if (tk=='-' && currCh=='=') {
            tk = LEXER_MINUSEQUAL;
            getNextCh();
        }  else if (tk=='+' && currCh=='+') {
            tk = LEXER_PLUSPLUS;
            getNextCh();
        }  else if (tk=='-' && currCh=='-') {
            tk = LEXER_MINUSMINUS;
            getNextCh();
        } else if (tk=='&' && currCh=='=') {
            tk = LEXER_ANDEQUAL;
            getNextCh();
        } else if (tk=='&' && currCh=='&') {
            tk = LEXER_ANDAND;
            getNextCh();
        } else if (tk=='|' && currCh=='=') {
            tk = LEXER_OREQUAL;
            getNextCh();
        } else if (tk=='|' && currCh=='|') {
            tk = LEXER_OROR;
            getNextCh();
        } else if (tk=='^' && currCh=='=') {
            tk = LEXER_XOREQUAL;
            getNextCh();
        }
    }
    /* This isn't quite right yet */
    tokenLastEnd = tokenEnd;
    tokenEnd = dataPos-3;
}

std::string Lexer::getSubString(int lastPosition) {
    int lastCharIdx = tokenLastEnd+1;
    if (lastCharIdx < dataEnd) {
        /* save a memory alloc by using our data array to create the
           substring */
        char old = data[lastCharIdx];
        data[lastCharIdx] = 0;
        std::string value = &data[lastPosition];
        data[lastCharIdx] = old;
        return value;
    } else {
        return std::string(&data[lastPosition]);
    }
}


Lexer *Lexer::getSubLex(int lastPosition) {
    int lastCharIdx = tokenLastEnd+1;
    if (lastCharIdx < dataEnd)
        return new Lexer(this, lastPosition, lastCharIdx);
    else
        return new Lexer(this, lastPosition, dataEnd);
}

std::string Lexer::getPosition(int pos) {
    if (pos<0) pos=tokenLastEnd;
    int line = 1,col = 1;
    for (int i=0;i<pos;i++) {
        char ch;
        if (i < dataEnd)
            ch = data[i];
        else
            ch = 0;
        col++;
        if (ch=='\n') {
            line++;
            col = 0;
        }
    }
    char buf[256];
    sprintf_s(buf, 256, "(line: %d, col: %d)", line, col);
    return buf;
}

// ----------------------------------------------------------------------------------- CSCRIPTVARLINK

VariableLink::VariableLink(Variable *var, const std::string &varName)
    : name(varName) {
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    this->nextSibling = 0;
    this->prevSibling = 0;
    this->var = var->ref();
    this->owned = false;
}

VariableLink::VariableLink(const VariableLink &link)
    : name(link.name) {
    // Copy constructor
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    this->nextSibling = 0;
    this->prevSibling = 0;
    this->var = link.var->ref();
    this->owned = false;
}

VariableLink::~VariableLink() {
#if DEBUG_MEMORY
    mark_deallocated(this);
#endif
    var->unref();
}

void VariableLink::replaceWith(Variable *newVar) {
    Variable *oldVar = var;
    var = newVar->ref();
    oldVar->unref();
}

void VariableLink::replaceWith(VariableLink *newVar) {
    if (newVar)
      replaceWith(newVar->var);
    else
      replaceWith(new Variable());
}

int VariableLink::getIntName() {
    return atoi(name.c_str());
}

void VariableLink::setIntName(int n) {
    char sIdx[64];
    sprintf_s(sIdx, sizeof(sIdx), "%d", n);
    name = sIdx;
}

// ----------------------------------------------------------------------------------- VARIABLE

Variable::Variable() {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    flags = VARIABLE_UNDEFINED;
}

Variable::Variable(const std::string &str) {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    flags = VARIABLE_STRING;
    data = str;
}


Variable::Variable(const std::string &varData, int varFlags) {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    flags = varFlags;
    if (varFlags & VARIABLE_INTEGER) {
      intData = strtol(varData.c_str(),0,0);
    } else if (varFlags & VARIABLE_DOUBLE) {
      doubleData = strtod(varData.c_str(),0);
    } else
      data = varData;
}

Variable::Variable(double val) {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    setDouble(val);
}

Variable::Variable(int val) {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    setInt(val);
}

Variable::~Variable(void) {
#if DEBUG_MEMORY
    mark_deallocated(this);
#endif
    removeAllChildren();
}

void Variable::init() {
    firstChild = 0;
    lastChild = 0;
    flags = 0;
    jsCallback = 0;
    jsCallbackUserData = 0;
    data = TINYJS_BLANK_DATA;
    intData = 0;
    doubleData = 0;
}

Variable *Variable::getReturnVar() {
    return getParameter(TINYJS_RETURN_VAR);
}

void Variable::setReturnVar(Variable *var) {
    findChildOrCreate(TINYJS_RETURN_VAR)->replaceWith(var);
}


Variable *Variable::getParameter(const std::string &name) {
    return findChildOrCreate(name)->var;
}

VariableLink *Variable::findChild(const std::string &childName) {
    VariableLink *v = firstChild;
    while (v) {
        if (v->name.compare(childName)==0)
            return v;
        v = v->nextSibling;
    }
    return 0;
}

VariableLink *Variable::findChildOrCreate(const std::string &childName, int varFlags) {
    VariableLink *l = findChild(childName);
    if (l) return l;

    return addChild(childName, new Variable(TINYJS_BLANK_DATA, varFlags));
}

VariableLink *Variable::findChildOrCreateByPath(const std::string &path) {
  size_t p = path.find('.');
  if (p == std::string::npos)
    return findChildOrCreate(path);

  return findChildOrCreate(path.substr(0,p), VARIABLE_OBJECT)->var->
            findChildOrCreateByPath(path.substr(p+1));
}

VariableLink *Variable::addChild(const std::string &childName, Variable *child) {
  if (isUndefined()) {
    flags = VARIABLE_OBJECT;
  }
    // if no child supplied, create one
    if (!child)
      child = new Variable();

    VariableLink *link = new VariableLink(child, childName);
    link->owned = true;
    if (lastChild) {
        lastChild->nextSibling = link;
        link->prevSibling = lastChild;
        lastChild = link;
    } else {
        firstChild = link;
        lastChild = link;
    }
    return link;
}

VariableLink *Variable::addChildNoDup(const std::string &childName, Variable *child) {
    // if no child supplied, create one
    if (!child)
      child = new Variable();

    VariableLink *v = findChild(childName);
    if (v) {
        v->replaceWith(child);
    } else {
        v = addChild(childName, child);
    }

    return v;
}

void Variable::removeChild(Variable *child) {
    VariableLink *link = firstChild;
    while (link) {
        if (link->var == child)
            break;
        link = link->nextSibling;
    }
    ASSERT(link);
    removeLink(link);
}

void Variable::removeLink(VariableLink *link) {
    if (!link) return;
    if (link->nextSibling)
      link->nextSibling->prevSibling = link->prevSibling;
    if (link->prevSibling)
      link->prevSibling->nextSibling = link->nextSibling;
    if (lastChild == link)
        lastChild = link->prevSibling;
    if (firstChild == link)
        firstChild = link->nextSibling;
    delete link;
}

void Variable::removeAllChildren() {
    VariableLink *c = firstChild;
    while (c) {
        VariableLink *t = c->nextSibling;
        delete c;
        c = t;
    }
    firstChild = 0;
    lastChild = 0;
}

Variable *Variable::getArrayIndex(int idx) {
    char sIdx[64];
    sprintf_s(sIdx, sizeof(sIdx), "%d", idx);
    VariableLink *link = findChild(sIdx);
    if (link) return link->var;
    else return new Variable(TINYJS_BLANK_DATA, VARIABLE_NULL); // undefined
}

void Variable::setArrayIndex(int idx, Variable *value) {
    char sIdx[64];
    sprintf_s(sIdx, sizeof(sIdx), "%d", idx);
    VariableLink *link = findChild(sIdx);

    if (link) {
      if (value->isUndefined())
        removeLink(link);
      else
        link->replaceWith(value);
    } else {
      if (!value->isUndefined())
        addChild(sIdx, value);
    }
}

int Variable::getArrayLength() {
    int highest = -1;
    if (!isArray()) return 0;

    VariableLink *link = firstChild;
    while (link) {
      if (isNumber(link->name)) {
        int val = atoi(link->name.c_str());
        if (val > highest) highest = val;
      }
      link = link->nextSibling;
    }
    return highest+1;
}

int Variable::getChildren() {
    int n = 0;
    VariableLink *link = firstChild;
    while (link) {
      n++;
      link = link->nextSibling;
    }
    return n;
}

int Variable::getInt() {
    /* strtol understands about hex and octal */
    if (isInt()) return intData;
    if (isNull()) return 0;
    if (isUndefined()) return 0;
    if (isDouble()) return (int)doubleData;
    return 0;
}

double Variable::getDouble() {
    if (isDouble()) return doubleData;
    if (isInt()) return intData;
    if (isNull()) return 0;
    if (isUndefined()) return 0;
    return 0; /* or NaN? */
}

const std::string &Variable::getString() {
    /* Because we can't return a string that is generated on demand.
     * I should really just use char* :) */
    static std::string s_null = "null";
    static std::string s_undefined = "undefined";
    if (isInt()) {
      char buffer[32];
      sprintf_s(buffer, sizeof(buffer), "%ld", intData);
      data = buffer;
      return data;
    }
    if (isDouble()) {
      char buffer[32];
      sprintf_s(buffer, sizeof(buffer), "%f", doubleData);
      data = buffer;
      return data;
    }
    if (isNull()) return s_null;
    if (isUndefined()) return s_undefined;
    // are we just a string here?
    return data;
}

void Variable::setInt(int val) {
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_INTEGER;
    intData = val;
    doubleData = 0;
    data = TINYJS_BLANK_DATA;
}

void Variable::setDouble(double val) {
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_DOUBLE;
    doubleData = val;
    intData = 0;
    data = TINYJS_BLANK_DATA;
}

void Variable::setString(const std::string &str) {
    // name sure it's not still a number or integer
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_STRING;
    data = str;
    intData = 0;
    doubleData = 0;
}

void Variable::setUndefined() {
    // name sure it's not still a number or integer
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_UNDEFINED;
    data = TINYJS_BLANK_DATA;
    intData = 0;
    doubleData = 0;
    removeAllChildren();
}

void Variable::setArray() {
    // name sure it's not still a number or integer
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_ARRAY;
    data = TINYJS_BLANK_DATA;
    intData = 0;
    doubleData = 0;
    removeAllChildren();
}

bool Variable::equals(Variable *v) {
    Variable *resV = mathsOp(v, LEXER_EQUAL);
    bool res = resV->getBool();
    delete resV;
    return res;
}

Variable *Variable::mathsOp(Variable *b, int op) {
    Variable *a = this;
    // Type equality check
    if (op == LEXER_TYPEEQUAL || op == LEXER_NTYPEEQUAL) {
      // check type first, then call again to check data
      bool eql = ((a->flags & VARIABLE_TYPEMASK) ==
                  (b->flags & VARIABLE_TYPEMASK));
      if (eql) {
        Variable *contents = a->mathsOp(b, LEXER_EQUAL);
        if (!contents->getBool()) eql = false;
        if (!contents->refs) delete contents;
      }
                 ;
      if (op == LEXER_TYPEEQUAL)
        return new Variable(eql);
      else
        return new Variable(!eql);
    }
    // do maths...
    if (a->isUndefined() && b->isUndefined()) {
      if (op == LEXER_EQUAL) return new Variable(true);
      else if (op == LEXER_NEQUAL) return new Variable(false);
      else return new Variable(); // undefined
    } else if ((a->isNumeric() || a->isUndefined()) &&
               (b->isNumeric() || b->isUndefined())) {
        if (!a->isDouble() && !b->isDouble()) {
            // use ints
            int da = a->getInt();
            int db = b->getInt();
            switch (op) {
                case '+': return new Variable(da+db);
                case '-': return new Variable(da-db);
                case '*': return new Variable(da*db);
                case '/': return new Variable(da/db);
                case '&': return new Variable(da&db);
                case '|': return new Variable(da|db);
                case '^': return new Variable(da^db);
                case '%': return new Variable(da%db);
                case LEXER_EQUAL:     return new Variable(da==db);
                case LEXER_NEQUAL:    return new Variable(da!=db);
                case '<':     return new Variable(da<db);
                case LEXER_LEQUAL:    return new Variable(da<=db);
                case '>':     return new Variable(da>db);
                case LEXER_GEQUAL:    return new Variable(da>=db);
                default: throw new Exception("Operation "+Lexer::getTokenStr(op)+" not supported on the Int datatype");
            }
        } else {
            // use doubles
            double da = a->getDouble();
            double db = b->getDouble();
            switch (op) {
                case '+': return new Variable(da+db);
                case '-': return new Variable(da-db);
                case '*': return new Variable(da*db);
                case '/': return new Variable(da/db);
                case LEXER_EQUAL:     return new Variable(da==db);
                case LEXER_NEQUAL:    return new Variable(da!=db);
                case '<':     return new Variable(da<db);
                case LEXER_LEQUAL:    return new Variable(da<=db);
                case '>':     return new Variable(da>db);
                case LEXER_GEQUAL:    return new Variable(da>=db);
                default: throw new Exception("Operation "+Lexer::getTokenStr(op)+" not supported on the Double datatype");
            }
        }
    } else if (a->isArray()) {
      /* Just check pointers */
      switch (op) {
           case LEXER_EQUAL: return new Variable(a==b);
           case LEXER_NEQUAL: return new Variable(a!=b);
           default: throw new Exception("Operation "+Lexer::getTokenStr(op)+" not supported on the Array datatype");
      }
    } else if (a->isObject()) {
          /* Just check pointers */
          switch (op) {
               case LEXER_EQUAL: return new Variable(a==b);
               case LEXER_NEQUAL: return new Variable(a!=b);
               default: throw new Exception("Operation "+Lexer::getTokenStr(op)+" not supported on the Object datatype");
          }
    } else {
       std::string da = a->getString();
       std::string db = b->getString();
       // use strings
       switch (op) {
           case '+':           return new Variable(da+db, VARIABLE_STRING);
           case LEXER_EQUAL:     return new Variable(da==db);
           case LEXER_NEQUAL:    return new Variable(da!=db);
           case '<':     return new Variable(da<db);
           case LEXER_LEQUAL:    return new Variable(da<=db);
           case '>':     return new Variable(da>db);
           case LEXER_GEQUAL:    return new Variable(da>=db);
           default: throw new Exception("Operation "+Lexer::getTokenStr(op)+" not supported on the string datatype");
       }
    }
    ASSERT(0);
    return 0;
}

void Variable::copySimpleData(Variable *val) {
    data = val->data;
    intData = val->intData;
    doubleData = val->doubleData;
    flags = (flags & ~VARIABLE_TYPEMASK) | (val->flags & VARIABLE_TYPEMASK);
}

void Variable::copyValue(Variable *val) {
    if (val) {
      copySimpleData(val);
      // remove all current children
      removeAllChildren();
      // copy children of 'val'
      VariableLink *child = val->firstChild;
      while (child) {
        Variable *copied;
        // don't copy the 'parent' object...
        if (child->name != TINYJS_PROTOTYPE_CLASS)
          copied = child->var->deepCopy();
        else
          copied = child->var;

        addChild(child->name, copied);

        child = child->nextSibling;
      }
    } else {
      setUndefined();
    }
}

Variable *Variable::deepCopy() {
    Variable *newVar = new Variable();
    newVar->copySimpleData(this);
    // copy children
    VariableLink *child = firstChild;
    while (child) {
        Variable *copied;
        // don't copy the 'parent' object...
        if (child->name != TINYJS_PROTOTYPE_CLASS)
          copied = child->var->deepCopy();
        else
          copied = child->var;

        newVar->addChild(child->name, copied);
        child = child->nextSibling;
    }
    return newVar;
}

void Variable::trace(std::string indentStr, const std::string &name) {
    TRACE("%s'%s' = '%s' %s\n",
        indentStr.c_str(),
        name.c_str(),
        getString().c_str(),
        getFlagsAsString().c_str());
    std::string indent = indentStr+" ";
    VariableLink *link = firstChild;
    while (link) {
      link->var->trace(indent, link->name);
      link = link->nextSibling;
    }
}

std::string Variable::getFlagsAsString() {
  std::string flagstr = "";
  if (flags&VARIABLE_FUNCTION) flagstr = flagstr + "FUNCTION ";
  if (flags&VARIABLE_OBJECT) flagstr = flagstr + "OBJECT ";
  if (flags&VARIABLE_ARRAY) flagstr = flagstr + "ARRAY ";
  if (flags&VARIABLE_NATIVE) flagstr = flagstr + "NATIVE ";
  if (flags&VARIABLE_DOUBLE) flagstr = flagstr + "DOUBLE ";
  if (flags&VARIABLE_INTEGER) flagstr = flagstr + "INTEGER ";
  if (flags&VARIABLE_STRING) flagstr = flagstr + "STRING ";
  return flagstr;
}

std::string Variable::getParsableString() {
  // Numbers can just be put in directly
  if (isNumeric())
    return getString();
  if (isFunction()) {
    std::ostringstream funcStr;
    funcStr << "function (";
    // get list of parameters
    VariableLink *link = firstChild;
    while (link) {
      funcStr << link->name;
      if (link->nextSibling) funcStr << ",";
      link = link->nextSibling;
    }
    // add function body
    funcStr << ") " << getString();
    return funcStr.str();
  }
  // if it is a string then we quote it
  if (isString())
    return getJSString(getString());
  if (isNull())
      return "null";
  return "undefined";
}

void Variable::getJSON(std::ostringstream &destination, const std::string &linePrefix) {
   if (isObject()) {
      std::string indentedLinePrefix = linePrefix+"  ";
      // children - handle with bracketed list
      destination << "{ \n";
      VariableLink *link = firstChild;
      while (link) {
        destination << indentedLinePrefix;
        destination  << getJSString(link->name);
        destination  << " : ";
        link->var->getJSON(destination, indentedLinePrefix);
        link = link->nextSibling;
        if (link) {
          destination  << ",\n";
        }
      }
      destination << "\n" << linePrefix << "}";
    } else if (isArray()) {
      std::string indentedLinePrefix = linePrefix+"  ";
      destination << "[\n";
      int len = getArrayLength();
      if (len>10000) len=10000; // we don't want to get stuck here!

      for (int i=0;i<len;i++) {
        getArrayIndex(i)->getJSON(destination, indentedLinePrefix);
        if (i<len-1) destination  << ",\n";
      }

      destination << "\n" << linePrefix << "]";
    } else {
      // no children or a function... just write value directly
      destination << getParsableString();
    }
}


void Variable::setCallback(JSCallback callback, void *userdata) {
    jsCallback = callback;
    jsCallbackUserData = userdata;
}

Variable *Variable::ref() {
    refs++;
    return this;
}

void Variable::unref() {
    if (refs<=0) printf("OMFG, we have unreffed too far!\n");
    if ((--refs)==0) {
      delete this;
    }
}

int Variable::getRefs() {
    return refs;
}


// ----------------------------------------------------------------------------------- INTERPRETER

Interpreter::Interpreter() {
    l = 0;
    root = (new Variable(TINYJS_BLANK_DATA, VARIABLE_OBJECT))->ref();
    // Add built-in classes
    stringClass = (new Variable(TINYJS_BLANK_DATA, VARIABLE_OBJECT))->ref();
    arrayClass = (new Variable(TINYJS_BLANK_DATA, VARIABLE_OBJECT))->ref();
    objectClass = (new Variable(TINYJS_BLANK_DATA, VARIABLE_OBJECT))->ref();
    root->addChild("String", stringClass);
    root->addChild("Array", arrayClass);
    root->addChild("Object", objectClass);
}

Interpreter::~Interpreter() {
    ASSERT(!l);
    scopes.clear();
    stringClass->unref();
    arrayClass->unref();
    objectClass->unref();
    root->unref();

#if DEBUG_MEMORY
    show_allocated();
#endif
}

void Interpreter::trace() {
    root->trace();
}

void Interpreter::execute(const std::string &code) {
    Lexer *oldLex = l;
    std::vector<Variable*> oldScopes = scopes;
    l = new Lexer(code);
#ifdef TINYJS_CALL_STACK
    call_stack.clear();
#endif
    scopes.clear();
    scopes.push_back(root);
    try {
        bool execute = true;
        while (l->tk) statement(execute);
    } catch (Exception *e) {
        std::ostringstream msg;
        msg << "Error " << e->text;
#ifdef TINYJS_CALL_STACK
        for (int i=(int)call_stack.size()-1;i>=0;i--)
          msg << "\n" << i << ": " << call_stack.at(i);
#endif
        msg << " at " << l->getPosition();
        delete l;
        l = oldLex;

        throw new Exception(msg.str());
    }
    delete l;
    l = oldLex;
    scopes = oldScopes;
}

VariableLink Interpreter::evaluateComplex(const std::string &code) {
    Lexer *oldLex = l;
    std::vector<Variable*> oldScopes = scopes;

    l = new Lexer(code);
#ifdef TINYJS_CALL_STACK
    call_stack.clear();
#endif
    scopes.clear();
    scopes.push_back(root);
    VariableLink *v = 0;
    try {
        bool execute = true;
        do {
          CLEAN(v);
          v = base(execute);
          if (l->tk!=LEXER_EOF) l->match(';');
        } while (l->tk!=LEXER_EOF);
    } catch (Exception *e) {
      std::ostringstream msg;
      msg << "Error " << e->text;
#ifdef TINYJS_CALL_STACK
      for (int i=(int)call_stack.size()-1;i>=0;i--)
        msg << "\n" << i << ": " << call_stack.at(i);
#endif
      msg << " at " << l->getPosition();
      delete l;
      l = oldLex;

      throw new Exception(msg.str());
    }
    delete l;
    l = oldLex;
    scopes = oldScopes;

    if (v) {
        VariableLink r = *v;
        CLEAN(v);
        return r;
    }
    // return undefined...
    return VariableLink(new Variable());
}

std::string Interpreter::evaluate(const std::string &code) {
    return evaluateComplex(code).var->getString();
}

void Interpreter::parseFunctionArguments(Variable *funcVar) {
  l->match('(');
  while (l->tk!=')') {
      funcVar->addChildNoDup(l->tkStr);
      l->match(LEXER_ID);
      if (l->tk!=')') l->match(',');
  }
  l->match(')');
}

void Interpreter::addNative(const std::string &funcDesc, JSCallback ptr, void *userdata) {
    Lexer *oldLex = l;
    l = new Lexer(funcDesc);

    Variable *base = root;

    l->match(LEXER_RESERVED_FUNCTION);
    std::string funcName = l->tkStr;
    l->match(LEXER_ID);
    /* Check for dots, we might want to do something like function String.substring ... */
    while (l->tk == '.') {
      l->match('.');
      VariableLink *link = base->findChild(funcName);
      // if it doesn't exist, make an object class
      if (!link) link = base->addChild(funcName, new Variable(TINYJS_BLANK_DATA, VARIABLE_OBJECT));
      base = link->var;
      funcName = l->tkStr;
      l->match(LEXER_ID);
    }

    Variable *funcVar = new Variable(TINYJS_BLANK_DATA, VARIABLE_FUNCTION | VARIABLE_NATIVE);
    funcVar->setCallback(ptr, userdata);
    parseFunctionArguments(funcVar);
    delete l;
    l = oldLex;

    base->addChild(funcName, funcVar);
}

VariableLink *Interpreter::parseFunctionDefinition() {
  // actually parse a function...
  l->match(LEXER_RESERVED_FUNCTION);
  std::string funcName = TINYJS_TEMP_NAME;
  /* we can have functions without names */
  if (l->tk==LEXER_ID) {
    funcName = l->tkStr;
    l->match(LEXER_ID);
  }
  VariableLink *funcVar = new VariableLink(new Variable(TINYJS_BLANK_DATA, VARIABLE_FUNCTION), funcName);
  parseFunctionArguments(funcVar->var);
  int funcBegin = l->tokenStart;
  bool noexecute = false;
  block(noexecute);
  funcVar->var->data = l->getSubString(funcBegin);
  return funcVar;
}

/** Handle a function call (assumes we've parsed the function name and we're
 * on the start bracket). 'parent' is the object that contains this method,
 * if there was one (otherwise it's just a normnal function).
 */
VariableLink *Interpreter::functionCall(bool &execute, VariableLink *function, Variable *parent) {
  if (execute) {
    if (!function->var->isFunction()) {
      std::ostringstream msg;
      msg << "Expecting '" << function->name << "' to be a function";
      throw new Exception(msg.str());
    }
    l->match('(');
    // create a new symbol table entry for execution of this function
    Variable *functionRoot = new Variable(TINYJS_BLANK_DATA, VARIABLE_FUNCTION);
    if (parent)
      functionRoot->addChildNoDup("this", parent);
    // grab in all parameters
    VariableLink *v = function->var->firstChild;
    while (v) {
        VariableLink *value = base(execute);
        if (execute) {
            if (value->var->isBasic()) {
              // pass by value
              functionRoot->addChild(v->name, value->var->deepCopy());
            } else {
              // pass by reference
              functionRoot->addChild(v->name, value->var);
            }
        }
        CLEAN(value);
        if (l->tk!=')') l->match(',');
        v = v->nextSibling;
    }
    l->match(')');
    // setup a return variable
    VariableLink *returnVar = NULL;
    // execute function!
    // add the function's execute space to the symbol table so we can recurse
    VariableLink *returnVarLink = functionRoot->addChild(TINYJS_RETURN_VAR);
    scopes.push_back(functionRoot);
#ifdef TINYJS_CALL_STACK
    call_stack.push_back(function->name + " from " + l->getPosition());
#endif

    if (function->var->isNative()) {
        ASSERT(function->var->jsCallback);
        function->var->jsCallback(functionRoot, function->var->jsCallbackUserData);
    } else {
        /* we just want to execute the block, but something could
         * have messed up and left us with the wrong Lexer, so
         * we want to be careful here... */
        Exception *exception = 0;
        Lexer *oldLex = l;
        Lexer *newLex = new Lexer(function->var->getString());
        l = newLex;
        try {
          block(execute);
          // because return will probably have called this, and set execute to false
          execute = true;
        } catch (Exception *e) {
          exception = e;
        }
        delete newLex;
        l = oldLex;

        if (exception)
          throw exception;
    }
#ifdef TINYJS_CALL_STACK
    if (!call_stack.empty()) call_stack.pop_back();
#endif
    scopes.pop_back();
    /* get the real return var before we remove it from our function */
    returnVar = new VariableLink(returnVarLink->var);
    functionRoot->removeLink(returnVarLink);
    delete functionRoot;
    if (returnVar)
      return returnVar;
    else
      return new VariableLink(new Variable());
  } else {
    // function, but not executing - just parse args and be done
    l->match('(');
    while (l->tk != ')') {
      VariableLink *value = base(execute);
      CLEAN(value);
      if (l->tk!=')') l->match(',');
    }
    l->match(')');
    if (l->tk == '{') { // TODO: why is this here?
      block(execute);
    }
    /* function will be a blank scriptvarlink if we're not executing,
     * so just return it rather than an alloc/free */
    return function;
  }
}

VariableLink *Interpreter::factor(bool &execute) {
    if (l->tk=='(') {
        l->match('(');
        VariableLink *a = base(execute);
        l->match(')');
        return a;
    }
    if (l->tk==LEXER_RESERVED_TRUE) {
        l->match(LEXER_RESERVED_TRUE);
        return new VariableLink(new Variable(1));
    }
    if (l->tk==LEXER_RESERVED_FALSE) {
        l->match(LEXER_RESERVED_FALSE);
        return new VariableLink(new Variable(0));
    }
    if (l->tk==LEXER_RESERVED_NULL) {
        l->match(LEXER_RESERVED_NULL);
        return new VariableLink(new Variable(TINYJS_BLANK_DATA,VARIABLE_NULL));
    }
    if (l->tk==LEXER_RESERVED_UNDEFINED) {
        l->match(LEXER_RESERVED_UNDEFINED);
        return new VariableLink(new Variable(TINYJS_BLANK_DATA,VARIABLE_UNDEFINED));
    }
    if (l->tk==LEXER_ID) {
        VariableLink *a = execute ? findInScopes(l->tkStr) : new VariableLink(new Variable());
        //printf("0x%08X for %s at %s\n", (unsigned int)a, l->tkStr.c_str(), l->getPosition().c_str());
        /* The parent if we're executing a method call */
        Variable *parent = 0;

        if (execute && !a) {
          /* Variable doesn't exist! JavaScript says we should create it
           * (we won't add it here. This is done in the assignment operator)*/
          a = new VariableLink(new Variable(), l->tkStr);
        }
        l->match(LEXER_ID);
        while (l->tk=='(' || l->tk=='.' || l->tk=='[') {
            if (l->tk=='(') { // ------------------------------------- Function Call
                a = functionCall(execute, a, parent);
            } else if (l->tk == '.') { // ------------------------------------- Record Access
                l->match('.');
                if (execute) {
                  const std::string &name = l->tkStr;
                  VariableLink *child = a->var->findChild(name);
                  if (!child) child = findInParentClasses(a->var, name);
                  if (!child) {
                    /* if we haven't found this defined yet, use the built-in
                       'length' properly */
                    if (a->var->isArray() && name == "length") {
                      int l = a->var->getArrayLength();
                      child = new VariableLink(new Variable(l));
                    } else if (a->var->isString() && name == "length") {
                      int l = a->var->getString().size();
                      child = new VariableLink(new Variable(l));
                    } else {
                      child = a->var->addChild(name);
                    }
                  }
                  parent = a->var;
                  a = child;
                }
                l->match(LEXER_ID);
            } else if (l->tk == '[') { // ------------------------------------- Array Access
                l->match('[');
                VariableLink *index = base(execute);
                l->match(']');
                if (execute) {
                  VariableLink *child = a->var->findChildOrCreate(index->var->getString());
                  parent = a->var;
                  a = child;
                }
                CLEAN(index);
            } else ASSERT(0);
        }
        return a;
    }
    if (l->tk==LEXER_INT || l->tk==LEXER_FLOAT) {
        Variable *a = new Variable(l->tkStr,
            ((l->tk==LEXER_INT)?VARIABLE_INTEGER:VARIABLE_DOUBLE));
        l->match(l->tk);
        return new VariableLink(a);
    }
    if (l->tk==LEXER_STR) {
        Variable *a = new Variable(l->tkStr, VARIABLE_STRING);
        l->match(LEXER_STR);
        return new VariableLink(a);
    }
    if (l->tk=='{') {
        Variable *contents = new Variable(TINYJS_BLANK_DATA, VARIABLE_OBJECT);
        /* JSON-style object definition */
        l->match('{');
        while (l->tk != '}') {
          std::string id = l->tkStr;
          // we only allow strings or IDs on the left hand side of an initialisation
          if (l->tk==LEXER_STR) l->match(LEXER_STR);
          else l->match(LEXER_ID);
          l->match(':');
          if (execute) {
            VariableLink *a = base(execute);
            contents->addChild(id, a->var);
            CLEAN(a);
          }
          // no need to clean here, as it will definitely be used
          if (l->tk != '}') l->match(',');
        }

        l->match('}');
        return new VariableLink(contents);
    }
    if (l->tk=='[') {
        Variable *contents = new Variable(TINYJS_BLANK_DATA, VARIABLE_ARRAY);
        /* JSON-style array */
        l->match('[');
        int idx = 0;
        while (l->tk != ']') {
          if (execute) {
            char idx_str[16]; // big enough for 2^32
            sprintf_s(idx_str, sizeof(idx_str), "%d",idx);

            VariableLink *a = base(execute);
            contents->addChild(idx_str, a->var);
            CLEAN(a);
          }
          // no need to clean here, as it will definitely be used
          if (l->tk != ']') l->match(',');
          idx++;
        }
        l->match(']');
        return new VariableLink(contents);
    }
    if (l->tk==LEXER_RESERVED_FUNCTION) {
      VariableLink *funcVar = parseFunctionDefinition();
        if (funcVar->name != TINYJS_TEMP_NAME)
          TRACE("Functions not defined at statement-level are not meant to have a name");
        return funcVar;
    }
    if (l->tk==LEXER_RESERVED_NEW) {
      // new -> create a new object
      l->match(LEXER_RESERVED_NEW);
      const std::string &className = l->tkStr;
      if (execute) {
        VariableLink *objClassOrFunc = findInScopes(className);
        if (!objClassOrFunc) {
          TRACE("%s is not a valid class name", className.c_str());
          return new VariableLink(new Variable());
        }
        l->match(LEXER_ID);
        Variable *obj = new Variable(TINYJS_BLANK_DATA, VARIABLE_OBJECT);
        VariableLink *objLink = new VariableLink(obj);
        if (objClassOrFunc->var->isFunction()) {
          CLEAN(functionCall(execute, objClassOrFunc, obj));
        } else {
          obj->addChild(TINYJS_PROTOTYPE_CLASS, objClassOrFunc->var);
          if (l->tk == '(') {
            l->match('(');
            l->match(')');
          }
        }
        return objLink;
      } else {
        l->match(LEXER_ID);
        if (l->tk == '(') {
          l->match('(');
          l->match(')');
        }
      }
    }
    // Nothing we can do here... just hope it's the end...
    l->match(LEXER_EOF);
    return 0;
}

VariableLink *Interpreter::unary(bool &execute) {
    VariableLink *a;
    if (l->tk=='!') {
        l->match('!'); // binary not
        a = factor(execute);
        if (execute) {
            Variable zero(0);
            Variable *res = a->var->mathsOp(&zero, LEXER_EQUAL);
            CREATE_LINK(a, res);
        }
    } else
        a = factor(execute);
    return a;
}

VariableLink *Interpreter::term(bool &execute) {
    VariableLink *a = unary(execute);
    while (l->tk=='*' || l->tk=='/' || l->tk=='%') {
        int op = l->tk;
        l->match(l->tk);
        VariableLink *b = unary(execute);
        if (execute) {
            Variable *res = a->var->mathsOp(b->var, op);
            CREATE_LINK(a, res);
        }
        CLEAN(b);
    }
    return a;
}

VariableLink *Interpreter::expression(bool &execute) {
    bool negate = false;
    if (l->tk=='-') {
        l->match('-');
        negate = true;
    }
    VariableLink *a = term(execute);
    if (negate) {
        Variable zero(0);
        Variable *res = zero.mathsOp(a->var, '-');
        CREATE_LINK(a, res);
    }

    while (l->tk=='+' || l->tk=='-' ||
        l->tk==LEXER_PLUSPLUS || l->tk==LEXER_MINUSMINUS) {
        int op = l->tk;
        l->match(l->tk);
        if (op==LEXER_PLUSPLUS || op==LEXER_MINUSMINUS) {
            if (execute) {
                Variable one(1);
                Variable *res = a->var->mathsOp(&one, op==LEXER_PLUSPLUS ? '+' : '-');
                VariableLink *oldValue = new VariableLink(a->var);
                // in-place add/subtract
                a->replaceWith(res);
                CLEAN(a);
                a = oldValue;
            }
        } else {
            VariableLink *b = term(execute);
            if (execute) {
                // not in-place, so just replace
                Variable *res = a->var->mathsOp(b->var, op);
                CREATE_LINK(a, res);
            }
            CLEAN(b);
        }
    }
    return a;
}

VariableLink *Interpreter::shift(bool &execute) {
  VariableLink *a = expression(execute);
  if (l->tk==LEXER_LSHIFT || l->tk==LEXER_RSHIFT || l->tk==LEXER_RSHIFTUNSIGNED) {
    int op = l->tk;
    l->match(op);
    VariableLink *b = base(execute);
    int shift = execute ? b->var->getInt() : 0;
    CLEAN(b);
    if (execute) {
      if (op==LEXER_LSHIFT) a->var->setInt(a->var->getInt() << shift);
      if (op==LEXER_RSHIFT) a->var->setInt(a->var->getInt() >> shift);
      if (op==LEXER_RSHIFTUNSIGNED) a->var->setInt(((unsigned int)a->var->getInt()) >> shift);
    }
  }
  return a;
}

VariableLink *Interpreter::condition(bool &execute) {
    VariableLink *a = shift(execute);
    VariableLink *b;
    while (l->tk==LEXER_EQUAL || l->tk==LEXER_NEQUAL ||
           l->tk==LEXER_TYPEEQUAL || l->tk==LEXER_NTYPEEQUAL ||
           l->tk==LEXER_LEQUAL || l->tk==LEXER_GEQUAL ||
           l->tk=='<' || l->tk=='>') {
        int op = l->tk;
        l->match(l->tk);
        b = shift(execute);
        if (execute) {
            Variable *res = a->var->mathsOp(b->var, op);
            CREATE_LINK(a,res);
        }
        CLEAN(b);
    }
    return a;
}

VariableLink *Interpreter::logic(bool &execute) {
    VariableLink *a = condition(execute);
    VariableLink *b;
    while (l->tk=='&' || l->tk=='|' || l->tk=='^' || l->tk==LEXER_ANDAND || l->tk==LEXER_OROR) {
        bool noexecute = false;
        int op = l->tk;
        l->match(l->tk);
        bool shortCircuit = false;
        bool boolean = false;
        // if we have short-circuit ops, then if we know the outcome
        // we don't bother to execute the other op. Even if not
        // we need to tell mathsOp it's an & or |
        if (op==LEXER_ANDAND) {
            op = '&';
            shortCircuit = !a->var->getBool();
            boolean = true;
        } else if (op==LEXER_OROR) {
            op = '|';
            shortCircuit = a->var->getBool();
            boolean = true;
        }
        b = condition(shortCircuit ? noexecute : execute);
        if (execute && !shortCircuit) {
            if (boolean) {
              Variable *newa = new Variable(a->var->getBool());
              Variable *newb = new Variable(b->var->getBool());
              CREATE_LINK(a, newa);
              CREATE_LINK(b, newb);
            }
            Variable *res = a->var->mathsOp(b->var, op);
            CREATE_LINK(a, res);
        }
        CLEAN(b);
    }
    return a;
}

VariableLink *Interpreter::ternary(bool &execute) {
  VariableLink *lhs = logic(execute);
  if (l->tk=='?') {
    bool noexecute = false;
    l->match('?');
    if (!execute) {
      CLEAN(lhs);
      CLEAN(base(noexecute));
      l->match(':');
      CLEAN(base(noexecute));
    } else {
      bool first = lhs->var->getBool();
      CLEAN(lhs);
      if (first) {
        lhs = base(execute);
        l->match(':');
        CLEAN(base(noexecute));
      } else {
        CLEAN(base(noexecute));
        l->match(':');
        lhs = base(execute);
      }
    }
  }

  return lhs;
}

VariableLink *Interpreter::base(bool &execute) {
    VariableLink *lhs = ternary(execute);
    if (l->tk=='=' || l->tk==LEXER_PLUSEQUAL || l->tk==LEXER_MINUSEQUAL) {
        /* If we're assigning to this and we don't have a parent,
         * add it to the symbol table root as per JavaScript. */
        if (execute && !lhs->owned) {
          if (lhs->name.length()>0) {
            VariableLink *realLhs = root->addChildNoDup(lhs->name, lhs->var);
            CLEAN(lhs);
            lhs = realLhs;
          } else
            TRACE("Trying to assign to an un-named type\n");
        }

        int op = l->tk;
        l->match(l->tk);
        VariableLink *rhs = base(execute);
        if (execute) {
            if (op=='=') {
                lhs->replaceWith(rhs);
            } else if (op==LEXER_PLUSEQUAL) {
                Variable *res = lhs->var->mathsOp(rhs->var, '+');
                lhs->replaceWith(res);
            } else if (op==LEXER_MINUSEQUAL) {
                Variable *res = lhs->var->mathsOp(rhs->var, '-');
                lhs->replaceWith(res);
            } else ASSERT(0);
        }
        CLEAN(rhs);
    }
    return lhs;
}

void Interpreter::block(bool &execute) {
    l->match('{');
    if (execute) {
      while (l->tk && l->tk!='}')
        statement(execute);
      l->match('}');
    } else {
      // fast skip of blocks
      int brackets = 1;
      while (l->tk && brackets) {
        if (l->tk == '{') brackets++;
        if (l->tk == '}') brackets--;
        l->match(l->tk);
      }
    }

}

void Interpreter::statement(bool &execute) {
    if (l->tk==LEXER_ID ||
        l->tk==LEXER_INT ||
        l->tk==LEXER_FLOAT ||
        l->tk==LEXER_STR ||
        l->tk=='-') {
        /* Execute a simple statement that only contains basic arithmetic... */
        CLEAN(base(execute));
        l->match(';');
    } else if (l->tk=='{') {
        /* A block of code */
        block(execute);
    } else if (l->tk==';') {
        /* Empty statement - to allow things like ;;; */
        l->match(';');
    } else if (l->tk==LEXER_RESERVED_VAR) {
        /* variable creation. TODO - we need a better way of parsing the left
         * hand side. Maybe just have a flag called can_create_var that we
         * set and then we parse as if we're doing a normal equals.*/
        l->match(LEXER_RESERVED_VAR);
        while (l->tk != ';') {
          VariableLink *a = 0;
          if (execute)
            a = scopes.back()->findChildOrCreate(l->tkStr);
          l->match(LEXER_ID);
          // now do stuff defined with dots
          while (l->tk == '.') {
              l->match('.');
              if (execute) {
                  VariableLink *lastA = a;
                  a = lastA->var->findChildOrCreate(l->tkStr);
              }
              l->match(LEXER_ID);
          }
          // sort out initialiser
          if (l->tk == '=') {
              l->match('=');
              VariableLink *var = base(execute);
              if (execute)
                  a->replaceWith(var);
              CLEAN(var);
          }
          if (l->tk != ';')
            l->match(',');
        }       
        l->match(';');
    } else if (l->tk==LEXER_RESERVED_IF) {
        l->match(LEXER_RESERVED_IF);
        l->match('(');
        VariableLink *var = base(execute);
        l->match(')');
        bool cond = execute && var->var->getBool();
        CLEAN(var);
        bool noexecute = false; // because we need to be abl;e to write to it
        statement(cond ? execute : noexecute);
        if (l->tk==LEXER_RESERVED_ELSE) {
            l->match(LEXER_RESERVED_ELSE);
            statement(cond ? noexecute : execute);
        }
    } else if (l->tk==LEXER_RESERVED_WHILE) {
        // We do repetition by pulling out the string representing our statement
        // there's definitely some opportunity for optimisation here
        l->match(LEXER_RESERVED_WHILE);
        l->match('(');
        int whileCondStart = l->tokenStart;
        bool noexecute = false;
        VariableLink *cond = base(execute);
        bool loopCond = execute && cond->var->getBool();
        CLEAN(cond);
        Lexer *whileCond = l->getSubLex(whileCondStart);
        l->match(')');
        int whileBodyStart = l->tokenStart;
        statement(loopCond ? execute : noexecute);
        Lexer *whileBody = l->getSubLex(whileBodyStart);
        Lexer *oldLex = l;
        int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
        while (loopCond && loopCount-->0) {
            whileCond->reset();
            l = whileCond;
            cond = base(execute);
            loopCond = execute && cond->var->getBool();
            CLEAN(cond);
            if (loopCond) {
                whileBody->reset();
                l = whileBody;
                statement(execute);
            }
        }
        l = oldLex;
        delete whileCond;
        delete whileBody;

        if (loopCount<=0) {
            root->trace();
            TRACE("WHILE Loop exceeded %d iterations at %s\n", TINYJS_LOOP_MAX_ITERATIONS, l->getPosition().c_str());
            throw new Exception("LOOP_ERROR");
        }
    } else if (l->tk==LEXER_RESERVED_FOR) {
        l->match(LEXER_RESERVED_FOR);
        l->match('(');
        statement(execute); // initialisation
        //l->match(';');
        int forCondStart = l->tokenStart;
        bool noexecute = false;
        VariableLink *cond = base(execute); // condition
        bool loopCond = execute && cond->var->getBool();
        CLEAN(cond);
        Lexer *forCond = l->getSubLex(forCondStart);
        l->match(';');
        int forIterStart = l->tokenStart;
        CLEAN(base(noexecute)); // iterator
        Lexer *forIter = l->getSubLex(forIterStart);
        l->match(')');
        int forBodyStart = l->tokenStart;
        statement(loopCond ? execute : noexecute);
        Lexer *forBody = l->getSubLex(forBodyStart);
        Lexer *oldLex = l;
        if (loopCond) {
            forIter->reset();
            l = forIter;
            CLEAN(base(execute));
        }
        int loopCount = TINYJS_LOOP_MAX_ITERATIONS;
        while (execute && loopCond && loopCount-->0) {
            forCond->reset();
            l = forCond;
            cond = base(execute);
            loopCond = cond->var->getBool();
            CLEAN(cond);
            if (execute && loopCond) {
                forBody->reset();
                l = forBody;
                statement(execute);
            }
            if (execute && loopCond) {
                forIter->reset();
                l = forIter;
                CLEAN(base(execute));
            }
        }
        l = oldLex;
        delete forCond;
        delete forIter;
        delete forBody;
        if (loopCount<=0) {
            root->trace();
            TRACE("FOR Loop exceeded %d iterations at %s\n", TINYJS_LOOP_MAX_ITERATIONS, l->getPosition().c_str());
            throw new Exception("LOOP_ERROR");
        }
    } else if (l->tk==LEXER_RESERVED_RETURN) {
        l->match(LEXER_RESERVED_RETURN);
        VariableLink *result = 0;
        if (l->tk != ';')
          result = base(execute);
        if (execute) {
          VariableLink *resultVar = scopes.back()->findChild(TINYJS_RETURN_VAR);
          if (resultVar)
            resultVar->replaceWith(result);
          else
            TRACE("RETURN statement, but not in a function.\n");
          execute = false;
        }
        CLEAN(result);
        l->match(';');
    } else if (l->tk==LEXER_RESERVED_FUNCTION) {
        VariableLink *funcVar = parseFunctionDefinition();
        if (execute) {
          if (funcVar->name == TINYJS_TEMP_NAME)
            TRACE("Functions defined at statement-level are meant to have a name\n");
          else
            scopes.back()->addChildNoDup(funcVar->name, funcVar->var);
        }
        CLEAN(funcVar);
    } else l->match(LEXER_EOF);
}

/// Get the given variable specified by a path (var1.var2.etc), or return 0
Variable *Interpreter::getScriptVariable(const std::string &path) {
    // traverse path
    size_t prevIdx = 0;
    size_t thisIdx = path.find('.');
    if (thisIdx == std::string::npos) thisIdx = path.length();
    Variable *var = root;
    while (var && prevIdx<path.length()) {
        std::string el = path.substr(prevIdx, thisIdx-prevIdx);
        VariableLink *varl = var->findChild(el);
        var = varl?varl->var:0;
        prevIdx = thisIdx+1;
        thisIdx = path.find('.', prevIdx);
        if (thisIdx == std::string::npos) thisIdx = path.length();
    }
    return var;
}

/// get the value of the given variable, return true if it exists and fetched
bool Interpreter::getVariable(const std::string &path, std::string &varData) {
    Variable *var = getScriptVariable(path);
    // return result
    if (var) {
        varData = var->getString();
        return true;
    } else
        return false;
}

/// set the value of the given variable, return true if it exists and gets set
bool Interpreter::setVariable(const std::string &path, const std::string &varData) {
    Variable *var = getScriptVariable(path);
    // return result
    if (var) {
        if (var->isInt())
            var->setInt((int)strtol(varData.c_str(),0,0));
        else if (var->isDouble())
            var->setDouble(strtod(varData.c_str(),0));
        else
            var->setString(varData.c_str());
        return true;
    }    
    else
        return false;
}

/// Finds a child, looking recursively up the scopes
VariableLink *Interpreter::findInScopes(const std::string &childName) {
    for (int s=scopes.size()-1;s>=0;s--) {
      VariableLink *v = scopes[s]->findChild(childName);
      if (v) return v;
    }
    return NULL;

}

/// Look up in any parent classes of the given object
VariableLink *Interpreter::findInParentClasses(Variable *object, const std::string &name) {
    // Look for links to actual parent classes
    VariableLink *parentClass = object->findChild(TINYJS_PROTOTYPE_CLASS);
    while (parentClass) {
      VariableLink *implementation = parentClass->var->findChild(name);
      if (implementation) return implementation;
      parentClass = parentClass->var->findChild(TINYJS_PROTOTYPE_CLASS);
    }
    // else fake it for strings and finally objects
    if (object->isString()) {
      VariableLink *implementation = stringClass->findChild(name);
      if (implementation) return implementation;
    }
    if (object->isArray()) {
      VariableLink *implementation = arrayClass->findChild(name);
      if (implementation) return implementation;
    }
    VariableLink *implementation = objectClass->findChild(name);
    if (implementation) return implementation;

    return 0;
}

}; // namespace TinyJS
