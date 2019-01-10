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

#ifndef CONFIG_TREE_HPP_
#define CONFIG_TREE_HPP_

#include <string>
#include <deque>

#include <boost/variant.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/type_traits/is_arithmetic.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/foreach.hpp>

#include <boost/xpressive/xpressive.hpp>

namespace cconfig {

class exception : public std::runtime_error
{
public:
   exception(const std::string& what) :
      std::runtime_error(what)
   {}
};

class parse_error : public cconfig::exception
{
public:
   parse_error(const std::string& what) :
      cconfig::exception(what)
   {}
};

class lookup_error : public cconfig::exception
{
public:
   lookup_error(const std::string& what) :
      cconfig::exception(what)
   {}
};

///
/// \brief A typedef for the return type of the split function.
///
typedef std::deque<boost::variant<std::string, unsigned int> > token_list;

namespace util {
   ///
   /// \brief Function for splitting lookup paths.
   /// 
   /// Lookup paths consist of group names separated by dots. Each group
   /// name may be followed by one or more index numbers enclosed in
   /// square brackets.
   ///
   /// \param s Lookup path that is to be splitted.
   /// \returns A deque containing strings or integers resembling group names
   /// and index numbers.
   ///
   static token_list split(const std::string& s)
   {
      using namespace boost::xpressive;
   
      token_list result;
      std::deque<std::string> tokens;
      std::string stripped = boost::algorithm::erase_all_copy(s, "]");
      boost::algorithm::split(tokens, stripped, boost::algorithm::is_any_of(".["));
   
      sregex rexName = +_w;
      sregex rexNumber = +_d;
      smatch what;
   
      BOOST_FOREACH(const std::string& t, tokens)
      {
         if(t.empty())
            throw cconfig::lookup_error("Subsequent path separators found in config path (" + s + ")");
   
         if(regex_match(t, what, rexNumber)) {
            result.push_back(boost::lexical_cast<unsigned int>(t));
         }
         else if(regex_match(t, what, rexName)) {
            result.push_back(t);
         }
         else {
            throw cconfig::lookup_error("Failed to parse config path (" + s + ") at token " + t);
         }
      }
   
      return result;
   }
}

namespace atom_detail {

///
/// \brief Visitor for the variant in cconfig::atom
///
/// This visitor enables numeric and lexical casting from the allowed types in atom
/// to arbitrary types by specializing for std::string.
///
template<typename T>
struct visitor : public boost::static_visitor<T>
{
   // note: this class is specialized for std::string below
   template <typename U>
   typename boost::enable_if<boost::is_arithmetic<U>, const T>::type
   operator()(const U& value) const { return boost::numeric_cast<T>(value); }

   template <typename U>
   typename boost::enable_if<boost::is_same<U, std::string>, const T>::type
   operator()(const U& value) const { return boost::lexical_cast<T>(value); }
};

template<>
struct visitor<std::string> : public boost::static_visitor<std::string>
{
   template <typename U>
   typename boost::enable_if<boost::is_arithmetic<U>, const std::string>::type
   operator()(const U& value) const { return boost::lexical_cast<std::string>(value); }

   template <typename U>
   typename boost::enable_if<boost::is_same<U, std::string>, const std::string>::type
   operator()(const U& value) const { return value; }
};

}

class group;
class list;
class atom;

///
/// \brief Abstract class resembling a single configuration setting.
///
/// This is the base class for all config element types (group, list
/// and atom). Their instances form a tree in memory which resembles
/// the configuration setting hierarchy.
///
class element
{
public:
   virtual ~element() {}

   const group& as_group() const;
   const list& as_list() const;
   const atom& as_atom() const;

   const element& operator[](const std::string& key) const;
   const element& operator[](size_t index) const;

   template<typename T>
   const T lookup(const std::string& path) const;

   template<typename T>
   const T lookup(const std::string& path, const T& default_value) const;

   template<typename T>
   const T as() const;

   template<typename T>
   operator T() const
   {
      return as<T>();
   }

protected:
   element() {}
   element(const element&) {}

private:
   const element& recursive_lookup(const element& e, token_list tokens) const;

