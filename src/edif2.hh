/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2013  Dr. Jürgen Sauermann

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EDIF_HH
#define EDIF_HH

/***
    The following was nicked from native/template.hh because I needed
    a custom close_fun().
***/

//#include "Value.icc"
#include "Native_interface.hh"

class NativeFunction;

extern "C" void * get_function_mux(const char * function_name);
static Token eval_ident_Bx(Value_P B, Axis x, const NativeFunction * caller);
static Token eval_fill_B(Value_P B, const NativeFunction * caller);
static Token eval_fill_AB(Value_P A, Value_P B, const NativeFunction * caller);

Token
eval_fill_B(Value_P B, const NativeFunction * caller)
{
UCS_string ucs("eval_fill_B() called");
Value_P Z(ucs, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}

Token
eval_fill_AB(Value_P A, Value_P B, const NativeFunction * caller)
{
UCS_string ucs("eval_fill_B() called");
Value_P Z(ucs, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}

Token
eval_ident_Bx(Value_P B, Axis x, const NativeFunction * caller)
{
UCS_string ucs("eval_ident_Bx() called");
Value_P Z(ucs, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}

#endif  // EDIF_HH
