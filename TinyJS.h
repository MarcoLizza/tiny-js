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

#ifndef TINYJS_H
#define TINYJS_H

#ifdef _DEBUG
  // If defined, this keeps a note of all calls and where from in memory. This is slower, but good for debugging
  #define TINYJS_CALL_STACK
#endif

#if defined(_WIN32) && !defined(_WIN32_WCE)
  #ifdef _DEBUG
    #define _CRTDBG_MAP_ALLOC
    #include <stdlib.h>
    #include <crtdbg.h>
  #endif
#endif
#include <string>
#include <vector>

#ifndef TRACE
  #define TRACE printf
#endif // TRACE

namespace TinyJS {

const int TINYJS_LOOP_MAX_ITERATIONS = 8192;

enum LEXER_TYPES {
    LEXER_EOF = 0,
    LEXER_ID = 256,
    LEXER_INT,
    LEXER_FLOAT,
    LEXER_STR,

    LEXER_EQUAL,
    LEXER_TYPEEQUAL,
    LEXER_NEQUAL,
    LEXER_NTYPEEQUAL,
    LEXER_LEQUAL,
    LEXER_LSHIFT,
    LEXER_LSHIFTEQUAL,
    LEXER_GEQUAL,
    LEXER_RSHIFT,
    LEXER_RSHIFTUNSIGNED,
    LEXER_RSHIFTEQUAL,
    LEXER_PLUSEQUAL,
    LEXER_MINUSEQUAL,
    LEXER_PLUSPLUS,
    LEXER_MINUSMINUS,
    LEXER_ANDEQUAL,
    LEXER_ANDAND,
    LEXER_OREQUAL,
    LEXER_OROR,
    LEXER_XOREQUAL,
    // reserved words
#define LEXER_RESERVED_LIST_START LEXER_RESERVED_IF
    LEXER_RESERVED_IF,
    LEXER_RESERVED_ELSE,
    LEXER_RESERVED_DO,
    LEXER_RESERVED_WHILE,
    LEXER_RESERVED_FOR,
    LEXER_RESERVED_BREAK,
    LEXER_RESERVED_CONTINUE,
    LEXER_RESERVED_FUNCTION,
    LEXER_RESERVED_RETURN,
    LEXER_RESERVED_VAR,
    LEXER_RESERVED_TRUE,
    LEXER_RESERVED_FALSE,
    LEXER_RESERVED_NULL,
    LEXER_RESERVED_UNDEFINED,
    LEXER_RESERVED_NEW,

	LEXER_RESERVED_LIST_END /* always the last entry */
};

enum VARIABLE_FLAGS {
    VARIABLE_UNDEFINED   = 0,
    VARIABLE_FUNCTION    = 1,
    VARIABLE_OBJECT      = 2,
    VARIABLE_ARRAY       = 4,
    VARIABLE_DOUBLE      = 8,  // floating point double
    VARIABLE_INTEGER     = 16, // integer number
    VARIABLE_STRING      = 32, // string
    VARIABLE_NULL        = 64, // it seems null is its own data type
    VARIABLE_NATIVE      = 128, // to specify this is a native function
    VARIABLE_NUMERICMASK = VARIABLE_NULL |
                           VARIABLE_DOUBLE |
                           VARIABLE_INTEGER,
    VARIABLE_TYPEMASK = VARIABLE_DOUBLE |
                        VARIABLE_INTEGER |
                        VARIABLE_STRING |
                        VARIABLE_FUNCTION |
                        VARIABLE_OBJECT |
                        VARIABLE_ARRAY |
                        VARIABLE_NULL,
};

#define TINYJS_RETURN_VAR "return"
#define TINYJS_PROTOTYPE_CLASS "prototype"
#define TINYJS_TEMP_NAME ""
#define TINYJS_BLANK_DATA ""

/// convert the given string into a quoted string suitable for javascript
std::string getJSString(const std::string &str);

class Exception {
public:
    std::string text;
    Exception(const std::string &exceptionText);
};

class Lexer
{
public:
    Lexer(const std::string &input);
    Lexer(Lexer *owner, int startChar, int endChar);
    ~Lexer(void);

    char currCh, nextCh;
    int tk; ///< The type of the token that we have
    int tokenStart; ///< Position in the data at the beginning of the token we have here
    int tokenEnd; ///< Position in the data at the last character of the token we have here
    int tokenLastEnd; ///< Position in the data at the last character of the last token
    std::string tkStr; ///< Data contained in the token we have here

    void match(int expected_tk); ///< Lexical match wotsit
    static std::string getTokenStr(int token); ///< Get the string representation of the given token
    void reset(); ///< Reset this lex so we can start again

