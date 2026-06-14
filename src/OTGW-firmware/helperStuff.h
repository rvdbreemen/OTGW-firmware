/*
***************************************************************************
**  Program  : helperStuff.h
**  Version  : v2.0.0-alpha.189
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Template utilities that must be visible to all translation units.
**  Included by OTGW-firmware.h — never include this file directly.
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#ifndef HELPERSTUFF_H
#define HELPERSTUFF_H

// PROGMEM_readAnything — copy a PROGMEM struct/value into RAM.
// Implemented as a macro (not a function/template) so ctags does not generate
// a broken forward declaration for it (ctags can't express template parameters).
// sizeof(dest) is safe because dest is always a local struct variable.
#ifndef PROGMEM_readAnything
#define PROGMEM_readAnything(src, dest) memcpy_P(&(dest), (src), sizeof(dest))
#endif

#endif // HELPERSTUFF_H

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
****************************************************************************
*/
