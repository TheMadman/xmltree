/*
 * XMLTree - An XML Parser-Helper Library
 * Copyright (C) 2025  Marcus Harrison
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef DESCENT_XML_LEX
#define DESCENT_XML_LEX

#ifdef __cplusplus
extern "C" {
#endif



#include <wchar.h>
#include <wctype.h>

#include <libadt.h>

#include "classifier.h"

/**
 * \file
 */

/**
 * \brief Represents a single token.
 */
struct descent_xml_lex {
	/**
	 * \brief Represents the type of token classifiered.
	 */
	descent_xml_classifier_fn *type;

	/**
	 * \brief A pointer to the full script.
	 */
	struct libadt_const_lptr script;

	/**
	 * \brief A pointer to the classifiered value.
	 *
	 * This will always be a pointer into .script.
	 */
	struct libadt_const_lptr value;
};

inline ssize_t _descent_xml_lex_mbrtowc(
	wchar_t *result,
	struct libadt_const_lptr string,
	mbstate_t *_mbstate
)
{
	if (string.length <= 0) {
		*result = L'\0';
		return 0;
	}
	return (ssize_t)mbrtowc(
		result,
		string.buffer,
		(size_t)string.length,
		_mbstate
	);
}

typedef struct {
	ssize_t amount;
	descent_xml_classifier_fn *type;
	struct libadt_const_lptr script;
} _descent_xml_lex_read_t;

inline bool _descent_xml_lex_read_error(_descent_xml_lex_read_t read)
{
	return read.amount < 0
		|| read.type == descent_xml_classifier_unexpected;
}

inline _descent_xml_lex_read_t _descent_xml_lex_read(
	struct libadt_const_lptr script,
	descent_xml_classifier_fn *const previous
)
{
	wchar_t c = 0;
	mbstate_t mbs = { 0 };
	_descent_xml_lex_read_t result = { 0 };
	result.amount = _descent_xml_lex_mbrtowc(&c, script, &mbs);
	if (_descent_xml_lex_read_error(result))
		result.type = (descent_xml_classifier_fn*)descent_xml_classifier_unexpected;
	else
		result.type = (descent_xml_classifier_fn*)previous(c);

	result.script = libadt_const_lptr_index(script, (ssize_t)result.amount);
	return result;
}

descent_xml_classifier_void_fn *descent_xml_lex_doctype(wchar_t input);
descent_xml_classifier_void_fn *descent_xml_lex_xmldecl(wchar_t input);
descent_xml_classifier_void_fn *descent_xml_lex_cdata(wchar_t input);
descent_xml_classifier_void_fn *descent_xml_lex_comment(wchar_t input);

/**
 * \brief Initializes a token object for use in descent_xml_lex_next().
 *
 * \param script The script to create a token from.
 *
 * \returns A token, valid for passing to descent_xml_lex_next().
 */
inline struct descent_xml_lex descent_xml_lex_init(
	struct libadt_const_lptr script
)
{
	return (struct descent_xml_lex) {
		.type = (descent_xml_classifier_fn*)descent_xml_classifier_start,
		.script = script,
		.value = libadt_const_lptr_truncate(script, 0),
	};
}

inline bool _descent_xml_lex_startswith(
	struct libadt_const_lptr string,
	struct libadt_const_lptr start
)
{
	if (string.size != start.size)
		return false;
	if (string.length < start.length)
		return false;

	return libadt_const_lptr_equal(
		libadt_const_lptr_truncate(string, (size_t)start.length),
		start
	);
}

inline ssize_t _descent_xml_lex_count_spaces(
	struct libadt_const_lptr next
)
{
	ssize_t spaces = 0;
	wchar_t c = 0;
	mbstate_t mbstate = { 0 };
	for (
		ssize_t current = _descent_xml_lex_mbrtowc(&c, next, &mbstate);
		iswspace((wint_t)c);
		next = libadt_const_lptr_index(next, current),
		current = _descent_xml_lex_mbrtowc(&c, next, &mbstate)
	) {
		const bool unexpected = c == L'\0'
			|| current < 0;
		if (unexpected)
			return -1;

		spaces += current;
	}

	return spaces;
}

inline struct libadt_const_lptr _descent_xml_lex_remainder(
	struct descent_xml_lex token
)
{
	return libadt_const_lptr_after(token.script, token.value);
}

typedef struct descent_xml_lex _descent_xml_lex_section(struct descent_xml_lex);

inline struct descent_xml_lex descent_xml_lex_then(
	struct descent_xml_lex token,
	_descent_xml_lex_section *section
)
{
	if (
		token.type == descent_xml_classifier_unexpected
		|| token.type == descent_xml_classifier_eof
	)
		return token;

	struct descent_xml_lex result = section(token);

	if (result.type == descent_xml_classifier_unexpected) {
		token.type = result.type;
		return token;
	}
	return result;
}

