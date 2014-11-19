tiny-js
=======

A simple single-file javascript interpreter written in C++.

Foreword
========

The original project has been developed by [Gordon Williams](www.pur3.co.uk). All credits of the initial breed are due to him.

Description
===========

This project aims to be an extremely simple (~2000 line) JavaScript interpreter, meant for inclusion in applications that require a simple, familiar script language that can be included with no dependencies other than normal C++ libraries. It currently consists of two source files - one containing the interpreter, another containing built-in functions such as String.substring.

TinyJS is not designed to be fast or full-featured. However it is great for scripting simple behaviour, or loading & saving settings.

I make absolutely no guarantees that this is compliant to JavaScript/EcmaScript standard. In fact I am sure it isn't. However I welcome suggestions for changes that will bring it closer to compliance without overly complicating the code, or useful test cases to add to the test suite.

Currently TinyJS supports:

* Variables, Arrays, Structures
* JSON parsing and output
* Functions
* Calling C/C++ code from JavaScript
* Objects with Inheritance (not fully implemented)

Please see the **Code Examples** chapter for examples of code that works.

For a list of known issues, please see the comments at the top of the `TinyJS.cpp` file.

To download TinyJS, just get SVN and run the following command:

```
svn checkout http://tiny-js.googlecode.com/svn/trunk/ tiny-js-read-only
```

There is also the `42tiny-js` branch. This is maintained by Armin and provides a more fully-featured JavaScript implementation.

```
svn checkout http://tiny-js.googlecode.com/svn/branches/42tiny-js 42tiny-js
```

TinyJS is released under an MIT licence.

Internal Structure
==================

TinyJS uses a Recursive Descent Parser, so there is no *parser generator* required. It does not compile to an intermediate code, and instead executes directly from source code. This makes it quite fast for code that is executed infrequently, and slow for loops.

Variables, arrays and objects are stored in a simple linked list tree structure (`42tiny-js` uses a `std::map`). This is simple, but relatively slow for large structures or arrays.

JavaScript for Microcontrollers
===============================

If you're after JavaScript for microcontrollers, take a look at the *Espruino JavaScript Interpreter*. It is a complete re-write of TinyJS targeted at processors with extremely low RAM (8KiB or more). It is currently available for a range of STM32 ARM Microcontrollers.

We've just launched a [KickStarter](http://www.kickstarter.com/projects/48651611/espruino-javascript-for-things) for a board with it pre-installed. If it completes successfully we'll be releasing all hardware and software as Open Source!

Code Examples
=============

The Variable `result` evaluates to `true` for each of these examples.

```javascript
// simple for loop
var a = 0;
for (var i=1;i<10;i++) a = a + i;
result = a==45;
```

```javascript
// simple function
function add(x,y) { return x+y; }
result = add(3,6)==9;
```

```javascript
// functions in variables using JSON-style initialisation
var bob = { add : function(x,y) { return x+y; } };

result = bob.add(3,6)==9;
```

```javascript
a = 345;    // an "integer", although there is only one numeric type in JavaScript
b = 34.5;   // a floating-point number
c = 3.45e2; // another floating-point, equivalent to 345
d = 0377;   // an octal integer equal to 255
e = 0xFF;   // a hexadecimal integer equal to 255, digits represented by the letters A-F may be upper or lowercase

result = a==345 && b*10==345 && c==345 && d==255 && e==255;
```

```javascript
// references for arrays
var a;
a[0] = 10;
a[1] = 22;
b = a;
b[0] = 5;
result = a[0]==5 && a[1]==22 && b[1]==22;
```

```javascript
// references with functions
var a = 42;
var b;
b[0] = 43;

function foo(myarray) {
  myarray[0]++;
}

function bar(myvalue) {
  myvalue++;
}

foo(b);
bar(a);

result = a==42 && b[0]==44;
```

```javascript
// built-in functions

foo = "foo bar stuff";
r = Math.rand();

parsed = Integer.parseInt("42");

aStr = "ABCD";
aChar = aStr.charAt(0);

obj1 = new Object();
obj1.food = "cake";
obj1.desert = "pie";

obj2 = obj1.clone();
obj2.food = "kittens";

result = foo.length()==13 && foo.indexOf("bar")==4 && foo.substring(8,13)=="stuff" && parsed==42 && 
         Integer.valueOf(aChar)==65 && obj1.food=="cake" && obj2.desert=="pie";
```
