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

#ifndef CONFIG_SCHEMA_HPP_
#define CONFIG_SCHEMA_HPP_

#include <boost/variant.hpp>
#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <stdexcept>
#include <iostream>

namespace cconfig {

class file;
class element;

namespace schema {

/**
 * @brief Custom exception class for cconfig::schema
 */
class exception : public std::runtime_error
{
public:
	exception(const std::string& what) :
		std::runtime_error(what)
	{}
};

/**
 * @brief Struct with information about the validation result
 *
 * An instance of this type is returned by the validate()
 * function and contains information about validity and
 * a possible error and its location.
 */
struct validation_result
{
	bool valid;
	std::string error_uri;
	std::string error_message;

	validation_result(
			bool _valid,
			const std::string& _error_uri="",
			const std::string& _error_message="") :
		valid(_valid),
		error_uri(_error_uri),
		error_message(_error_message)
	{}	
};

/**
 * @brief Abstract node class for the schema tree
 *
 * To simplify things all members of this class and its child
 * classes (group, list and atom) have been made public. The
 * validation classes are used only internally by the validator
 * and the code generator, so this should be no problem.
 */
class node
{
public:
	node() : required_(false), parent_(NULL) {}
	virtual ~node() {}

	std::string uri() const;
	std::string uri_safe() const;

	/**
	 * @brief Virtual function for validating the tree
	 *
	 * This is called by the validator on the root node of
	 * the config schema to recursively validate the config
	 * file against the schema tree.
	 *
	 * @param e Reference to the config file element that
	 * corresponds to this node
	 * @param strict Flag for enabling strict validation
	 * (when strict validation is enabled, the validator
	 * will ensure that all options in the config file are
	 * also defined in the schema to detect typos)
	 *
	 * @return validation_result object
	 */
	virtual validation_result validate(
			const cconfig::element& e,
			bool strict=false) const = 0;

	/**
	 * @brief Virtual function for generating declaration code (header file)
	 */
	virtual std::string generate_declaration() const = 0;

	/**
	 * @brief Virtual function for generating definition code (cpp file)
	 */
	virtual std::string generate_definition() const = 0;

	/**
	 * @brief Virtual function for generating initialization code (cpp file)
	 */
	virtual std::string generate_initialization() const = 0;

	/**
	 * @brief Virtual function for generating initialization function (cpp file)
	 */
	virtual std::string generate_function() const = 0;

	/**
	 * @brief Virtual function for generating the schema tree builder (cpp file)
	 *
	 * @param unique_id Reference to an (arbitrary) integer that is incremented
	 * internally for generating variable names for the tree builder
	 * @param indent Indentation width
	 */
	virtual std::string generate_tree_builder(int& unique_id, int indent) const = 0;

	/**
	 * @brief Virtual function for generating a config file stub
	 *
	 * @param variant Module to generate the stub for
	 * @param indent Indentation width
	 */
	virtual std::string generate_config_stub(int indent) const = 0;

	/**
	 * @brief Generates schema tree initialization common to all node types
	 *
	 * @param unique_id Integer for generating variable names for the tree builder
	 * @param indent Indentation width
	 */
	std::string generate_common_tree_initialization(int unique_id, int indent) const;
	
	/**
	 * @brief Utility function for indenting a string
	 */
	static void indent_string(std::string& s, int indent)
	{
		for(int i=0; i<indent; i++)
			s += "\t";
	}

	/**
	 * @brief Adds an attribute to the node
	 *
	 * @tparam T Type of the attribute
	 * @param name Name of the attribute
	 * @param value Attribute value
	 */
	template <typename T>
	void add_attribute(const std::string& name, const T& value)
	{
		attributes_.insert(
			std::make_pair(
				name,
				attribute_value_type(value)
			)
		);
	}

	/**
	 * @brief Function for checking if a specific attribute is set on the node
	 *
	 * @param name Name of the attribute
	 */
	bool has_attribute(const std::string& name) const;

	/**
	 * @brief Retrieves the value for an attribute
	 *
	 * Throws an exception if the attribute is not set.
	 *
	 * @tparam T Type of the attribute
	 * @param name Name of the attribute
	 *
	 * @return Attribute value
	 */
	template <typename T>
	T get_attribute(const std::string& name) const
	{
		attribute_map_type::const_iterator it = attributes_.find(name);
		if(it == attributes_.end())
			throw exception("Attribute not found (" + name + ")");

		try {
			return boost::get<T>(it->second);
		} catch(boost::bad_get& e) {
			throw exception("Invalid conversion requested for attribute "
				+ name + " (" + e.what() + ")");
		}
	}