inline struct descent_xml_lex descent_xml_lex_or(
	struct descent_xml_lex token,
	_descent_xml_lex_section *left,
	_descent_xml_lex_section *right
)
{
	struct descent_xml_lex result = descent_xml_lex_then(token, left);
	if (result.type == descent_xml_classifier_unexpected)
		result = descent_xml_lex_then(token, right);
	return result;
}

inline struct descent_xml_lex descent_xml_lex_optional(
	struct descent_xml_lex token,
	_descent_xml_lex_section *section
)
{
	struct descent_xml_lex result
		= descent_xml_lex_then(token, section);
	if (result.type == descent_xml_classifier_unexpected)
		return token;
	return result;
}

inline struct descent_xml_lex _descent_xml_lex_space(
	struct descent_xml_lex token
)
{
	struct libadt_const_lptr remainder
		= _descent_xml_lex_remainder(token);
	ssize_t spaces = _descent_xml_lex_count_spaces(remainder);
	if (spaces <= 0) {
		token.type = descent_xml_classifier_unexpected;
		return token;
	}
	token.value.length += spaces;
	return token;
}

inline struct descent_xml_lex _descent_xml_lex_name(
	struct descent_xml_lex token
)
{
	struct libadt_const_lptr remainder
		= _descent_xml_lex_remainder(token);
	_descent_xml_lex_read_t read
		= _descent_xml_lex_read(remainder, descent_xml_classifier_element);

	if (read.type == descent_xml_classifier_unexpected) {
		token.type = read.type;
		return token;
	}

	ssize_t total = 0;
	while (read.type == descent_xml_classifier_element_name) {
		if (_descent_xml_lex_read_error(read)) {
			token.type = descent_xml_classifier_unexpected;
			return token;
		}

		total += read.amount;
		read = _descent_xml_lex_read(read.script, read.type);
	}

	token.value.length += total;
	return token;
}

inline struct descent_xml_lex _descent_xml_lex_assign(
	struct descent_xml_lex token
)
{
	struct libadt_const_lptr remainder
		= _descent_xml_lex_remainder(token);
	// TODO: do this properly
	if (*(char*)remainder.buffer != '=') {
		token.type = descent_xml_classifier_unexpected;
	} else {
		token.value.length++;
	}
	return token;
}

inline struct descent_xml_lex _descent_xml_lex_quote_string(
	struct descent_xml_lex token
)
{
	struct libadt_const_lptr remainder
		= _descent_xml_lex_remainder(token);
	_descent_xml_lex_read_t read
		= _descent_xml_lex_read(
			remainder,
			descent_xml_classifier_attribute_assign
		);

	// these names are too fucking long
	const bool quote
		= read.type == descent_xml_classifier_attribute_value_single_quote_start
		|| read.type == descent_xml_classifier_attribute_value_double_quote_start;
	if (_descent_xml_lex_read_error(read) || !quote) {
		token.type = descent_xml_classifier_unexpected;
		return token;
	}

	ssize_t total = 0;

	bool end_quote
		= read.type == descent_xml_classifier_attribute_value_single_quote_end
		|| read.type == descent_xml_classifier_attribute_value_double_quote_end;
	bool error
		= read.type == descent_xml_classifier_unexpected
		|| read.type == descent_xml_classifier_eof;
	while (!end_quote) {
		if (_descent_xml_lex_read_error(read) || error) {
			token.type = descent_xml_classifier_unexpected;
			return token;
		}

		total += read.amount;

		read = _descent_xml_lex_read(read.script, read.type);
		end_quote
			= read.type == descent_xml_classifier_attribute_value_single_quote_end
			|| read.type == descent_xml_classifier_attribute_value_double_quote_end;
		error
			= read.type == descent_xml_classifier_unexpected
			|| read.type == descent_xml_classifier_eof;
	}

	total++;

	token.value.length += total;
	return token;
}

inline struct descent_xml_lex _descent_xml_lex_doctype_str(
	struct descent_xml_lex token
)
{
	struct libadt_const_lptr remainder
		= _descent_xml_lex_remainder(token);
	const struct libadt_const_lptr
		doctypedecl = libadt_str_literal("!DOCTYPE");

	if (!_descent_xml_lex_startswith(remainder, doctypedecl)) {
		token.type = descent_xml_classifier_unexpected;
		return token;
	}

	token.value.length += doctypedecl.length;
	return token;
}

