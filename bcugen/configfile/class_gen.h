/*
    BCU SDK bcu development enviroment
    Copyright (C) 2005 Martin K�gler <mkoegler@auto.tuwien.ac.at>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#undef OBJECT
#define OBJECT(A) class A { public: A(); int lineno;
#undef END_OBJECT
#define END_OBJECT };
#undef ATTRIB_STRING
#define ATTRIB_STRING(A) String A;int A##_lineno;
#undef ATTRIB_IDENT
#define ATTRIB_IDENT(A) String A;int A##_lineno;
#undef ATTRIB_FLOAT
#define ATTRIB_FLOAT(A) ftype A;int A##_lineno;
#undef ATTRIB_INT
#define ATTRIB_INT(A) itype A;int A##_lineno;
#undef ATTRIB_BOOL
#define ATTRIB_BOOL(A) bool A;int A##_lineno;
#undef ATTRIB_ENUM_MAP
#define ATTRIB_ENUM_MAP(A) IdentMap A;int A##_lineno;
#undef ATTRIB_ARRAY_OBJECT
#define ATTRIB_ARRAY_OBJECT(A) Array<A> A##s;
#undef ATTRIB_INT_MAP
#define ATTRIB_INT_MAP(A,B) itype A;int A##_lineno;
#undef ATTRIB_FLOAT_MAP
#define ATTRIB_FLOAT_MAP(A,B) ftype A;int A##_lineno;
#undef ATTRIB_ENUM
#define ATTRIB_ENUM(A,B,C) B A;int A##_lineno;
#undef ATTRIB_IDENT_ARRAY
#define ATTRIB_IDENT_ARRAY(A) IdentArray A;int A##_lineno;
#undef ATTRIB_STRING_ARRAY
#define ATTRIB_STRING_ARRAY(A) StringArray A;int A##_lineno;

#undef PRIVATE_VAR
#define PRIVATE_VAR(A) A;