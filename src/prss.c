/* $Id$
 *
 * Copyright (c) 2007 Mikko Sysikaski <mikko.sysikaski@gmail.com>
 *					  Toni Spets <toni.spets@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include "prss.h"
#include "conky.h"
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifndef PARSE_OPTIONS
#define PARSE_OPTIONS 0
#endif

PRSS *prss_parse_doc(xmlDocPtr doc);

PRSS *prss_parse_data(const char *xml_data)
{
	xmlDocPtr doc = xmlReadMemory(xml_data, strlen(xml_data), "", NULL,
		PARSE_OPTIONS);

	if (!doc) {
		return NULL;
	}

	return prss_parse_doc(doc);
}

PRSS *prss_parse_file(const char *xml_file)
{
	xmlDocPtr doc = xmlReadFile(xml_file, NULL, PARSE_OPTIONS);

	if (!doc) {
		return NULL;
	}

	return prss_parse_doc(doc);
}

void prss_free(PRSS *data)
{
	if (!data) {
		return;
	}
	xmlFreeDoc(data->_data);
	free(data->version);
	free(data->items);
	free(data);
}

static inline void prss_null(PRSS *p)
{
	memset(p, 0, sizeof(PRSS));
}
static inline void prss_null_item(PRSS_Item *i)
{
	memset(i, 0, sizeof(PRSS_Item));
}

static inline void read_item(PRSS_Item *res, xmlNodePtr data)
{
	prss_null_item(res);

	res->title = res->link = res->description = NULL;
	for (; data; data = data->next) {
		xmlNodePtr child;
		const char *name;

		if (data->type != XML_ELEMENT_NODE) {
			continue;
		}
		child = data->children;

		if (!child) {
			continue;
		}

		name = (const char *)data->name;
		if (!strcasecmp(name, "title")) {
			res->title = (char *) child->content;
		} else if (!strcasecmp(name, "link")) {
			res->link = (char *) child->content;
		} else if (!strcasecmp(name, "description")) {
			res->description = (char *) child->content;
		} else if (!strcasecmp(name, "category")) {
			res->category = (char *) child->content;
		} else if (!strcasecmp(name, "pubDate")) {
			res->pubdate = (char *) child->content;
		} else if (!strcasecmp(name, "guid")) {
			res->guid = (char *) child->content;
		}
	}
}
static inline void read_element(PRSS *res, xmlNodePtr n)
{
	xmlNodePtr child;
	const char *name;

	if (n->type != XML_ELEMENT_NODE) {
		return;
	}
	child = n->children;

	if (!child) {
		return;
	}

	name = (const char *)n->name;
	if (!strcasecmp(name, "title")) {
		res->title = (char *) child->content;
	} else if (!strcasecmp(name, "link")) {
		res->link = (char *) child->content;
	} else if (!strcasecmp(name, "description")) {
		res->description = (char *) child->content;
	} else if (!strcasecmp(name, "language")) {
		res->language = (char *) child->content;
	} else if (!strcasecmp(name, "pubDate")) {
		res->pubdate = (char *) child->content;
	} else if (!strcasecmp(name, "lastBuildDate")) {
		res->lastbuilddate = (char *) child->content;
	} else if (!strcasecmp(name, "generator")) {
		res->generator = (char *) child->content;
	} else if (!strcasecmp(name, "docs")) {
		res->docs = (char *) child->content;
	} else if (!strcasecmp(name, "managingEditor")) {
		res->managingeditor = (char *) child->content;
	} else if (!strcasecmp(name, "webMaster")) {
		res->webmaster = (char *) child->content;
	} else if (!strcasecmp(name, "copyright")) {
		res->copyright = (char *) child->content;
	} else if (!strcasecmp(name, "ttl")) {
		res->ttl = (char *) child->content;
	} else if (!strcasecmp(name, "item")) {
		read_item(&res->items[res->item_count++], n->children);
	}
}

static inline int parse_rss_2_0(PRSS *res, xmlNodePtr root)
{
	xmlNodePtr channel = root->children;
	xmlNodePtr n;
	int items = 0;

	while (channel && (channel->type != XML_ELEMENT_NODE
			|| strcmp((const char *) channel->name, "channel"))) {
		channel = channel->next;
	}
	if (!channel) {
		return 0;
	}

	for (n = channel->children; n; n = n->next) {
		if (n->type == XML_ELEMENT_NODE &&
				!strcmp((const char *) n->name, "item")) {
			++items;
		}
	}

	res->version = strndup("2.0", text_buffer_size);
	res->items = malloc(items * sizeof(PRSS_Item));
	res->item_count = 0;

	for (n = channel->children; n; n = n->next) {
		read_element(res, n);
	}

	return 1;
}
static inline int parse_rss_1_0(PRSS *res, xmlNodePtr root)
{
	int items = 0;
	xmlNodePtr n;

	for (n = root->children; n; n = n->next) {
		if (n->type == XML_ELEMENT_NODE) {
			if (!strcmp((const char *) n->name, "item")) {
				++items;
			} else if (!strcmp((const char *) n->name, "channel")) {
				xmlNodePtr i;

				for (i = n->children; i; i = i->next) {
					read_element(res, i);
				}
			}
		}
	}

	res->version = strndup("1.0", text_buffer_size);
	res->items = malloc(items * sizeof(PRSS_Item));
	res->item_count = 0;

	for (n = root->children; n; n = n->next) {
		if (n->type == XML_ELEMENT_NODE &&
				!strcmp((const char *) n->name, "item")) {
			read_item(&res->items[res->item_count++], n->children);
		}
	}

	return 1;
}
static inline int parse_rss_0_9x(PRSS *res, xmlNodePtr root)
{
	// almost same...
	return parse_rss_2_0(res, root);
}

PRSS *prss_parse_doc(xmlDocPtr doc)
{
	/* FIXME: doc shouldn't be freed after failure when called explicitly from
	 * program! */

	xmlNodePtr root = xmlDocGetRootElement(doc);
	PRSS *result = malloc(sizeof(PRSS));

	prss_null(result);
	result->_data = doc;
	do {
		if (root->type == XML_ELEMENT_NODE) {
			if (!strcmp((const char *) root->name, "RDF")) {
				// RSS 1.0 document
				if (!parse_rss_1_0(result, root)) {
					free(result);
					xmlFreeDoc(doc);
					return NULL;
				}
				return result;
			} else if (!strcmp((const char *) root->name, "rss")) {
				// RSS 2.0 or <1.0 document
				if (!parse_rss_2_0(result, root)) {
					free(result);
					xmlFreeDoc(doc);
					return NULL;
				}
				return result;
			}
		}
		root = root->next;
	} while (root);
	free(result);
	return NULL;
}
