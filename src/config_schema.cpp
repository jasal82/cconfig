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

#include "config_schema.hpp"
#include "config_file.hpp"

#include "ConfigSchemaLexer.hpp"
#include "ConfigSchemaParser.hpp"

#include <fstream>

#include <boost/foreach.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/join.hpp>

std::string
cconfig::schema::node::uri() const
{
	if(parent_ == NULL)
		return "/";

	std::deque<std::string> uri_elements;
	const node* n = this;
	while(n != NULL)
	{
		std::string element;
		if(n->parent_ == NULL)
			element = "";
		else if(n->name_.empty())
			element = "unnamed";
		else
			element = n->name_;

		const list* l = dynamic_cast<const list*>(n);
		if(l)
		{
			element += "[]";
		}

		uri_elements.push_front(element);
		n = n->parent_;
	}
	
	return boost::algorithm::join(uri_elements, "/");
}

std::string
cconfig::schema::node::uri_safe() const
{
	std::string u = boost::algorithm::replace_all_copy(uri(), "/", "_");
	boost::algorithm::erase_all(u, "[]");
	return u;
}

bool
cconfig::schema::node::has_attribute(const std::string& name) const
{
	attribute_map_type::const_iterator it = attributes_.find(name);
	return it != attributes_.end();
}

std::string
cconfig::schema::node::generate_common_tree_initialization(int unique_id, int indent) const
{
	// visitors are your friend
	attribute_visitor visitor;

	std::string s;
	std::string varname = "var" + boost::lexical_cast<std::string>(unique_id);

	for(attribute_map_type::const_iterator ita = attributes_.begin();
		ita != attributes_.end(); ++ita)
	{
		indent_string(s, indent); s += varname + "->add_attribute(\"" + ita->first + "\", "
			+ boost::apply_visitor(visitor, ita->second) + ");\n";
	}

	return s;
}

cconfig::schema::group::~group()
{
	node_map_type::iterator it = children_.begin();
	for(; it != children_.end(); ++it)
		delete it->second;
}

void
cconfig::schema::group::add_child(
		const std::string& name,
		node* n,
		bool required)
{
	// insert node into map and set name and parent in
	// the node itself
	children_.insert(std::make_pair(name, n));
	n->name_ = name;
	n->parent_ = this;

	// set required flag in the node and update flag
	// on the parent group; this is a bit tricky...
	// one may ask why the required flag is not set
	// upon construction of the specific node, but
	// this is not possible because of the bottom-up
	// way of building the tree
	if(required || n->required_)
	{
		// default is false (optional) but there may
		// be a required flag attached to the current
		// definition ('required' parameter) or inherited
		// from the node's child nodes ('required_' member
		// of the node object); in both cases we need
		// to update the 'this' node's required_ flag
		// (inheritance)
		n->required_ = true;
		this->required_ = true;
	}
}

cconfig::schema::validation_result
cconfig::schema::group::validate(const cconfig::element& e, bool strict) const
{
	try {
		// loop child nodes in schema and validate the config
		// nodes against them
		const cconfig::group& g = e.as_group();
		node_map_type::const_iterator it = children_.begin();
		for(; it != children_.end(); ++it)
		{
			try {
				const cconfig::element& c = e[it->first];
				// validate child node
				validation_result r = it->second->validate(c, strict);
				// mark as invalid if result was invalid
				if(!r.valid)
					return r;
			} catch(cconfig::lookup_error&) {
				// mark as invalid if config setting was not found though
				// it is required
				if(it->second->required_)
					return validation_result(false, this->uri(),
						"Missing required attribute '" + it->first + "'");
			}
		}

		// check the other way round (all config settings must be defined
		// in schema) if strict flag is set
		if(strict)
		{
			cconfig::group::iterator git = g.begin();
			for(; git != g.end(); ++git)
			{
				node_map_type::const_iterator it = children_.find(git->first);
				if(it == children_.end())
					return validation_result(false, this->uri(),
						"Attribute '" + git->first + "' not found in schema "
						+ "(strict validation). This might possibly be a typo.");
			}
		}
	} catch(cconfig::lookup_error&) {
		return validation_result(false, this->uri(), "Group required");
	}

	return true;
}