    std::string getSubString(int pos); ///< Return a sub-string from the given position up until right now
    Lexer *getSubLex(int lastPosition); ///< Return a sub-lexer from the given position up until right now

    std::string getPosition(int pos=-1); ///< Return a string representing the position in lines and columns of the character pos given

protected:
    /* When we go into a loop, we use getSubLex to get a lexer for just the sub-part of the
       relevant string. This doesn't re-allocate and copy the string, but instead copies
       the data pointer and sets dataOwned to false, and dataStart/dataEnd to the relevant things. */
    char *data; ///< Data string to get tokens from
    int dataStart, dataEnd; ///< Start and end position in data string
    bool dataOwned; ///< Do we own this data string?

    int dataPos; ///< Position in data (we CAN go past the end of the string here)

    void getNextCh();
    void getNextToken(); ///< Get the text token from our text string
};

class Variable;

typedef void (*JSCallback)(Variable *var, void *userdata);

class VariableLink
{
public:
  std::string name;
  VariableLink *nextSibling;
  VariableLink *prevSibling;
  Variable *var;
  bool owned;

  VariableLink(Variable *var, const std::string &name = TINYJS_TEMP_NAME);
  VariableLink(const VariableLink &link); ///< Copy constructor
  ~VariableLink();
  void replaceWith(Variable *newVar); ///< Replace the Variable pointed to
  void replaceWith(VariableLink *newVar); ///< Replace the Variable pointed to (just dereferences)
  int getIntName() const; ///< Get the name as an integer (for arrays)
  void setIntName(int n); ///< Set the name as an integer (for arrays)
};

/// Variable class (containing a doubly-linked list of children)
class Variable
{
public:
    Variable(); ///< Create undefined
    Variable(const std::string &varData, int varFlags); ///< User defined
    Variable(const std::string &str); ///< Create a string
    Variable(double varData);
    Variable(int val);
    Variable(const std::vector<unsigned char> &val); ///< Create a array-of-bytes
    ~Variable(void);

    Variable *getReturnVar(); ///< If this is a function, get the result value (for use by native functions)
    void setReturnVar(Variable *var); ///< Set the result value. Use this when setting complex return data as it avoids a deepCopy()
    Variable *getParameter(const std::string &name); ///< If this is a function, get the parameter with the given name (for use by native functions)

    VariableLink *findChild(const std::string &childName) const; ///< Tries to find a child with the given name, may return 0
    VariableLink *findChildOrCreate(const std::string &childName, int varFlags=VARIABLE_UNDEFINED); ///< Tries to find a child with the given name, or will create it with the given flags
    VariableLink *findChildOrCreateByPath(const std::string &path); ///< Tries to find a child with the given path (separated by dots)
    VariableLink *addChild(const std::string &childName, Variable *child=NULL);
    VariableLink *addChildNoDup(const std::string &childName, Variable *child=NULL); ///< add a child overwriting any with the same name
    void removeChild(Variable *child);
    void removeLink(VariableLink *link); ///< Remove a specific link (this is faster than finding via a child)
    void removeAllChildren();
    Variable *getArrayIndex(int idx) const; ///< The the value at an array index
    void setArrayIndex(int idx, Variable *value); ///< Set the value at an array index
    int getArrayLength() const; ///< If this is an array, return the number of items in it (else 0)
    int getChildren() const; ///< Get the number of children

    const std::vector<unsigned char> getArray() const;
    int getInt() const;
    bool getBool() const { return getInt() != 0; }
    double getDouble() const;
    const std::string getString() const;
    std::string getParsableString() const; ///< get Data as a parsable javascript string
    void setInt(int num);
    void setDouble(double val);
    void setString(const std::string &str);
    void setUndefined();
    void setArray();
    void setArray(const std::vector<unsigned char> &val);
    bool equals(const Variable *v);

    bool isInt() const { return (flags&VARIABLE_INTEGER)!=0; }
    bool isDouble() const { return (flags&VARIABLE_DOUBLE)!=0; }
    bool isString() const { return (flags&VARIABLE_STRING)!=0; }
    bool isNumeric() const { return (flags&VARIABLE_NUMERICMASK)!=0; }
    bool isFunction() const { return (flags&VARIABLE_FUNCTION)!=0; }
    bool isObject() const { return (flags&VARIABLE_OBJECT)!=0; }
    bool isArray() const { return (flags&VARIABLE_ARRAY)!=0; }
    bool isNative() const { return (flags&VARIABLE_NATIVE)!=0; }
    bool isUndefined() const { return (flags & VARIABLE_TYPEMASK) == VARIABLE_UNDEFINED; }
    bool isNull() const { return (flags & VARIABLE_NULL)!=0; }
    bool isBasic() const { return firstChild==0; } ///< Is this *not* an array/object/etc

