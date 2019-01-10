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

grammar ConfigSchema;

options {
    language=Cpp;
}

@parser::includes {
#include "ConfigSchemaLexer.hpp"
#include "config_schema.hpp"
}

@lexer::traits {
class ConfigSchemaLexer; class ConfigSchemaParser;
typedef antlr3::Traits<ConfigSchemaLexer, ConfigSchemaParser> ConfigSchemaLexerTraits;
typedef ConfigSchemaLexerTraits ConfigSchemaParserTraits;
}

@members {
std::string getUri() {

}
}

BOOLEAN
    :   'true'
    |   'false'
    ;

REQUIRED
    :   'required'
    ;

OPTIONAL
    :   'optional'
    ;

GROUP_LABEL
    :   'group'
    ;

LIST_LABEL
    :   'list'
    ;

ARRAY_LABEL
    :   'array'
    ;

INTEGER_LABEL
    :   'int'
    ;

STRING_LABEL
    :   'string'
    ;

BOOL_LABEL
    :   'bool'
    ;

FLOAT_LABEL
    :   'float'
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
    :   '//' ~('\n'|'\r')* '\r'? '\n' { $channel=HIDDEN; }
    |   '/*' ( options { greedy=false; } : . )* '*/' { $channel=HIDDEN; }
    ;

WS  :   ( ' '
        | '\t'
        | '\r'
        | '\n'
        ) {$channel=HIDDEN;}
    ;

STRING
    :   '"' ( ESC_SEQ | ~('\\'|'"') )* '"'
        { setText(getText().substr(1, getText().length() - 2)); }
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

file returns [cconfig::schema::group* value]
    :   { $value = new cconfig::schema::group(); }
        variableDefinition[$value]*
    ;

variableDefinition[cconfig::schema::group* g]
scope {
    bool r;
}
@init {
    $variableDefinition::r = false;
}
    :   ID
        (status { $variableDefinition::r = $status.r; })?
        type
        ';'
        {
            $g->add_child(
                $ID.text,
                $type.value,
                $variableDefinition::r
            );
        }
    ;

status returns [bool r]
    :   (   REQUIRED { $r = true; }
        |   OPTIONAL { $r = false; }
        )
    ;

type returns [cconfig::schema::node* value]
    :   primitiveType   { $value = $primitiveType.value; }
    |   arrayType       { $value = $arrayType.value; }
    |   groupType       { $value = $groupType.value; }
    |   listType        { $value = $listType.value; }
    ;

primitiveType returns [cconfig::schema::atom* value]
    :
    '('
    (   INTEGER_LABEL   { $value = new cconfig::schema::atom(typeid(long)); }
    |   STRING_LABEL    { $value = new cconfig::schema::atom(typeid(std::string)); }
    |   BOOL_LABEL      { $value = new cconfig::schema::atom(typeid(bool)); }
    |   FLOAT_LABEL     { $value = new cconfig::schema::atom(typeid(double)); }
    )
    ')'
    attributeList[$value]
    ;

arrayType returns [cconfig::schema::list* value]
    :   { $value = new cconfig::schema::list(); }
        attributeList[$value]
        '(' ARRAY_LABEL ')'
        '{' primitiveType '}'
        { $value->add_child($primitiveType.value); }
    ;

groupType returns [cconfig::schema::group* value]
    :   { $value = new cconfig::schema::group(); }
        attributeList[$value]
        '(' GROUP_LABEL ')'
        '{' (variableDefinition[$value])+ '}'
    ;

listType returns [cconfig::schema::list* value]
    :   { $value = new cconfig::schema::list(); }
        attributeList[$value]
        '(' LIST_LABEL ')'
        '{' type '}'
        { $value->add_child($type.value); }
    ;

attributeList[cconfig::schema::node* n]
    :   attribute[$n]*
    ;

attribute[cconfig::schema::node* n]
    :   ID
        '='
        (   double_     { $n->add_attribute($ID.text, $double_.value); }
        |   long_       { $n->add_attribute($ID.text, $long_.value); }
        |   bool_       { $n->add_attribute($ID.text, $bool_.value); }
        |   string_     { $n->add_attribute($ID.text, $string_.value); }
        )
    ;
    
double_ returns [double value]
    :   FLOAT
        { $value = boost::lexical_cast<double>($FLOAT.text); }
    ;

long_ returns [long value]
    :   INT
        { $value = boost::lexical_cast<long>($INT.text); }
    ;

bool_ returns [bool value]
    :   BOOLEAN
        { $value = boost::lexical_cast<bool>($BOOLEAN.text); }
    ;

string_ returns [std::string value]
    :   STRING
        { $value = $STRING.text; }
    ;