   class visitor : public boost::static_visitor<const element&>
   {
   public:
      visitor(const element& current) : current_(current) {}

      const element& operator()(const std::string& s) const { return current_[s]; }
      const element& operator()(unsigned int i) const { return current_[i]; }
   
   private:
      const element& current_;
   };

};

class group : public element
{
public:
   group() {}

   void insert(const std::string& key, element* value) { std::string key_(key); settings_.insert(key_, value); }
   const element& get(const std::string& key) const;

private:
   typedef boost::ptr_map<std::string, element> setting_map_t;
   setting_map_t settings_;
   
public:
   typedef setting_map_t::const_iterator iterator;
   iterator begin() const { return settings_.begin(); }
   iterator end() const { return settings_.end(); }
};

class list : public element
{
public:
   list() {}

   void append(element* value) { settings_.push_back(value); }
   const element& get(size_t index) const { return settings_[index]; }

   size_t size() const { return settings_.size(); }
   bool empty() const { return settings_.empty(); }

private:
   typedef boost::ptr_vector<element> setting_list_t;
   setting_list_t settings_;

public:
   typedef setting_list_t::const_iterator iterator;
   iterator begin() const { return settings_.begin(); }
   iterator end() const { return settings_.end(); }
};

class atom : public element
{
public:
   explicit atom(const long& value) : value_(value) {}
   explicit atom(const double& value) : value_(value) {}
   explicit atom(const std::string& value) : value_(value) {}
   explicit atom(const bool& value) : value_(value) {}

   const std::type_info& type() const { return value_.type(); }

   template<typename T>
   const T as() const
   {
      return boost::apply_visitor(atom_detail::visitor<T>(), value_);
   }

private:
   boost::variant<bool, long, double, std::string> value_;
};

inline const group& element::as_group() const
{
   try {
      return dynamic_cast<const group&>(*this);
   } catch(std::bad_cast&) {
      // TODO: put location of element in hierarchy into error message
      throw cconfig::lookup_error("Config setting is not a group");
   }
}

inline const list& element::as_list() const
{
   try {
      return dynamic_cast<const list&>(*this);
   } catch(std::bad_cast&) {
      // TODO: put location of element in hierarchy into error message
      throw cconfig::lookup_error("Config setting is not a group");
   }
}

inline const atom& element::as_atom() const
{
   try {
      return dynamic_cast<const atom&>(*this);
   } catch(std::bad_cast&) {
      // TODO: put location of element in hierarchy into error message
      throw cconfig::lookup_error("Config setting is not a group");
   }
}

inline const element& element::operator[](const std::string& key) const
{
   if(key.find_first_of(".[") != std::string::npos)
   {
      token_list tokens = util::split(key);
      try {
         return recursive_lookup(*this, tokens);
      } catch(cconfig::lookup_error&) {
         throw cconfig::lookup_error("Config setting not found (" + key + ")");
      }
   }
   else
      return this->as_group().get(key);
}

inline const element& element::operator[](size_t index) const
{
   return this->as_list().get(index);
}

template<typename T>
inline const T element::lookup(const std::string& path) const
{
   token_list tokens = util::split(path);
   try {
      return recursive_lookup(*this, tokens).as<T>();
   } catch(cconfig::lookup_error&) {
      throw cconfig::lookup_error("Config setting not found (" + path + ")");
   }
}

template<typename T>
inline const T element::lookup(const std::string& path, const T& default_value) const
{
   try {
      return lookup<T>(path);
   }
   catch(cconfig::lookup_error&) {
      return default_value;
   }
}

inline const element& element::recursive_lookup(const element& e, token_list tokens) const
{
   if(tokens.empty())
      return e;

   boost::variant<std::string, unsigned int> t = tokens.front();
   tokens.pop_front();
   
   const element& next(boost::apply_visitor(visitor(e), t));
   return recursive_lookup(next, tokens);
}

template<typename T>
inline const T element::as() const
{
   return this->as_atom().as<T>();
}

inline const element& group::get(const std::string& key) const
{
   setting_map_t::const_iterator it = settings_.find(key);
   if(it == settings_.end())
      throw cconfig::lookup_error("Element not found (" + key + ")");
   return *it->second;
}

}

#endif