std::string
cconfig::schema::group::generate_declaration() const
{
	if(children_.empty())
		return "";

	std::string code;

	// generate declarations for children first
	node_map_type::const_iterator it = children_.begin();
	for(; it != children_.end(); ++it)
	{
		code += it->second->generate_declaration();
	}

	// then generate own declaration
	if(parent_ == NULL)	// root node
		code += "struct Config {\n";
	else
	{
		code += "struct group" + uri_safe() + " {\n";
	}

	for(it = children_.begin(); it != children_.end(); ++it)
	{
		code += "\t" + it->second->generate_definition();
	}

	// generate initializations
	std::vector<std::string> initializations;
	for(it = children_.begin(); it != children_.end(); ++it)
	{
		if(cconfig::schema::atom* a = dynamic_cast<cconfig::schema::atom*>(it->second))
		{
			std::string s(it->first + "(");
			
			if(a->has_attribute("default"))
			{
				// we have a default value, so put it into initializer
				if(a->type_ == typeid(long))
					s += boost::lexical_cast<std::string>(a->get_attribute<long>("default")) + "L";
				else if(a->type_ == typeid(bool))
					s += (a->get_attribute<bool>("default")?"true":"false");
				else if(a->type_ == typeid(double))
					s += boost::lexical_cast<std::string>(a->get_attribute<double>("default"));
				else if(a->type_ == typeid(std::string))
					s += "\"" + a->get_attribute<std::string>("default") + "\"";
			}
			else
			{
				// initialize with a sensible default to avoid accidental usage
				// of undefined variables
				if(a->type_ == typeid(long))
					s += "0L";
				else if(a->type_ == typeid(bool))
					s += "false";
				else if(a->type_ == typeid(double))
					s += "0.0";
			}
			
			s += ")";

			initializations.push_back(s);
		}
	}

	if(!initializations.empty())
	{
		if(parent_ == NULL) // root node
			code += "\n\tConfig() :\n\t\t";
		else
		{
			code += "\n\tgroup" + uri_safe() + "() :\n\t\t";
		}

		code += boost::algorithm::join(initializations, ",\n\t\t");
		code += "\n\t{}\n";
	}

	if(parent_ == NULL) // root node
	{
		code += "\n\tcconfig::file& file() { return *file_; }\n";
		code += "\tcconfig::file* file_;\n";
	}

	code += "};\n\n";
	return code;
}

std::string
cconfig::schema::group::generate_definition() const
{
	return "group" + uri_safe() + " " + name_ + ";\n";
}

std::string
cconfig::schema::group::generate_initialization() const
{
	return "generate_group" + uri_safe() + "(child_element, child_node)";
}

std::string
cconfig::schema::group::generate_function() const
{
	std::string code;

	std::string return_type;
	std::string function_name;

	if(parent_ == NULL) // root node
	{
		return_type = "Config";
		function_name = "generate_Config";
	}
	else
	{
		std::string u = uri_safe();
		return_type = "group" + u;
		function_name = "generate_group" + u;
	}

	node_map_type::const_iterator it = children_.begin();
	for(; it != children_.end(); ++it)
		code += it->second->generate_function();

	code += return_type + " " + function_name +
		"(const cconfig::element& e, cconfig::schema::node* n)\n";
	code += "{\n";
	code += "\t" + return_type + " r;\n";
	code += "\tcconfig::schema::group* g = dynamic_cast<cconfig::schema::group*>(n);\n";
	for(it = children_.begin(); it != children_.end(); ++it)
	{
		code += "\t{\n";
		code += "\t\tcconfig::schema::node* child_node = g->children_.find(\""
			+ it->first + "\")->second;\n";
		code += "\t\t{\n";
		if(!it->second->required_)
		{
			code += "\t\t\ttry {\n";
			code += "\t\t\t\tconst cconfig::element& child_element = e[\"" + it->first + "\"];\n";
			code += "\t\t\t\tr." + it->first + " = ";
			code += it->second->generate_initialization() + ";\n";
			code += "\t\t\t} catch(cconfig::lookup_error&) {}\n";
			// this is an optional setting and possible defaults are already
			// defined in the struct declaration so we may (and should)
			// safely ignore this error
		}
		else
		{
			code += "\t\t\tconst cconfig::element& child_element = e[\"" + it->first + "\"];\n";
			code += "\t\t\tr." + it->first + " = ";
			code += it->second->generate_initialization() + ";\n";
		}
		code += "\t\t}\n";
		code += "\t}\n";
	}
	code += "\n\treturn r;\n";
	code += "}\n\n";

	return code;
}