	std::string name_;
	bool required_;

	node* parent_;

	/**
	 * @brief Visitor for the variants in attribute map
	 *
	 * Generates an initialization string for all allowed
	 * types in the attribute variant.
	 */
	struct attribute_visitor :
		boost::static_visitor<std::string>
	{
		std::string operator()(long x) const
		{ return "(long)" + boost::lexical_cast<std::string>(x); }
		
		std::string operator()(bool x) const
		{ return std::string("(bool)") + (x?"true":"false"); }
		
		std::string operator()(double x) const
		{ return "(double)" + boost::lexical_cast<std::string>(x); }
		
		std::string operator()(const std::string& x) const
		{ return "\"" + x + "\""; }
	};

	typedef boost::variant<long, bool, double, std::string> attribute_value_type;
	typedef std::map<std::string, attribute_value_type> attribute_map_type;
	attribute_map_type attributes_;
};

class group : public node
{
public:
	~group();

	void add_child(
			const std::string& name,
			node* n,
			bool required);
	
	validation_result validate(
			const cconfig::element& e,
			bool strict=false) const;
	
	std::string generate_declaration() const;
	std::string generate_definition() const;
	std::string generate_initialization() const;
	std::string generate_function() const;

	std::string generate_tree_builder(int& unique_id, int indent) const;
	std::string generate_config_stub(int indent) const;

	typedef std::map<std::string, node*> node_map_type;
	node_map_type children_;
};

class list : public node
{
public:
	~list();

	void add_child(node* n);
	
	validation_result validate(
			const cconfig::element& e,
			bool strict=false) const;
	
	std::string generate_declaration() const;
	std::string generate_definition() const;
	std::string generate_initialization() const;
	std::string generate_function() const;
	
	std::string generate_tree_builder(int& unique_id, int indent) const;
	std::string generate_config_stub(int indent) const;

	typedef std::vector<node*> node_list_type;
	node_list_type children_;
};

class atom : public node
{
public:
	atom(const std::type_info& type) :
		type_(type) {}

	validation_result validate(
			const cconfig::element& e,
			bool strict=false) const;
	
	std::string generate_declaration() const;
	std::string generate_definition() const;
	std::string generate_initialization() const;
	std::string generate_function() const;

	std::string generate_tree_builder(int& unique_id, int indent) const;
	std::string generate_config_stub(int indent) const;

	/**
	 * @brief Generates type string from the variant type
	 */
	std::string c_type_string() const;

	const std::type_info& type_;
};

/**
 * @brief Class encapsulating a config schema
 */
class schema
{
public:
	schema() {}
	explicit schema(const std::string& filename) { load(filename); }

	~schema() { delete root_; }

	/**
	 * @brief Loads the schema from a file
	 *
	 * @param filename Full path of schema file
	 */
	void load(const std::string& filename);

	/**
	 * @brief Allows to set the root node manually
	 *
	 * This is used internally by the generated tree builder
	 *
	 * @param root Pointer to the root node (transfers ownership)
	 */
	void set(group* root) { root_ = root; }

	/**
	 * @brief Validates a config file
	 * 
	 * This is called to recursively validate the config
	 * file against the schema tree.
	 * 
	 * @param e Reference to the config file element that
	 * corresponds to this node
	 * @param strict Flag for enabling strict validation
	 * (when strict validation is enabled, the validator
	 * will ensure that all options in the config file are
	 * also defined in the schema to detect typos)
	 *
	 * @return validation_result object
	 */
	validation_result validate(
			cconfig::file& config,
			bool strict=false);

	/**
	 * @brief Generates wrapper code
	 *
	 * This function generates code with a static representation
	 * of the config file and schema structure and is only used
	 * internally in generate_wrapper.cpp.
	 *
	 * @param basename Base name of the resulting generated files
	 * (extension is appended by the generator)
	 * @param targetdir Directory to put the generated files into
	 * @param includepath Relative path to the includes that will
	 * be written into the generated files so that the compiler
	 * can find the config file and schema headers
	 */
	void generate_wrapper(const std::string& basename, const std::string& targetdir,
		const std::string& includepath) const;
	
	void generate_config_stub(const std::string& outputfile) const;
	
	node* root() { return root_; }

private:
	group* root_;
};

}}

#endif
