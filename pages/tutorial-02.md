\page tutorial-02 Second Tutorial: A Simple Printer

The previous program worked, but wasn't very interesting. In this tutorial, we're going to use the C-string interface to write a parser that prints what it encounters.

This involves using \ref descent_xml_parse_cstr, passing it an `element_handler` and a `text_handler`.

The signature of an element handler function is given in \ref descent_xml_parse_element_cstr_fn, and the signature of a text handler is given in \ref descent_xml_parse_text_cstr_fn. We're going to write an element handler that prints the element name and its attributes, and a text handler that prints the text content encountered.

Save the following file as `print-example.c` and compile it with `cc -o print-example print-example.c -ldescent_xmlstatic`.

\include{lineno} print-example.c

Running this program will produce the following output:

```
$ ./print-example
text node:

element_name: element
attribute: attr=val
attribute: attr2=
empty element (ends with /> or ?>): false
text node:
        Hello, world!

text node: Hello, CDATA!
text node:
        Hello, after CDATA!
```

This example reveals a few things:
- \ref descent_xml_parse_cstr doesn't do intelligent parsing: in XML, spaces between elements _are_ Text nodes containing whitespace, and this is reflected in Descent XML. Similarly, Descent XML doesn't convert entities for you, and it doesn't trim whitespace around Text nodes that also include non-whitespace content.
  - `![CDATA[]]` sections are passed separately, even when embedded in text nodes.
- The `attributes` array will always be a multiple of two, for each attribute=value pair, plus a null terminator. An attribute with an empty value will have an empty-string value.
- Descent XML does not provide a callback for closing tags, and doesn't call the element handler for closing tags. The next tutorial will cover checking for correctly-nested elements and closing tags.

You may have noticed that we call descent_xml_validate_document() before running our custom parser. The parser interface itself doesn't do nested-structure validation, though the lexer does perform some validation that certain characters do not appear in certain contexts.

Proceed to the next tutorial, \ref tutorial-03.