std::string
cconfig::schema::group::generate_tree_builder(int& unique_id, int indent) const
{
	std::string s;
	std::string varname = "var" + boost::lexical_cast<std::string>(unique_id);

	indent_string(s, indent); s += "cconfig::schema::group* " + varname
		+ " = new cconfig::schema::group();\n";
	s += generate_common_tree_initialization(unique_id, indent);
	
	for(node_map_type::const_iterator it = children_.begin();
		it != children_.end(); ++it)
	{
		indent_string(s, indent); s += "{\n";
		// we need a unique variable name here so we construct one using
		// the incrementing variable unique_id
		std::string childvarname = "var" + boost::lexical_cast<std::string>(++unique_id);
		s += it->second->generate_tree_builder(unique_id, indent+1);

		indent_string(s, indent+1); s += varname + "->add_child(\"" + it->first + "\", "
			+ childvarname + ", " + (it->second->required_?"true":"false") + ");\n";
		
		indent_string(s, indent); s += "}\n";
	}

	return s;
}

std::string
cconfig::schema::group::generate_config_stub(int indent) const
{
	std::string s;
	
	s += "{\n";

	for(node_map_type::const_iterator it = children_.begin();
		it != children_.end(); ++it)
	{
		indent_string(s, indent+1);
		s += it->first
			+ " = "
			+ it->second->generate_config_stub(indent+1)
			+ ";\n";
	}

	indent_string(s, indent); s += "}";
	return s;
}

cconfig::schema::list::~list()
{
	node_list_type::iterator it = children_.begin();
	for(; it != children_.end(); ++it)
		delete *it;
}

void
cconfig::schema::list::add_child(node* n)
{
	children_.push_back(n);
	n->parent_ = this;
}

cconfig::schema::validation_result
cconfig::schema::list::validate(
		const cconfig::element& e,
		bool strict) const
{
	try {
		const cconfig::list& l = e.as_list();
		node_list_type::const_iterator it = children_.begin();
		// only one child allowed! TODO: allow more childs after thinking
		// about how to handle this properly (this might be brainfuck though)

		cconfig::list::iterator lit = l.begin();
		for(; lit != l.end(); ++lit)
		{
			validation_result r = (*it)->validate(*lit, strict);
			if(!r.valid)
				return r;
		}

		// check min attribute
		if(has_attribute("min"))
		{
			unsigned int min = get_attribute<int>("min");
			if(l.size() < min)
				return validation_result(false, this->uri(),
					"List has not enough entries, need at least " +
					boost::lexical_cast<std::string>(min)
				);
		}

		return validation_result(true);
	} catch(cconfig::lookup_error&) {
		return validation_result(false, this->uri(), "List required");
	}
}

std::string
cconfig::schema::list::generate_declaration() const
{
	if(children_.empty())
		return "";

	std::string code;

	// generate declarations for child first
	node_list_type::const_iterator it = children_.begin();
	code += (*it)->generate_declaration();

	// then generate own declaration
	code += "typedef std::vector<";
	if(dynamic_cast<cconfig::schema::group*>(*it))
		code += "group" + (*it)->uri_safe();
	else if(dynamic_cast<cconfig::schema::group*>(*it))
		code += "list" + (*it)->uri_safe();
	else if(cconfig::schema::atom* a = dynamic_cast<cconfig::schema::atom*>(*it))
		code += a->c_type_string();
	
	code += "> list" + uri_safe() + ";\n";
	return code;
}

