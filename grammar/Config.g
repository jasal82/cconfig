/**
 * Copyright (c) 2010-2012, Johannes Asal
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES,  INCLUDING,  BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS  FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
 * CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT  LIMITED  TO,  PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS; OR BUSINESS
 * INTERRUPTION)  HOWEVER  CAUSED AND ON ANY THEORY OF LIABILITY,  WHETHER  IN
 * CONTRACT,  STRICT  LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

grammar Config;

options {
    language=Cpp;
}

@parser::includes {
#include "ConfigLexer.hpp"
#include "config_tree.hpp"
}

@lexer::traits {
class ConfigLexer; class ConfigParser;
typedef antlr3::Traits<ConfigLexer, ConfigParser> ConfigLexerTraits;
typedef ConfigLexerTraits ConfigParserTraits;
}

BOOLEAN
    :   'true' | 'false'
    ;

ID  :   ('a'..'z'|'A'..'Z'|'_') ('a'..'z'|'A'..'Z'|'0'..'9'|'_')*
    ;

INT :   ('+'|'-')? '0'..'9'+
    ;

FLOAT
    :   ('+'|'-')? ('0'..'9')+ '.' ('0'..'9')* EXPONENT?
    |   ('+'|'-')? '.' ('0'..'9')+ EXPONENT?
    |   ('+'|'-')? ('0'..'9')+ EXPONENT
    ;

COMMENT
    :   '//' ~('\n'|'\r')* '\r'? '\n' {$channel=HIDDEN;}
    |   '/*' ( options {greedy=false;} : . )* '*/' {$channel=HIDDEN;}
    ;

WS  :   ( ' '
        | '\t'
        | '\r'
        | '\n'
        ) {$channel=HIDDEN;}
    ;

STRING
    :  '"' ( ESC_SEQ | ~('\\'|'"') )* '"'   { setText(getText().substr(1, getText().length() - 2)); }
    ;

fragment
EXPONENT : ('e'|'E') ('+'|'-')? ('0'..'9')+ ;

fragment
HEX_DIGIT : ('0'..'9'|'a'..'f'|'A'..'F') ;

fragment
ESC_SEQ
    :   '\\' ('b'|'t'|'n'|'f'|'r'|'\"'|'\''|'\\')
    |   UNICODE_ESC
    |   OCTAL_ESC
    ;

fragment
OCTAL_ESC
    :   '\\' ('0'..'3') ('0'..'7') ('0'..'7')
    |   '\\' ('0'..'7') ('0'..'7')
    |   '\\' ('0'..'7')
    ;

fragment
UNICODE_ESC
    :   '\\' 'u' HEX_DIGIT HEX_DIGIT HEX_DIGIT HEX_DIGIT
    ;

file returns [cconfig::group* root]
@init { $root = new cconfig::group(); }
    :   definition[$root]* EOF
    ;

definition[cconfig::group* g]
    :   groupDefinition[$g]
    |   variableDefinition[$g]
    ;

groupDefinition[cconfig::group* g]
    :   i=ID group
        { $g->insert($i.text, $group.value); }
    ;

variableDefinition[cconfig::group* g]
    :   i=ID '='
        (   list    { $g->insert($i.text, $list.value); }
        |   array   { $g->insert($i.text, $array.value); }
        |   atom    { $g->insert($i.text, $atom.value); }
        ) ';'
    ;

group returns [cconfig::group* value]
@init { $value = new cconfig::group(); }
    :   '{'
        definition[$value]*
        '}'
    ;

list returns [cconfig::list* value]
@init { $value = new cconfig::list(); }
    :   '('
        listBody[$value]?
        ')'
    ;

listBody[cconfig::list* l]
    :   a=listElement { $l->append($a.value); }
        (',' b=listElement { $l->append($b.value); } )*
    ;

listElement returns [cconfig::element* value]
    :   group   { $value = $group.value; }
    |   list    { $value = $list.value; }   
    |   array   { $value = $array.value; }
    |   atom    { $value = $atom.value; }
    ;

array returns [cconfig::list* value]
@init { $value = new cconfig::list(); }
    :   '['
        (   floatArrayBody[$value]
        |   intArrayBody[$value]
        |   boolArrayBody[$value]
        |   stringArrayBody[$value]
        )?
        ']'
    ;
    
floatArrayBody[cconfig::list* l]
    :   a=float_ { $l->append($a.value); }
        (',' b=float_ { $l->append($b.value); } )*
    ;

intArrayBody[cconfig::list* l]
    :   a=int_ { $l->append($a.value); }
        (',' b=int_ { $l->append($b.value); } )*
    ;
    
boolArrayBody[cconfig::list* l]
    :   a=bool_ { $l->append($a.value); }
        (',' b=bool_ { $l->append($b.value); } )*
    ;

stringArrayBody[cconfig::list* l]
    :   a=string_ { $l->append($a.value); }
        (',' b=string_ { $l->append($b.value); } )*
    ;

atom returns [cconfig::element* value]
    :   float_  { $value = $float_.value; }
    |   int_    { $value = $int_.value; }
    |   bool_   { $value = $bool_.value; }
    |   string_ { $value = $string_.value; }
    ;

float_ returns [cconfig::atom* value]
    :   FLOAT
        { $value = new cconfig::atom(boost::lexical_cast<double>($FLOAT.text)); }
    ;

int_ returns [cconfig::atom* value]
    :   INT
        { $value = new cconfig::atom(boost::lexical_cast<long>($INT.text)); }
    ;

bool_ returns [cconfig::atom* value]
    :   BOOLEAN
        { $value = new cconfig::atom(boost::lexical_cast<bool>($BOOLEAN.text)); }
    ;

string_ returns [cconfig::atom* value]
    :   STRING
        { $value = new cconfig::atom($STRING.text); }
    ;