inline struct descent_xml_lex _descent_xml_lex_xmldecl_str(
	struct descent_xml_lex token
)
{
	struct libadt_const_lptr remainder
		= _descent_xml_lex_remainder(token);
	const struct libadt_const_lptr
		doctypedecl = libadt_str_literal("?xml");

	if (!_descent_xml_lex_startswith(remainder, doctypedecl)) {
		token.type = descent_xml_classifier_unexpected;
		return token;
	}

	token.value.length += doctypedecl.length;
	return token;
}


inline struct descent_xml_lex _descent_xml_lex_doctype_system(
	struct descent_xml_lex token
)
{
	struct libadt_const_lptr remainder
		= _descent_xml_lex_remainder(token);
	const struct libadt_const_lptr
		systemid = libadt_str_literal("SYSTEM");

	if (!_descent_xml_lex_startswith(remainder, systemid)) {
		token.type = descent_xml_classifier_unexpected;
		return token;
	}

	token.value.length += systemid.length;
	token = descent_xml_lex_then(token, _descent_xml_lex_space);
	token = descent_xml_lex_then(token, _descent_xml_lex_quote_string);
	return token;
}

inline struct descent_xml_lex _descent_xml_lex_doctype_public(
	struct descent_xml_lex token
)
{
	struct libadt_const_lptr remainder
		= _descent_xml_lex_remainder(token);
	const struct libadt_const_lptr
		publicid = libadt_str_literal("PUBLIC");

	if (!_descent_xml_lex_startswith(remainder, publicid)) {
		token.type = descent_xml_classifier_unexpected;
		return token;
	}

	token.value.length += publicid.length;
	token = descent_xml_lex_then(token, _descent_xml_lex_space);
	token = descent_xml_lex_then(token, _descent_xml_lex_quote_string);
	token = descent_xml_lex_then(token, _descent_xml_lex_space);
	token = descent_xml_lex_then(token, _descent_xml_lex_quote_string);
	return token;
}

inline struct descent_xml_lex _descent_xml_lex_doctype_extrawurst(
	struct descent_xml_lex token
)
{
	token = descent_xml_lex_then(
		token,
		_descent_xml_lex_space
	);
	if (token.type == descent_xml_classifier_unexpected)
		return token;

	token = descent_xml_lex_or(
		token,
		_descent_xml_lex_doctype_system,
		_descent_xml_lex_doctype_public
	);
	return token;
}

inline struct descent_xml_lex descent_xml_lex_handle_doctype(
	struct descent_xml_lex token
)
{
	token = descent_xml_lex_then(token, _descent_xml_lex_doctype_str);
	token = descent_xml_lex_then(token, _descent_xml_lex_space);
	token = descent_xml_lex_then(token, _descent_xml_lex_name);
	token = descent_xml_lex_optional(
		token,
		_descent_xml_lex_doctype_extrawurst
	);
	token = descent_xml_lex_optional(
		token,
		_descent_xml_lex_space
	);

	if (token.type == descent_xml_classifier_unexpected)
		return token;

	token.value = libadt_const_lptr_index(token.value, 1);
	token.type = descent_xml_lex_doctype;
	return token;
}

inline struct descent_xml_lex _descent_xml_lex_attribute_value(
	struct descent_xml_lex token
)
{
	token = descent_xml_lex_then(token, _descent_xml_lex_space);
	token = descent_xml_lex_then(token, _descent_xml_lex_name);
	token = descent_xml_lex_optional(token, _descent_xml_lex_space);
	token = descent_xml_lex_then(token, _descent_xml_lex_assign);
	token = descent_xml_lex_optional(token, _descent_xml_lex_space);
	token = descent_xml_lex_then(token, _descent_xml_lex_quote_string);
	return token;
}

inline struct descent_xml_lex descent_xml_lex_handle_xmldecl(
	struct descent_xml_lex token
)
{
	token = descent_xml_lex_then(token, _descent_xml_lex_xmldecl_str);
	token = descent_xml_lex_then(token, _descent_xml_lex_attribute_value);

	struct descent_xml_lex next = token;
	while (
		(next = descent_xml_lex_then(
			next,
			_descent_xml_lex_attribute_value
		)).type != descent_xml_classifier_unexpected
	) {
		token = next;
	}
	token = descent_xml_lex_optional(token, _descent_xml_lex_space);

	if (token.type == descent_xml_classifier_unexpected)
		return token;

	struct libadt_const_lptr remainder = _descent_xml_lex_remainder(token);
	// TODO: do this properly sometime
	if (*(char*)remainder.buffer != '?') {
		token.type = descent_xml_classifier_unexpected;
		return token;
	} else {
		token.value.length++;
	}

	token.value = libadt_const_lptr_index(token.value, 1);
	token.type = descent_xml_lex_xmldecl;
	return token;
}