std::string
cconfig::schema::list::generate_definition() const
{
	return "list" + uri_safe() + " " + name_ + ";\n";
}

std::string
cconfig::schema::list::generate_initialization() const
{
	return "generate_list" + uri_safe() + "(child_element, child_node)";
}

std::string
cconfig::schema::list::generate_function() const
{
	std::string code;

	std::string u = uri_safe();
	std::string return_type = "list" + u;

	cconfig::schema::node* spec = children_.front();
	code += spec->generate_function();

	code += return_type + " generate_list" + u +
			"(const cconfig::element& e, cconfig::schema::node* n)\n";
	code += "{\n";
	code += "\t" + return_type + " r;\n";
	code += "\tcconfig::schema::list* ln = dynamic_cast<cconfig::schema::list*>(n);\n";
	code += "\tcconfig::schema::node* child_node = *(ln->children_.begin());\n";
	code += "\tconst cconfig::list& l = e.as_list();\n";
	code += "\tcconfig::list::iterator it = l.begin();\n";
	code += "\tfor(; it != l.end(); ++it)\n";
	code += "\t{\n";
	code += "\t\tconst cconfig::element& child_element = *it;\n";
	code += "\t\tr.push_back(" + spec->generate_initialization() + ");\n";
	code += "\t}\n";
	code += "\n\treturn r;\n";
	code += "}\n\n";

	return code;
}

std::string
cconfig::schema::list::generate_tree_builder(int& unique_id, int indent) const
{
	std::string s;
	std::string varname = "var" + boost::lexical_cast<std::string>(unique_id);

	indent_string(s, indent); s += "cconfig::schema::list* " + varname
		+ " = new cconfig::schema::list();\n";
	s += generate_common_tree_initialization(unique_id, indent);
	
	for(node_list_type::const_iterator it = children_.begin();
		it != children_.end(); ++it)
	{
		std::string childvarname = "var" + boost::lexical_cast<std::string>(++unique_id);
		s += (*it)->generate_tree_builder(unique_id, indent);

		indent_string(s, indent); s += varname + "->add_child(" + childvarname + ");\n";
	}

	return s;
}

std::string
cconfig::schema::list::generate_config_stub(int indent) const
{
	std::string s;

	// we defined that there may be only one child in the schema
	node_list_type::const_iterator it = children_.begin();

	// this may be an array or a list, so we need to make a sensible
	// guess based on the constraints
	if(dynamic_cast<cconfig::schema::atom*>(*it))
	{
		// this should be an array so we generate a dummy parameter
		s += "[" + (*it)->generate_config_stub(indent+1) + "]";
	}
	else
	{
		// this must be a list and, as the child must be a group
		// or list, we should generate a (single) stub for that as well
		s += "(\n";
		indent_string(s, indent+1);
		(*it)->generate_config_stub(indent+1);
		indent_string(s, indent); s += ")";
	}

	return s;
}

cconfig::schema::validation_result
cconfig::schema::atom::validate(
		const cconfig::element& e,
		bool strict) const
{
	try {
		const cconfig::atom& a = e.as_atom();
		if(a.type() != type_)
		{
			std::string type_name;
			// TODO: would be nice to have some generalized way of doing
			// this stuff (visitor...?)
			if(type_ == typeid(std::string))
				type_name = "string";
			else if(type_ == typeid(long))
				type_name = "integer";
			else if(type_ == typeid(bool))
				type_name = "bool";
			else if(type_ == typeid(double))
				type_name = "float";

			return validation_result(false, this->uri(),
				"Type mismatch, " + type_name + " required");
		}
	} catch(cconfig::lookup_error&) {
		return validation_result(false, this->uri(), "Atom required");
	}

	return validation_result(true);
}

std::string
cconfig::schema::atom::c_type_string() const
{
	// TODO: use static_visitor
	if(type_ == typeid(std::string))
		return "std::string";
	else if(type_ == typeid(long))
		return "long";
	else if(type_ == typeid(bool))
		return "bool";
	else if(type_ == typeid(double))
		return "double";
	else
		throw std::runtime_error("Unknown error in config schema");
}

