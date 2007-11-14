/*
 * phc -- the open source PHP compiler
 * See doc/license/README.license for licensing information
 *
 * Convert the phc AST to XML format
 */

#ifndef PHC_XML_UNPARSER
#define PHC_XML_UNPARSER

#include <iostream>
#include "lib/base64.h"
#include "lib/demangle.h"
#include "lib/error.h"
#include "lib/String.h"
#include "lib/List.h"
#include "cmdline.h"

template
<
	class Script,
	class Node,
	class Visitor,
	class Identifier,
	class Literal,
	class Null
>
class XML_unparser : public Visitor 
{
protected:
	ostream& os;
	int indent;
	bool print_attrs;

	void print_indent()
	{
		extern gengetopt_args_info args_info;
		for(int i = 0; i < indent; i++)
			os << args_info.tab_arg;
	}

	bool needs_encoding(String* str)
	{
		String::const_iterator i;

		for(i = str->begin(); i != str->end(); i++)
		{
			if(*i < 32)
				return true;

			if(*i > 126)
				return true;

			if(*i == '<' || *i == '>' || *i == '&')
				return true;
		}

		return false;
	}

public:
	XML_unparser(ostream& os = cout, bool print_attrs = true)
		: os(os), print_attrs (print_attrs)
	{
		indent = 0;
	}

	virtual ~XML_unparser() 
	{
	}

public:

	void visit_marker(char const* name, bool value)
	{
		print_indent();
		os << "<bool>" 
			<< "<!-- " << name << " -->"
			<< (value ? "true" : "false") << "</bool>" << endl;
	}

	void visit_null(char const* type_id)
	{
		print_indent();
		os << "<" << type_id << " xsi:nil=\"true\" />" << endl;
	}

	void visit_null_list(char const* type_id)
	{
		print_indent();
		os << "<" << type_id << "_list xsi:nil=\"true\" />" << endl;
	}

	void pre_list(char const* type_id, int size)
	{
		print_indent();
		os << "<" << type_id << "_list>" << endl;
		indent++;
	}

	void post_list(char const* type_id, int size)
	{
		indent--;
		print_indent();
		os << "</" << type_id << "_list>" << endl;
	}

public:

	void pre_node(Node* in)
	{
		bool is_root = dynamic_cast<Script*>(in);

		if(is_root)
			os << "<?xml version=\"1.0\"?>" << endl;

		print_indent();
		os << "<" << demangle(in);

		if(is_root)
		{
			os << " xmlns=\"http://www.phpcompiler.org/phc-1.1\"";
			os << " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"";
		}

		os << ">" << endl;
		indent++;

		if(print_attrs) print_attributes(in);
	}

	void post_node(Node* in)
	{
		indent--;
		print_indent();
		os << "</" << demangle(in) << ">" << endl;
	}

	void pre_identifier(Identifier* in)
	{
		String* value = in->get_value_as_string();

		print_indent();
		if(needs_encoding(value))
			os << "<value encoding=\"base64\">" << *base64_encode(value) << "</value>" << endl;
		else
			os << "<value>" << *value << "</value>" << endl;;

	}

	void pre_literal(Literal* in)
	{
		String* value = in->get_value_as_string();
		String* source_rep = in->get_source_rep();

		// Token_null does not have a value
		if(!dynamic_cast<Null*>(in))
		{
			print_indent();
			if(needs_encoding(value))
				os << "<value encoding=\"base64\">" << *base64_encode(value) << "</value>" << endl;
			else	
				os << "<value>" << *value << "</value>" << endl;
		}

		print_indent();
		if(needs_encoding(source_rep))
			os << "<source_rep encoding=\"base64\">" << *base64_encode(source_rep) << "</source_rep>" << endl;
		else	
			os << "<source_rep>" << *source_rep << "</source_rep>" << endl;
	}

protected:

	void print_attributes(Node* in)
	{
		if(in->attrs->size() == 0)
		{
			print_indent();
			os << "<attrs />" << endl;
		}
		else
		{
			print_indent();
			os << "<attrs>" << endl;
			indent++;

			AttrMap::const_iterator i;
			for(i = in->attrs->begin(); i != in->attrs->end(); i++)
			{
				print_attribute((*i).first, (*i).second);
			}

			indent--;
			print_indent();
			os << "</attrs>" << endl;
		}
	}

	void print_attribute(string name, Object* attr)
	{
		print_indent();

		if(String* str = dynamic_cast<String*>(attr))
		{
			os << "<attr key=\"" << name << "\"><string>" << *str << "</string></attr>" << endl;
		}
		else if(Integer* i = dynamic_cast<Integer*>(attr))
		{
			os << "<attr key=\"" << name << "\"><integer>" << i->value () << "</integer></attr>" << endl;
		}
		else if(Boolean* b = dynamic_cast<Boolean*>(attr))
		{
			os << "<attr key=\"" << name << "\"><bool>" << (b->value() ? "true" : "false") << "</bool></attr>" << endl;
		}
		else if(List<String*>* ls = dynamic_cast<List<String*>*>(attr))
		{
			os << "<attr key=\"" << name << "\"><string_list>" << endl;
			indent++;

			List<String*>::const_iterator j;
			for(j = ls->begin(); j != ls->end(); j++)
			{
				print_indent();
				os << "<string";

				if(needs_encoding(*j))
				{
					os << " encoding=\"base64\">";
					os << *base64_encode(*j);
				}
				else	
				{
					os << ">";
					os << **j;
				}

				os << "</string>" << endl;
			}

			indent--;
			print_indent();
			os << "</string_list></attr>" << endl;
		}
		else if (attr == NULL)
		{
			os << "<!-- skipping NULL attribute " << name << " -->" << endl;
		}
		else
			phc_warning("Don't know how to deal with attribute '%s' of type '%s'", name.c_str(), demangle(attr));	
	}

};

#include "AST_visitor.h"
class AST_XML_unparser : public XML_unparser
<
	AST::AST_php_script,
	AST::AST_node,
	AST::AST_visitor, 
	AST::AST_identifier, 
	AST::AST_literal,
	AST::Token_null
>
{
public:
	AST_XML_unparser(ostream& os = cout, bool print_attrs = true)
	: XML_unparser<
			AST::AST_php_script,
			AST::AST_node,
			AST::AST_visitor, 
			AST::AST_identifier, 
			AST::AST_literal,
			AST::Token_null
		> (os, print_attrs)
	{
	}
};

#include "HIR_visitor.h"
class HIR_XML_unparser : public XML_unparser
<
	HIR::HIR_php_script, 
	HIR::HIR_node, 
	HIR::HIR_visitor, 
	HIR::HIR_identifier, 
	HIR::HIR_literal,
	HIR::Token_null
> 
{
public:
	HIR_XML_unparser(ostream& os = cout, bool print_attrs = true)
	: XML_unparser<
			HIR::HIR_php_script,
			HIR::HIR_node,
			HIR::HIR_visitor, 
			HIR::HIR_identifier, 
			HIR::HIR_literal,
			HIR::Token_null
		> (os, print_attrs)

	{
	}
};

#endif // PHC_XML_UNPARSER
