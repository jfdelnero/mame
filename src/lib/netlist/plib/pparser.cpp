// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * pparser.c
 *
 */

#include "pparser.h"
#include "palloc.h"
#include "putil.h"

//#include <cstdarg>

namespace plib {
// ----------------------------------------------------------------------------------------
// A simple tokenizer
// ----------------------------------------------------------------------------------------

pstring ptokenizer::currentline_str()
{
	return m_cur_line;
}


void ptokenizer::skipeol()
{
	pstring::value_type c = getc();
	while (c)
	{
		if (c == 10)
		{
			c = getc();
			if (c != 13)
				ungetc(c);
			return;
		}
		c = getc();
	}
}


pstring::value_type ptokenizer::getc()
{
	if (m_unget != 0)
	{
		pstring::value_type c = m_unget;
		m_unget = 0;
		return c;
	}
	if (m_px == m_cur_line.end())
	{
		m_lineno++;
		if (m_strm.readline(m_cur_line))
			m_px = m_cur_line.begin();
		else
			return 0;
		return '\n';
	}
	pstring::value_type c = *(m_px++);
	return c;
}

void ptokenizer::ungetc(pstring::value_type c)
{
	m_unget = c;
}

void ptokenizer::require_token(const token_id_t &token_num)
{
	require_token(get_token(), token_num);
}

void ptokenizer::require_token(const token_t &tok, const token_id_t &token_num)
{
	if (!tok.is(token_num))
	{
		pstring val("");
		for (auto &i : m_tokens)
			if (i.second.id() == token_num.id())
				val = i.first;
		error(pfmt("Expected token <{1}> got <{2}>")(val)(tok.str()) );
	}
}

pstring ptokenizer::get_string()
{
	token_t tok = get_token();
	if (!tok.is_type(STRING))
	{
		error(pfmt("Expected a string, got <{1}>")(tok.str()) );
	}
	return tok.str();
}

pstring ptokenizer::get_identifier()
{
	token_t tok = get_token();
	if (!tok.is_type(IDENTIFIER))
	{
		error(pfmt("Expected an identifier, got <{1}>")(tok.str()) );
	}
	return tok.str();
}

pstring ptokenizer::get_identifier_or_number()
{
	token_t tok = get_token();
	if (!(tok.is_type(IDENTIFIER) || tok.is_type(NUMBER)))
	{
		error(pfmt("Expected an identifier or number, got <{1}>")(tok.str()) );
	}
	return tok.str();
}

// FIXME: combine into template
double ptokenizer::get_number_double()
{
	token_t tok = get_token();
	if (!tok.is_type(NUMBER))
	{
		error(pfmt("Expected a number, got <{1}>")(tok.str()) );
	}
	bool err(false);
	auto ret = plib::pstonum_ne<double, true>(tok.str(), err);
	if (err)
		error(pfmt("Expected a number, got <{1}>")(tok.str()) );
	return ret;
}

long ptokenizer::get_number_long()
{
	token_t tok = get_token();
	if (!tok.is_type(NUMBER))
	{
		error(pfmt("Expected a long int, got <{1}>")(tok.str()) );
	}
	bool err(false);
	auto ret = plib::pstonum_ne<long, true>(tok.str(), err);
	if (err)
		error(pfmt("Expected a long int, got <{1}>")(tok.str()) );
	return ret;
}

ptokenizer::token_t ptokenizer::get_token()
{
	while (true)
	{
		token_t ret = get_token_internal();
		if (ret.is_type(ENDOFFILE))
			return ret;

		if (m_support_line_markers && ret.is_type(LINEMARKER))
		{
			// FIXME: do something with information gathered
			ret = get_token_internal();
			if (!ret.is_type(NUMBER))
				error(pfmt("Expected line number after line marker but got <{1}>")(ret.str()) );
			ret = get_token_internal();
			if (!ret.is_type(STRING))
				error(pfmt("Expected file name after line marker but got <{1}>")(ret.str()) );
			ret = get_token_internal();
			while (ret.is_type(NUMBER))
			{
				// FIXME: process flags; actually only 1 (file enter) and 2 (after file exit)
				ret = get_token_internal();
			}
		}

		if (ret.is(m_tok_comment_start))
		{
			do {
				ret = get_token_internal();
			} while (ret.is_not(m_tok_comment_end));
		}
		else if (ret.is(m_tok_line_comment))
		{
			skipeol();
		}
#if 0
		else if (ret.str() == "#")
		{
			skipeol();
		}
#endif
		else
		{
			return ret;
		}
	}
}

ptokenizer::token_t ptokenizer::get_token_internal()
{
	/* skip ws */
	pstring::value_type c = getc();
	while (m_whitespace.find(c) != pstring::npos)
	{
		c = getc();
		if (eof())
		{
			return token_t(ENDOFFILE);
		}
	}
	if (m_support_line_markers && c == '#')
		return token_t(LINEMARKER);
	else if (m_number_chars_start.find(c) != pstring::npos)
	{
		/* read number while we receive number or identifier chars
		 * treat it as an identifier when there are identifier chars in it
		 *
		 */
		token_type ret = NUMBER;
		pstring tokstr = "";
		while (true) {
			if (m_identifier_chars.find(c) != pstring::npos && m_number_chars.find(c) == pstring::npos)
				ret = IDENTIFIER;
			else if (m_number_chars.find(c) == pstring::npos)
				break;
			tokstr += c;
			c = getc();
		}
		ungetc(c);
		return token_t(ret, tokstr);
	}
	else if (m_identifier_chars.find(c) != pstring::npos)
	{
		/* read identifier till non identifier char */
		pstring tokstr = "";
		while (m_identifier_chars.find(c) != pstring::npos)
		{
			tokstr += c;
			c = getc();
		}
		ungetc(c);
		auto id = m_tokens.find(tokstr);
		if (id != m_tokens.end())
			return token_t(id->second, tokstr);
		else
			return token_t(IDENTIFIER, tokstr);
	}
	else if (c == m_string)
	{
		pstring tokstr = "";
		c = getc();
		while (c != m_string)
		{
			tokstr += c;
			c = getc();
		}
		return token_t(STRING, tokstr);
	}
	else
	{
		/* read identifier till first identifier char or ws */
		pstring tokstr = "";
		while ((m_identifier_chars.find(c) == pstring::npos) && (m_whitespace.find(c) == pstring::npos))
		{
			tokstr += c;
			/* expensive, check for single char tokens */
			if (tokstr.length() == 1)
			{
				auto id = m_tokens.find(tokstr);
				if (id != m_tokens.end())
					return token_t(id->second, tokstr);
			}
			c = getc();
		}
		ungetc(c);
		auto id = m_tokens.find(tokstr);
		if (id != m_tokens.end())
			return token_t(id->second, tokstr);
		else
			return token_t(UNKNOWN, tokstr);
	}
}

// ----------------------------------------------------------------------------------------
// A simple preprocessor
// ----------------------------------------------------------------------------------------

ppreprocessor::ppreprocessor(psource_collection_t<> &sources, defines_map_type *defines)
: std::istream(new readbuffer(this))
, m_sources(sources)
, m_if_flag(0)
, m_if_level(0)
, m_pos(0)
, m_state(PROCESS)
, m_comment(false)
{
	m_expr_sep.emplace_back("!");
	m_expr_sep.emplace_back("(");
	m_expr_sep.emplace_back(")");
	m_expr_sep.emplace_back("+");
	m_expr_sep.emplace_back("-");
	m_expr_sep.emplace_back("*");
	m_expr_sep.emplace_back("/");
	m_expr_sep.emplace_back("&&");
	m_expr_sep.emplace_back("||");
	m_expr_sep.emplace_back("==");
	m_expr_sep.emplace_back(",");
	m_expr_sep.emplace_back(" ");
	m_expr_sep.emplace_back("\t");

	if (defines != nullptr)
		m_defines = *defines;
	m_defines.insert({"__PLIB_PREPROCESSOR__", define_t("__PLIB_PREPROCESSOR__", "1")});
}

void ppreprocessor::error(const pstring &err)
{
	pstring s("");
	pstring trail      ("                 from ");
	pstring trail_first("In file included from ");
	pstring e = plib::pfmt("PREPRO ERROR: {1}:{2}:0: error: {3}\n")
			(m_stack.top().m_name, m_stack.top().m_lineno, err);
	m_stack.pop();
	while (m_stack.size() > 0)
	{
		if (m_stack.size() == 1)
			trail = trail_first;
		s = trail + plib::pfmt("{1}:{2}:0\n")(m_stack.top().m_name, m_stack.top().m_lineno) + s;
		m_stack.pop();
	}
	throw pexception("\n" + s + e);
}

#define CHECKTOK2(p_op, p_prio) \
	else if (tok == # p_op)                         \
	{                                               \
		if (!has_val)								\
			{ error("parsing error!"); return 1;}	\
		if (prio < (p_prio))                        \
			return val;                             \
		start++;                                    \
		const auto v2 = expr(sexpr, start, (p_prio)); \
		val = (val p_op v2);                        \
	}                                               \

// Operator precedence see https://en.cppreference.com/w/cpp/language/operator_precedence

int ppreprocessor::expr(const std::vector<pstring> &sexpr, std::size_t &start, int prio)
{
	int val(0);
	bool has_val(false);

	pstring tok=sexpr[start];
	if (tok == "(")
	{
		start++;
		val = expr(sexpr, start, /*prio*/ 255);
		if (sexpr[start] != ")")
			error("parsing error!");
		has_val = true;
		start++;
	}
	while (start < sexpr.size())
	{
		tok=sexpr[start];
		if (tok == ")")
		{
			if (!has_val)
			{
				error("parsing error!");
				return 1; // tease compiler
			}
			else
				return val;
		}
		else if (tok == "!")
		{
			if (prio < 3)
			{
				if (!has_val)
				{
					error("parsing error!");
					return 1; // tease compiler
				}
				else
					return val;
			}
			start++;
			val = !expr(sexpr, start, 3);
			has_val = true;
		}
		CHECKTOK2(*,  5)
		CHECKTOK2(/,  5) // NOLINT(clang-analyzer-core.DivideZero)
		CHECKTOK2(+,  6)
		CHECKTOK2(-,  6)
		CHECKTOK2(==, 10)
		CHECKTOK2(&&, 14)
		CHECKTOK2(||, 15)
		else
		{
			val = plib::pstonum<decltype(val)>(tok);
			has_val = true;
			start++;
		}
	}
	if (!has_val)
	{
		error("parsing error!");
		return 1; // tease compiler
	}
	else
		return val;
}

ppreprocessor::define_t *ppreprocessor::get_define(const pstring &name)
{
	auto idx = m_defines.find(name);
	return (idx != m_defines.end()) ? &idx->second : nullptr;
}

pstring ppreprocessor::replace_macros(const pstring &line)
{
	std::vector<pstring> elems(psplit(line, m_expr_sep));
	pstring ret("");
	for (auto & elem : elems)
	{
		define_t *def = get_define(elem);
		ret += (def != nullptr) ? def->m_replace : elem;
	}
	return ret;
}

static pstring catremainder(const std::vector<pstring> &elems, std::size_t start, const pstring &sep)
{
	pstring ret("");
	for (std::size_t i = start; i < elems.size(); i++)
	{
		ret += elems[i];
		ret += sep;
	}
	return ret;
}

pstring ppreprocessor::process_comments(pstring line)
{
	bool in_string = false;

	std::size_t e = line.size();
	pstring ret = "";
	for (std::size_t i=0; i < e; )
	{
		pstring c = plib::left(line, 1);
		line = line.substr(1);
		if (!m_comment)
		{
			if (c=="\"")
			{
				in_string = !in_string;
				ret += c;
			}
			else if (in_string && c=="\\")
			{
				i++;
				ret += (c + plib::left(line, 1));
				line = line.substr(1);
			}
			else if (!in_string && c=="/" && plib::left(line,1) == "*")
				m_comment = true;
			else if (!in_string && c=="/" && plib::left(line,1) == "/")
				break;
			else
				ret += c;
		}
		else
			if (c=="*" && plib::left(line,1) == "/")
			{
				i++;
				line = line.substr(1);
				m_comment = false;
			}
		i++;
	}
	return ret;
}

pstring ppreprocessor::process_line(pstring line)
{
	bool line_cont = plib::right(line, 1) == "\\";
	if (line_cont)
		line = plib::left(line, line.size() - 1);

	if (m_state == LINE_CONTINUATION)
		m_line += line;
	else
		m_line = line;

	if (line_cont)
	{
		m_state = LINE_CONTINUATION;
		return "";
	}
	else
		m_state = PROCESS;

	line = process_comments(m_line);

	pstring lt = plib::trim(plib::replace_all(line, "\t", " "));
	pstring ret;
	// FIXME ... revise and extend macro handling
	if (plib::startsWith(lt, "#"))
	{
		std::vector<pstring> lti(psplit(lt, " ", true));
		if (lti[0] == "#if")
		{
			m_if_level++;
			std::size_t start = 0;
			lt = replace_macros(lt);
			std::vector<pstring> t(psplit(replace_all(lt.substr(3), " ", ""), m_expr_sep));
			auto val = static_cast<int>(expr(t, start, 255));
			if (val == 0)
				m_if_flag |= (1 << m_if_level);
		}
		else if (lti[0] == "#ifdef")
		{
			m_if_level++;
			if (get_define(lti[1]) == nullptr)
				m_if_flag |= (1 << m_if_level);
		}
		else if (lti[0] == "#ifndef")
		{
			m_if_level++;
			if (get_define(lti[1]) != nullptr)
				m_if_flag |= (1 << m_if_level);
		}
		else if (lti[0] == "#else")
		{
			m_if_flag ^= (1 << m_if_level);
		}
		else if (lti[0] == "#endif")
		{
			m_if_flag &= ~(1 << m_if_level);
			m_if_level--;
		}
		else if (lti[0] == "#include")
		{
			if (m_if_flag == 0)
			{
				pstring arg("");
				for (std::size_t i=1; i<lti.size(); i++)
					arg += (lti[i] + " ");

				arg = plib::trim(arg);

				if (startsWith(arg, "\"") && endsWith(arg, "\""))
				{
					arg = arg.substr(1, arg.length() - 2);
					/* first try local context */
					auto l(plib::util::buildpath({m_stack.top().m_local_path, arg}));
					auto lstrm(m_sources.get_stream<>(l));
					if (lstrm)
					{
						m_stack.emplace(input_context(std::move(lstrm), plib::util::path(l), l));
					}
					else
					{
						auto strm(m_sources.get_stream<>(arg));
						if (strm)
						{
							m_stack.emplace(input_context(std::move(strm), plib::util::path(arg), arg));
						}
						else
							error("include not found:" + arg);
					}
				}
				else
					error("include misspelled:" + arg);
			}
		}
		else if (lti[0] == "#pragma")
		{
			if (m_if_flag == 0 && lti.size() > 3 && lti[1] == "NETLIST")
			{
				if (lti[2] == "warning")
					error("NETLIST: " + catremainder(lti, 3, " "));
			}
		}
		else if (lti[0] == "#define")
		{
			if (m_if_flag == 0)
			{
				if (lti.size() < 2)
					error("define needs at least one argument: " + line);
				else if (lti.size() == 2)
					m_defines.insert({lti[1], define_t(lti[1], "")});
				else
				{
					pstring arg("");
					for (std::size_t i=2; i<lti.size() - 1; i++)
						arg += lti[i] + " ";
					arg += lti[lti.size()-1];
					m_defines.insert({lti[1], define_t(lti[1], arg)});
				}
			}
		}
		else
		{
			if (m_if_flag == 0)
				error(pfmt("unknown directive on line {1}: {2}")(m_stack.top().m_lineno)(replace_macros(line)));
		}
	}
	else
	{
		lt = replace_macros(lt);
		if (m_if_flag == 0)
			ret += lt;
	}
	return ret;
}

void ppreprocessor::process_stack()
{
	while (m_stack.size() > 0)
	{
		pstring line;
		pstring linemarker = pfmt("# {1} \"{2}\" 1\n")(m_stack.top().m_lineno, m_stack.top().m_name);
		m_outbuf += decltype(m_outbuf)(linemarker.c_str());
		while (m_stack.top().m_reader.readline(line))
		{
			m_stack.top().m_lineno++;
			line = process_line(line);
			m_outbuf += decltype(m_outbuf)(line.c_str()) + static_cast<char>(10);
		}
		m_stack.pop();
		if (m_stack.size() > 0)
		{
			linemarker = pfmt("# {1} \"{2}\" 2\n")(m_stack.top().m_lineno, m_stack.top().m_name);
			m_outbuf += decltype(m_outbuf)(linemarker.c_str());
		}
	}
}



} // namespace plib
