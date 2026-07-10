/*
***************************************************************************
**  Program  : helperStuff.h
**  Version  : v2.0.0-alpha.344
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Template utilities that must be visible to all translation units.
**  Included by OTGW-firmware.h — never include this file directly.
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.
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
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
*
****************************************************************************
*/
