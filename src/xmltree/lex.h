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

#ifndef XMLTREE_LEX
#define XMLTREE_LEX

#ifdef __cplusplus
extern "C" {
#endif

#include "init.h"

#include <wchar.h>

#include <libadt/lptr.h>

#include "classifier.h"

/**
 * \file
 */

/**
 * \brief Represents a single token.
 */
struct xmltree_lex {
	/**
	 * \brief Represents the type of token classifiered.
	 */
	xmltree_classifier_fn *type;

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

inline size_t _xmltree_mbrtowc(
	wchar_t *result,
	struct libadt_const_lptr string,
	mbstate_t *_mbstate
)
{
	if (string.length <= 0) {
		// when does this break?
		*result = (wchar_t)WEOF;
		return 0;
	}
	return mbrtowc(
		result,
		string.buffer,
		(size_t)string.length,
		_mbstate
	);
}

typedef struct {
	size_t amount;
	xmltree_classifier_fn *type;
	struct libadt_const_lptr script;
} _xmltree_read_t;

inline bool _xmltree_read_error(_xmltree_read_t read)
{
	return read.amount == (size_t)-1
		|| read.amount == (size_t)-2
		|| read.type == xmltree_classifier_unexpected;
}

inline _xmltree_read_t _xmltree_read(
	struct libadt_const_lptr script,
	xmltree_classifier_fn *const previous
)
{
	wchar_t c = 0;
	mbstate_t mbs = { 0 };
	_xmltree_read_t result = { 0 };
	result.amount = _xmltree_mbrtowc(&c, script, &mbs);
	if (_xmltree_read_error(result))
		result.type = (xmltree_classifier_fn*)xmltree_classifier_unexpected;
	else
		result.type = (xmltree_classifier_fn*)previous(c);

	result.script = libadt_const_lptr_index(script, (ssize_t)result.amount);
	return result;
}

/**
 * \brief Initializes a token object for use in xmltree_lex_next().
 *
 * \param script The script to create a token from.
 *
 * \returns A token, valid for passing to xmltree_lex_next().
 */
XMLTREE_EXPORT inline struct xmltree_lex xmltree_lex_init(
	struct libadt_const_lptr script
)
{
	return (struct xmltree_lex) {
		.type = (xmltree_classifier_fn*)xmltree_classifier_start,
		.script = script,
		.value = libadt_const_lptr_truncate(script, 0),
	};
}

/**
 * \brief Returns the next, raw token in the script referred to by
 * 	previous.
 */
XMLTREE_EXPORT inline struct xmltree_lex xmltree_lex_next_raw(
	struct xmltree_lex previous
)
{
	const ssize_t value_offset = (char *)previous.value.buffer
		- (char *)previous.script.buffer;
	struct libadt_const_lptr next = libadt_const_lptr_index(
		previous.script,
		value_offset + previous.value.length
	);

	_xmltree_read_t
		read = _xmltree_read(next, previous.type),
		previous_read = read;

	if (_xmltree_read_error(read))
		return (struct xmltree_lex) {
			.script = previous.script,
			.type = xmltree_classifier_unexpected,
			.value = libadt_const_lptr_truncate(next, 0),
		};

	if (read.type == xmltree_classifier_eof) {
		return (struct xmltree_lex) {
			.script = previous.script,
			.type = read.type,
			.value = libadt_const_lptr_truncate(next, read.amount)
		};
	}

	size_t value_length = read.amount;
	for (
		read = _xmltree_read(read.script, read.type);
		!_xmltree_read_error(read);
		read = _xmltree_read(read.script, read.type)
	) {
		if (read.type != previous_read.type)
			break;

		previous_read = read;
		value_length += read.amount;
	}

	return (struct xmltree_lex) {
		.script = previous.script,
		.type = previous_read.type,
		.value = libadt_const_lptr_truncate(next, value_length),
	};
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XMLTREE_LEX