std::string
cconfig::schema::atom::generate_declaration() const
{
	return "";
}

std::string
cconfig::schema::atom::generate_definition() const
{
	// TODO: duh..., use static_visitor
	std::string code;
	if(type_ == typeid(std::string))
		code += "std::string";
	else if(type_ == typeid(long))
		code += "long";
	else if(type_ == typeid(bool))
		code += "bool";
	else if(type_ == typeid(double))
		code += "double";

	code += " " + name_ + ";\n";
	return code;
}

std::string
cconfig::schema::atom::generate_initialization() const
{
	// TODO: use static_visitor again
	if(type_ == typeid(std::string))
		return "generate_string(child_element, child_node)";
	else if(type_ == typeid(long))
		return "generate_long(child_element, child_node)";
	else if(type_ == typeid(bool))
		return "generate_bool(child_element, child_node)";
	else if(type_ == typeid(double))
		return "generate_double(child_element, child_node)";
	else
		throw std::runtime_error("Unknown error in config schema");
}

std::string
cconfig::schema::atom::generate_function() const
{
	return "";
}

std::string
cconfig::schema::atom::generate_tree_builder(int& unique_id, int indent) const
{
	// TODO: use static_visitor once more
	std::string s;
	std::string varname = "var" + boost::lexical_cast<std::string>(unique_id);

	indent_string(s, indent); s += "cconfig::schema::atom* " + varname
		+ " = new cconfig::schema::atom(";
	if(type_ == typeid(long))
		s += "typeid(long)";
	else if(type_ == typeid(bool))
		s += "typeid(bool)";
	else if(type_ == typeid(double))
		s += "typeid(double)";
	else if(type_ == typeid(std::string))
		s += "typeid(std::string)";
	s += ");\n";

	s += generate_common_tree_initialization(unique_id, indent);
	
	return s;
}

std::string
cconfig::schema::atom::generate_config_stub(int indent) const
{
	// TODO: use static_visitor for god's sake
	std::string s;
	
	if(type_ == typeid(long))
		s += "0";
	else if(type_ == typeid(bool))
		s += "false";
	else if(type_ == typeid(double))
		s += "0.0";
	else if(type_ == typeid(std::string))
		s += "\"\"";
	
	return s;
}

void
cconfig::schema::schema::load(const std::string& filename)
{
	ConfigSchemaLexer::InputStreamType input(reinterpret_cast<const ANTLR_UINT8*>(filename.c_str()), ANTLR_ENC_8BIT);
	ConfigSchemaLexer lexer(&input);
	ConfigSchemaParser::TokenStreamType tokens(ANTLR_SIZE_HINT, lexer.get_tokSource());
	ConfigSchemaParser parser(&tokens);

	root_ = parser.file();
}

cconfig::schema::validation_result
cconfig::schema::schema::validate(
		cconfig::file& config,
		bool strict)
{
	return root_->validate(config.root(), strict);
}