inline struct descent_xml_lex _descent_xml_lex_handle_prolog(
	struct descent_xml_lex token
)
{
	return descent_xml_lex_or(
		token,
		descent_xml_lex_handle_xmldecl,
		descent_xml_lex_handle_doctype
	);
}

inline struct descent_xml_lex descent_xml_lex_handle_cdata(
	struct descent_xml_lex token
)
{
	struct libadt_const_lptr remainder
		= _descent_xml_lex_remainder(token);
	const struct libadt_const_lptr cdata
		= libadt_str_literal("![CDATA[");
	ssize_t total = 0;
	if (!_descent_xml_lex_startswith(remainder, cdata)) {
		token.type = descent_xml_classifier_unexpected;
		return token;
	}

	total += cdata.length;
	remainder = libadt_const_lptr_index(remainder, cdata.length);

	const struct libadt_const_lptr cdata_end
		= libadt_str_literal("]]");

	while (!_descent_xml_lex_startswith(remainder, cdata_end)) {
		if (remainder.length <= 0) {
			token.type = descent_xml_classifier_unexpected;
			return token;
		}
		total++;
		remainder = libadt_const_lptr_index(remainder, 1);
	}

	total += cdata_end.length;
	token.type = descent_xml_lex_cdata;
	token.value = libadt_const_lptr_index(token.value, 1);
	token.value.length += total;
	return token;
}

inline struct descent_xml_lex descent_xml_lex_handle_comment(
	struct descent_xml_lex token
)
{
	struct libadt_const_lptr remainder
		= _descent_xml_lex_remainder(token);
	const struct libadt_const_lptr comment
		= libadt_str_literal("!--");
	ssize_t total = 0;
	if (!_descent_xml_lex_startswith(remainder, comment)) {
		token.type = descent_xml_classifier_unexpected;
		return token;
	}

	total += comment.length;
	remainder = libadt_const_lptr_index(remainder, comment.length);

	const struct libadt_const_lptr comment_end
		= libadt_str_literal("--");

	while (!_descent_xml_lex_startswith(remainder, comment_end)) {
		if (remainder.length <= 0) {
			token.type = descent_xml_classifier_unexpected;
			return token;
		}
		total++;
		remainder = libadt_const_lptr_index(remainder, 1);
	}

	total += comment_end.length;
	token.type = descent_xml_lex_comment;
	token.value = libadt_const_lptr_index(token.value, 1);
	token.value.length += total;
	return token;
}

inline struct descent_xml_lex _descent_xml_lex_handle_unmarkdown(
	struct descent_xml_lex token
)
{
	return descent_xml_lex_or(
		token,
		descent_xml_lex_handle_comment,
		descent_xml_lex_handle_cdata
	);
}

/**
 * \brief Returns the next, raw token in the script referred to by
 * 	previous.
 *
 * \param previous The previous token from the script.
 *
 * \returns The next token.
 */
inline struct descent_xml_lex descent_xml_lex_next_raw(
	struct descent_xml_lex token
)
{
	struct libadt_const_lptr next = _descent_xml_lex_remainder(token);

	if (token.type == descent_xml_classifier_element) {
		// all this bizarre XML syntax pisses me off so
		// I'm just beating it into submission
		struct descent_xml_lex test = descent_xml_lex_or(
			token,
			_descent_xml_lex_handle_prolog,
			_descent_xml_lex_handle_unmarkdown
		);
		if (test.type != descent_xml_classifier_unexpected)
			return test;
	}

	_descent_xml_lex_read_t
		read = _descent_xml_lex_read(next, token.type),
		previous_read = read;

	if (_descent_xml_lex_read_error(read))
		return (struct descent_xml_lex) {
			.script = token.script,
			.type = descent_xml_classifier_unexpected,
			.value = libadt_const_lptr_truncate(next, 0),
		};

	if (read.type == descent_xml_classifier_eof) {
		return (struct descent_xml_lex) {
			.script = token.script,
			.type = read.type,
			.value = libadt_const_lptr_truncate(next, (size_t)read.amount)
		};
	}

	ssize_t value_length = read.amount;
	for (
		read = _descent_xml_lex_read(read.script, read.type);
		!_descent_xml_lex_read_error(read);
		read = _descent_xml_lex_read(read.script, read.type)
	) {
		if (read.type != previous_read.type)
			break;

		previous_read = read;
		value_length += read.amount;
	}

	return (struct descent_xml_lex) {
		.script = token.script,
		.type = previous_read.type,
		.value = libadt_const_lptr_truncate(next, (size_t)value_length),
	};
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DESCENT_XML_LEX
