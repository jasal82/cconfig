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

#include <boost/program_options.hpp>
namespace po = boost::program_options;

int main(int argc, char** argv)
{
   std::string output_file;
   std::string filename;

   po::options_description desc("Allowed options");
   desc.add_options()
      ("help", "show this message")
      ("outputfile,o",
         po::value<std::string>(&output_file)->default_value("config_stub"),
         "output file name without extension (default 'config_stub')")
      ("schema,s",
         po::value<std::string>(&filename),
         "schema file")
      ;

   po::positional_options_description pdesc;
   pdesc.add("schema", 1);

   boost::program_options::variables_map vm;
   po::store(po::command_line_parser(argc, argv).options(desc).positional(pdesc).run(), vm);
   po::notify(vm);

   std::cout << "CConfig stub generator v1.0" << std::endl;

   if(vm.count("help") || vm.count("schema") == 0)
   {
      std::cout << "Usage: cconfig_stub_gen [options] schemafile" << std::endl;
      std::cout << desc << std::endl;
      return 0;
   }
   
   cconfig::schema::schema s;
   s.load(filename);
   s.generate_config_stub(output_file);

   return 0;
}