void
cconfig::schema::schema::generate_wrapper(
	const std::string& basename,
	const std::string& targetdir,
	const std::string& includepath) const
{
	// YUCK!!!
	std::string header;
	header += "// THIS FILE HAS BEEN GENERATED FROM THE SCHEMA FILE\n";
	header += "// DO NOT CHANGE THIS FILE IN ANY CASE!!\n\n";
	header += "#ifndef CONFIG_WRAPPER_H_\n";
	header += "#define CONFIG_WRAPPER_H_\n\n";
	header += "#include \"" + includepath + "config_file.hpp\"\n";
	header += "#include \"" + includepath + "config_schema.hpp\"\n\n";
	header += "#include <stdexcept>\n";
	header += "#include <string>\n";
	header += "#include <vector>\n\n";
	header += "namespace cconfig { namespace wrapper {\n\n";
	header += "class validation_error : public std::runtime_error\n";
	header += "{\n";
	header += "public:\n";
	header += "\tvalidation_error(const std::string& what) :\n";
	header += "\t\tstd::runtime_error(what) {}\n";
	header += "};\n\n";
	header += root_->generate_declaration();
	header += "Config load_config(const std::string& config_filename);\n";
	header += "cconfig::schema::schema* generate_schema();\n\n";
	header += "}}\n\n";
	header += "#endif\n";

	std::ofstream header_file((targetdir + "/" + basename + ".hpp").c_str());
	header_file << header;
	header_file.close();

	std::string cpp;
	cpp += "// THIS FILE HAS BEEN GENERATED FROM THE SCHEMA FILE\n";
	cpp += "// DO NOT CHANGE THIS FILE IN ANY CASE!!\n\n";
	cpp += "#include \"" + basename + ".hpp\"\n\n";
	cpp += "namespace {\n\n";
	cpp += "using namespace cconfig::wrapper;\n\n";
	cpp += "std::string generate_string(const cconfig::element& e, cconfig::schema::node*) { return e.as<std::string>(); }\n";
	cpp += "std::string generate_string(const cconfig::element& e, cconfig::schema::node*, const std::string& d) { try { return e.as<std::string>(); } catch(...) { return d; } }\n";
	cpp += "long generate_long(const cconfig::element& e, cconfig::schema::node*) { return e.as<long>(); }\n";
	cpp += "long generate_long(const cconfig::element& e, cconfig::schema::node*, long d) { try { return e.as<long>(); } catch(...) { return d; } }\n";
	cpp += "bool generate_bool(const cconfig::element& e, cconfig::schema::node*) { return e.as<bool>(); }\n";
	cpp += "bool generate_bool(const cconfig::element& e, cconfig::schema::node*, bool d) { try { return e.as<bool>(); } catch(...) { return d; } }\n";
	cpp += "double generate_double(const cconfig::element& e, cconfig::schema::node*) { return e.as<double>(); }\n";
	cpp += "double generate_double(const cconfig::element& e, cconfig::schema::node*, double d) { try { return e.as<double>(); } catch(...) { return d; } }\n\n";
	cpp += root_->generate_function();
	cpp += "\n}\n\n";
	cpp += "\ncconfig::wrapper::Config cconfig::wrapper::load_config(const std::string& config_filename)\n";
	cpp += "{\n";
	cpp += "\tcconfig::file* f = new cconfig::file;\n";
	cpp += "\tf->load(config_filename);\n\n";
	cpp += "\tcconfig::schema::schema* s = generate_schema();\n";
	cpp += "\tcconfig::schema::validation_result r = s->validate(*f, true);\n\n";
	cpp += "\tif(!r.valid)\n";
	cpp += "\t\tthrow validation_error(\"Validation failed at \" + ((r.error_uri == \"/\")?\"root level\":r.error_uri) + \": \" + r.error_message);\n\n";
	cpp += "\tcconfig::wrapper::Config c = generate_Config(f->root(), s->root());\n";
	cpp += "\tc.file_ = f;\n";
	cpp += "\tdelete s;\n";
	cpp += "\treturn c;\n";
	cpp += "}\n\n";
	cpp += "cconfig::schema::schema* cconfig::wrapper::generate_schema()\n";
	cpp += "{\n";
	int unique_id = 0;
	cpp += root_->generate_tree_builder(unique_id, 1);
	cpp += "\n\tcconfig::schema::schema* s = new cconfig::schema::schema;\n";
	cpp += "\ts->set(var0);\n";
	cpp += "\treturn s;\n";
	cpp += "}\n\n";

	std::ofstream cpp_file((targetdir + "/" + basename + ".cpp").c_str());
	cpp_file << cpp;
	cpp_file.close();
}

void
cconfig::schema::schema::generate_config_stub(const std::string& outputfile) const
{
	std::ofstream stub_file(outputfile.c_str());
	std::string s;

	for(cconfig::schema::group::node_map_type::const_iterator it = root_->children_.begin();
		it != root_->children_.end(); ++it)
	{
		s += it->first
			+ " = "
			+ it->second->generate_config_stub(0)
			+ ";\n";
	}

	stub_file << s;
	stub_file.close();
}