    Variable *mathsOp(const Variable *b, int op); ///< do a maths op with another script variable
    void copyValue(const Variable *val); ///< copy the value from the value given
    Variable *deepCopy() const; ///< deep copy this node and return the result

    void trace(const std::string &indentStr = "", const std::string &name = "") const; ///< Dump out the contents of this using trace
    std::string getFlagsAsString() const; ///< For debugging - just dump a string version of the flags
    void getJSON(std::ostringstream &destination, const std::string &linePrefix="") const; ///< Write out all the JS code needed to recreate this script variable to the stream (as JSON)
    void setCallback(JSCallback callback, void *userdata); ///< Set the callback for native functions

    VariableLink *firstChild;
    VariableLink *lastChild;

    /// For memory management/garbage collection
    Variable *ref(); ///< Add reference to this variable
    void unref(); ///< Remove a reference, and delete this variable if required
    int getRefs() const; ///< Get the number of references to this script variable
protected:
    int refs; ///< The number of references held to this - used for garbage collection

    std::string stringData; ///< The contents of this variable if it is a string
    long intData; ///< The contents of this variable if it is an int
    double doubleData; ///< The contents of this variable if it is a double
    int flags; ///< the flags determine the type of the variable - int/double/string/etc
    JSCallback jsCallback; ///< Callback for native functions
    void *jsCallbackUserData; ///< user data passed as second argument to native functions

    void init(); ///< initialisation of data members

    /** Copy the basic data and flags from the variable given, with no
      * children. Should be used internally only - by copyValue and deepCopy */
    void copySimpleData(const Variable *val);

    friend class Interpreter;
};

class Interpreter {
public:
    Interpreter();
    ~Interpreter();

    void execute(const std::string &code);
    /** Evaluate the given code and return a link to a javascript object,
     * useful for (dangerous) JSON parsing. If nothing to return, will return
     * 'undefined' variable type. VariableLink is returned as this will
     * automatically unref the result as it goes out of scope. If you want to
     * keep it, you must use ref() and unref() */
    VariableLink evaluateComplex(const std::string &code);
    /** Evaluate the given code and return a string. If nothing to return, will return
     * 'undefined' */
    std::string evaluate(const std::string &code);

    /// add a native function to be called from TinyJS
    /** example:
       \code
           void scRandInt(Variable *c, void *userdata) { ... }
           tinyJS->addNative("function randInt(min, max)", scRandInt, 0);
       \endcode

       or

       \code
           void scSubstring(Variable *c, void *userdata) { ... }
           tinyJS->addNative("function String.substring(lo, hi)", scSubstring, 0);
       \endcode
    */
    void addNative(const std::string &funcDesc, JSCallback ptr, void *userdata);

    /// get the given variable specified by a path (var1.var2.etc), or return 0
    Variable *getScriptVariable(const std::string &path) const;
    /// get the value of the given variable, return true if it exists and fetched
    bool getVariable(const std::string &path, std::string &varData) const;
    /// set the value of the given variable, return true if it exists and gets set
    bool setVariable(const std::string &path, const std::string &varData);

    /// Send all variables to stdout
    void trace();

    Variable *root;   /// root of symbol table
private:
    Lexer *l;             /// current lexer
    std::vector<Variable*> scopes; /// stack of scopes when parsing
#ifdef TINYJS_CALL_STACK
    std::vector<std::string> call_stack; /// Names of places called so we can show when erroring
#endif

    Variable *stringClass; /// Built in string class
    Variable *objectClass; /// Built in object class
    Variable *arrayClass; /// Built in array class

    // parsing - in order of precedence
    VariableLink *functionCall(bool &execute, VariableLink *function, Variable *parent);
    VariableLink *factor(bool &execute);
    VariableLink *unary(bool &execute);
    VariableLink *term(bool &execute);
    VariableLink *expression(bool &execute);
    VariableLink *shift(bool &execute);
    VariableLink *condition(bool &execute);
    VariableLink *logic(bool &execute);
    VariableLink *ternary(bool &execute);
    VariableLink *base(bool &execute);
    void block(bool &execute);
    void statement(bool &execute);
    // parsing utility functions
    VariableLink *parseFunctionDefinition();
    void parseFunctionArguments(Variable *funcVar);

    VariableLink *findInScopes(const std::string &childName) const; ///< Finds a child, looking recursively up the scopes
    /// Look up in any parent classes of the given object
    VariableLink *findInParentClasses(Variable *object, const std::string &name) const;
};

}; // namespace TinyJS

#endif
